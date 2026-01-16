#pragma once

#include <iostream>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <set>

// installed
#include <librealsense2/rs.hpp>

// local
#include <syncorder/gonfig/gonfig.h>
#include <syncorder/devices/common/verifier_base.h>


/**
 * @struct RealsenseVideoResult
 * Verification result for a single video
 */
struct RealsenseVideoResult {
    std::string video_name;
    bool valid{false};
    double duration{0.0};
    int total_frames{0};
    int expected_frames{0};
    int capturing_success_frames{0};
    bool bag_valid{false};
};


/**
 * @class Realsense Verifier
 */

class RealsenseVerifier : public BVerifier {
private:
    std::string output_path_;
    std::vector<RealsenseVideoResult> video_results_;

public:
    RealsenseVerifier() {
        output_path_ = gonfig.output_path;
    }

    ~RealsenseVerifier() = default;

public:
    bool verify() override {
        std::cout << "[Realsense] Starting verification\n";

        result_ = true;
        video_results_.clear();

        // Collect all sessions with their timing data and file paths
        struct SessionData {
            std::string session_name;
            std::string session_path;
            FrameTimingData timing;
            std::string csv_path;
            std::string realsense_path;
        };
        std::vector<SessionData> sessions;

        // Scan for session directories
        for (const auto& entry : std::filesystem::directory_iterator(output_path_)) {
            if (!entry.is_directory()) continue;

            std::string dir_name = entry.path().filename().string();
            if (dir_name.find("session_") != 0) continue;

            SessionData session;
            session.session_name = dir_name;
            session.session_path = entry.path().generic_string();

            // Parse frame_timing.log from this session
            std::string timing_path = session.session_path + "/frame_timing.log";
            session.timing = _parseFrameTiming(timing_path);

            // Find Realsense CSV and directory in this session
            std::string realsense_path = session.session_path + "/realsense";
            if (std::filesystem::exists(realsense_path)) {
                session.realsense_path = realsense_path;
                for (const auto& file : std::filesystem::directory_iterator(realsense_path)) {
                    if (file.is_regular_file() && file.path().extension().string() == ".csv") {
                        session.csv_path = file.path().generic_string();
                        break;
                    }
                }
            }

            if (session.timing.valid && !session.csv_path.empty()) {
                sessions.push_back(session);
                std::cout << "[Realsense] Found session: " << dir_name << " with "
                          << session.timing.videos.size() << " video(s)\n";
            }
        }

        if (sessions.empty()) {
            std::cout << "[Realsense] Error: No valid sessions found\n";
            result_ = false;
            return result_;
        }

        // Sort sessions by name (chronological order)
        std::sort(sessions.begin(), sessions.end(),
                  [](const SessionData& a, const SessionData& b) {
                      return a.session_name < b.session_name;
                  });

        // Build a map of video_index -> latest video data and session info
        std::map<int, VideoSessionInfo> latest_videos;

        for (const auto& session : sessions) {
            for (const auto& video : session.timing.videos) {
                // Always update with the later session (overwrites previous)
                VideoSessionInfo info;
                info.video = video;
                info.csv_path = session.csv_path;
                info.realsense_path = session.realsense_path;
                latest_videos[video.video_index] = info;
                std::cout << "[Realsense] Video " << video.video_index
                          << " from session " << session.session_name << "\n";
            }
        }

        std::cout << "[Realsense] Using " << latest_videos.size()
                  << " video(s) from latest recordings\n";

        // Verify each video with its corresponding CSV file
        bool csv_result = _verifyCsvsByVideoIndividually(latest_videos);

        // Verify BAG files for each video
        _verifyBagFilesIndividually(latest_videos);

        result_ = csv_result;
        _writeResult();

        std::cout << "[Realsense] Verify phase " << (result_ ? "completed" : "failed") << "\n";

        return result_;
    }

private:
    struct VideoSessionInfo {
        VideoTimingData video;
        std::string csv_path;
        std::string realsense_path;
    };

    bool _verifyCsvsByVideoIndividually(const std::map<int, VideoSessionInfo>& video_sessions) {
        bool all_valid = true;

        for (const auto& [video_index, info] : video_sessions) {
            RealsenseVideoResult result;
            result.video_name = info.video.getVideoName();
            result.duration = info.video.getDuration();
            result.expected_frames = (int)(result.duration * 60); // 60fps

            std::cout << "\n[Realsense] Processing " << result.video_name
                      << " from CSV: " << info.csv_path << "\n";

            // Process CSV file for this specific video
            if (!_processCsvFileForVideo(info.csv_path, info.video, result)) {
                std::cout << "[Realsense] Failed to process CSV for " << result.video_name << "\n";
                result.valid = false;
                all_valid = false;
            } else {
                std::cout << "  Duration: " << result.duration << "s\n";
                std::cout << "  Total frames: " << result.total_frames << "\n";
                std::cout << "  Expected frames: " << result.expected_frames << "\n";
                std::cout << "  Capturing success frames: " << result.capturing_success_frames << "\n";

                // Validation: check if total rows match expected frames (with tolerance)
                if (result.total_frames < result.expected_frames * 0.95) {
                    std::cout << "  CSV Status: FAILED (insufficient frames)\n";
                    result.valid = false;
                    all_valid = false;
                } else if (result.total_frames > result.expected_frames * 1.1) {
                    std::cout << "  CSV Status: WARNING (too many frames)\n";
                    result.valid = true; // Still valid but with warning
                } else {
                    std::cout << "  CSV Status: PASSED\n";
                    result.valid = true;
                }
            }

            video_results_.push_back(result);
        }

        return all_valid;
    }

    bool _processCsvFileForVideo(const std::string& csv_path, const VideoTimingData& video,
                                  RealsenseVideoResult& result) {
        if (!std::filesystem::exists(csv_path)) {
            std::cout << "[Realsense] CSV file does not exist: " << csv_path << "\n";
            return false;
        }

        try {
            std::ifstream file(csv_path);
            std::string line;

            // Read and verify header
            if (!std::getline(file, line)) {
                std::cout << "[Realsense] Could not read header\n";
                return false;
            }

            if (line.empty() || line.find("index") == std::string::npos) {
                std::cout << "[Realsense] Invalid CSV header format\n";
                return false;
            }

            // Parse CSV rows for this specific video based on timestamp
            while (std::getline(file, line)) {
                if (line.empty()) continue;

                // Parse timestamp from CSV (format: index,color_timestamp,...)
                std::istringstream iss(line);
                std::string index_str, timestamp_str;
                std::getline(iss, index_str, ',');
                std::getline(iss, timestamp_str, ',');

                double frame_timestamp = 0.0;
                try {
                    frame_timestamp = std::stod(timestamp_str);
                } catch (...) {
                    continue; // Skip invalid rows
                }

                // Convert timestamp from milliseconds to seconds
                double frame_time_sec = frame_timestamp / 1000.0;

                // Check if this frame belongs to this specific video
                if (frame_time_sec >= video.start_time && frame_time_sec <= video.end_time) {
                    result.total_frames++;
                    result.capturing_success_frames++;
                }
            }

            return true;

        } catch (const std::exception& e) {
            std::cout << "[Realsense] CSV processing failed: " << e.what() << "\n";
            return false;
        }
    }

    void _verifyBagFilesIndividually(const std::map<int, VideoSessionInfo>& video_sessions) {
        for (auto& result : video_results_) {
            // Extract video_index from video_name (e.g., "VIDEO_INDEX_1" -> 1)
            std::string video_name = result.video_name;
            size_t underscore_pos = video_name.rfind('_');
            if (underscore_pos == std::string::npos) {
                std::cout << "[Realsense] Invalid video name format: " << video_name << "\n";
                result.bag_valid = false;
                continue;
            }

            int video_index = std::stoi(video_name.substr(underscore_pos + 1));
            auto it = video_sessions.find(video_index);
            if (it == video_sessions.end()) {
                std::cout << "[Realsense] Video session info not found for " << video_name << "\n";
                result.bag_valid = false;
                continue;
            }

            // Find any .bag file in the realsense directory
            // BAG files are named with timestamps (e.g., 1760941214.bag)
            std::string bag_path;
            bool bag_found = false;

            if (std::filesystem::exists(it->second.realsense_path)) {
                for (const auto& entry : std::filesystem::directory_iterator(it->second.realsense_path)) {
                    if (entry.is_regular_file() && entry.path().extension().string() == ".bag") {
                        bag_path = entry.path().generic_string();
                        bag_found = true;
                        break;
                    }
                }
            }

            if (bag_found) {
                result.bag_valid = _verifyBag(bag_path);
            } else {
                std::cout << "[Realsense] BAG file not found in: " << it->second.realsense_path << "\n";
                result.bag_valid = false;
            }

            // Overall validity requires both CSV and BAG to be valid
            result.valid = result.valid && result.bag_valid;
        }
    }

    bool _verifyCsvsByVideo(const std::vector<std::string>& csv_paths, const FrameTimingData& timing) {
        // Initialize result for each video
        std::map<int, RealsenseVideoResult> video_results_map;
        for (const auto& video : timing.videos) {
            RealsenseVideoResult result;
            result.video_name = video.getVideoName();
            result.duration = video.getDuration();
            result.expected_frames = (int)(result.duration * 60); // 60fps
            video_results_map[video.video_index] = result;
        }

        // Process all CSV files
        for (const auto& csv_path : csv_paths) {
            if (!_processCsvFile(csv_path, timing, video_results_map)) {
                return false;
            }
        }

        // Validate each video and store results
        bool all_valid = true;
        for (auto& [video_index, result] : video_results_map) {
            std::cout << "\n[Realsense] " << result.video_name << ":\n";
            std::cout << "  Duration: " << result.duration << "s\n";
            std::cout << "  Total frames: " << result.total_frames << "\n";
            std::cout << "  Expected frames: " << result.expected_frames << "\n";
            std::cout << "  Capturing success frames: " << result.capturing_success_frames << "\n";

            // Validation: check if total rows match expected frames (with tolerance)
            if (result.total_frames < result.expected_frames * 0.95) {
                std::cout << "  CSV Status: FAILED (insufficient frames)\n";
                result.valid = false;
                all_valid = false;
            } else if (result.total_frames > result.expected_frames * 1.1) {
                std::cout << "  CSV Status: WARNING (too many frames)\n";
                result.valid = true; // Still valid but with warning
            } else {
                std::cout << "  CSV Status: PASSED\n";
                result.valid = true;
            }

            video_results_.push_back(result);
        }

        return all_valid;
    }

    bool _processCsvFile(const std::string& csv_path, const FrameTimingData& timing,
                         std::map<int, RealsenseVideoResult>& video_results_map) {
        std::cout << "[Realsense] Processing CSV file: " << csv_path << "\n";

        if (!std::filesystem::exists(csv_path)) {
            std::cout << "[Realsense] File does not exist\n";
            return false;
        }

        try {
            std::ifstream file(csv_path);
            std::string line;

            // Read and verify header
            if (!std::getline(file, line)) {
                std::cout << "[Realsense] Could not read header\n";
                return false;
            }

            // No need to validate specific header format, just check it's not empty
            if (line.empty() || line.find("index") == std::string::npos) {
                std::cout << "[Realsense] Invalid CSV header format\n";
                return false;
            }

            // Parse CSV rows and assign to videos based on timestamp
            while (std::getline(file, line)) {
                if (line.empty()) continue;

                // Parse timestamp from CSV (format: index,color_timestamp,...)
                std::istringstream iss(line);
                std::string index_str, timestamp_str;
                std::getline(iss, index_str, ',');
                std::getline(iss, timestamp_str, ',');

                double frame_timestamp = 0.0;
                try {
                    frame_timestamp = std::stod(timestamp_str);
                } catch (...) {
                    continue; // Skip invalid rows
                }

                // Convert timestamp from milliseconds to seconds
                double frame_time_sec = frame_timestamp / 1000.0;

                // Find which video this frame belongs to
                for (const auto& video : timing.videos) {
                    if (frame_time_sec >= video.start_time && frame_time_sec <= video.end_time) {
                        auto& result = video_results_map[video.video_index];
                        result.total_frames++;
                        result.capturing_success_frames++;
                        break;
                    }
                }
            }

            return true;

        } catch (const std::exception& e) {
            std::cout << "[Realsense] CSV processing failed: " << e.what() << "\n";
            return false;
        }
    }

    void _verifyBagFiles(const std::vector<std::string>& realsense_paths, const FrameTimingData& timing) {
        // For each video, verify corresponding BAG file in all realsense paths
        for (auto& result : video_results_) {
            // BAG files should be named like VIDEO_INDEX_1.bag
            std::string bag_filename = result.video_name + ".bag";
            bool bag_found = false;

            for (const auto& realsense_path : realsense_paths) {
                std::string bag_path = realsense_path + "/" + bag_filename;
                if (std::filesystem::exists(bag_path)) {
                    result.bag_valid = _verifyBag(bag_path);
                    bag_found = true;
                    break;
                }
            }

            if (!bag_found) {
                std::cout << "[Realsense] Warning: BAG file not found for " << result.video_name << "\n";
                result.bag_valid = false;
            }

            // Overall validity requires both CSV and BAG to be valid
            result.valid = result.valid && result.bag_valid;
        }
    }

    bool _verifyBag(const std::string& bag_path) {
        std::cout << "[Realsense] Verifying BAG file: " << bag_path << "\n";

        if (!std::filesystem::exists(bag_path)) {
            std::cout << "[Realsense] File does not exist\n";
            return false;
        }

        auto file_size = std::filesystem::file_size(bag_path);
        std::cout << "[Realsense] File size: " << file_size << " bytes\n";

        if (file_size == 0) {
            std::cout << "[Realsense] File is empty\n";
            return false;
        }

        // Wait for file to stabilize (check if still being written)
        std::cout << "[Realsense] Checking file stability...\n";
        auto last_size = file_size;
        for (int i = 0; i < 10; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            auto current_size = std::filesystem::file_size(bag_path);
            if (current_size != last_size) {
                std::cout << "[Realsense] File size changing, waiting...\n";
                last_size = current_size;
            } else {
                break;
            }
            if (i == 9) {
                std::cout << "[Realsense] File still changing after 1 second\n";
                return false;
            }
        }

        // Verify BAG file by attempting to open with RealSense SDK (read-only)
        try {
            rs2::config cfg;
            cfg.enable_device_from_file(bag_path, false);  // false = read-only mode

            rs2::pipeline pipe;
            pipe.start(cfg);
            pipe.stop();

            std::cout << "[Realsense] BAG file verification successful\n";
            return true;

        } catch (const rs2::error& e) {
            std::cout << "[Realsense] RealSense error during BAG verification: " << e.what() << "\n";
            return false;
        } catch (const std::exception& e) {
            std::cout << "[Realsense] BAG verification failed: " << e.what() << "\n";
            return false;
        }
    }

    void _writeResult() {
        if (!std::filesystem::exists(gonfig.verified_path)) {
            std::filesystem::create_directories(gonfig.verified_path);
        }

        std::string csv_path = gonfig.verified_path + "realsense_verify_result.csv";
        std::ofstream csv(csv_path);

        if (!csv.is_open()) {
            std::cout << "[Realsense] Failed to create result CSV file: " << csv_path << "\n";
            return;
        }

        // Write header
        csv << "video_name,duration,total_frames,expected_frames,capturing_success_frames,bag_valid\n";

        // Write each video result
        for (const auto& video : video_results_) {
            csv << video.video_name << ","
                << std::fixed << std::setprecision(3) << video.duration << ","
                << video.total_frames << ","
                << video.expected_frames << ","
                << video.capturing_success_frames << ","
                << (video.bag_valid ? "true" : "false") << "\n";
        }

        csv.close();
        std::cout << "[Realsense] Results written to " << csv_path << "\n";
    }
};
