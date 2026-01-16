#pragma once

#include <iostream>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <algorithm>
#include <set>

// local
#include <syncorder/gonfig/gonfig.h>
#include <syncorder/devices/common/verifier_base.h>


/**
 * @struct TobiiVideoResult
 * Verification result for a single video
 */
struct TobiiVideoResult {
    std::string video_name;
    bool valid{false};
    double duration{0.0};
    int total_frames{0};
    int expected_frames{0};
    int tracking_success_frames{0};
    int tracking_failed_frames{0};
};


/**
 * @class Tobii Verifier
 */

class TobiiVerifier : public BVerifier {
private:
    std::string output_path_;
    std::vector<TobiiVideoResult> video_results_;

public:
    TobiiVerifier() {
        output_path_ = gonfig.output_path;
    }

    ~TobiiVerifier() = default;

public:
    bool verify() override {
        std::cout << "[Tobii] Starting verification\n";

        result_ = true;
        video_results_.clear();

        // Collect all sessions with their timing data and CSV paths
        struct SessionData {
            std::string session_name;
            std::string session_path;
            FrameTimingData timing;
            std::string csv_path;
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

            // Find Tobii CSV file in this session
            std::string tobii_path = session.session_path + "/tobii";
            if (std::filesystem::exists(tobii_path)) {
                for (const auto& file : std::filesystem::directory_iterator(tobii_path)) {
                    if (file.is_regular_file() && file.path().extension().string() == ".csv") {
                        session.csv_path = file.path().generic_string();
                        break;
                    }
                }
            }

            if (session.timing.valid && !session.csv_path.empty()) {
                sessions.push_back(session);
                std::cout << "[Tobii] Found session: " << dir_name << " with "
                          << session.timing.videos.size() << " video(s)\n";
            }
        }

        if (sessions.empty()) {
            std::cout << "[Tobii] Error: No valid sessions found\n";
            result_ = false;
            return result_;
        }

        // Sort sessions by name (chronological order)
        std::sort(sessions.begin(), sessions.end(),
                  [](const SessionData& a, const SessionData& b) {
                      return a.session_name < b.session_name;
                  });

        // Build a map of video_index -> latest video data and CSV path
        std::map<int, VideoSessionInfo> latest_videos;

        for (const auto& session : sessions) {
            for (const auto& video : session.timing.videos) {
                // Always update with the later session (overwrites previous)
                VideoSessionInfo info;
                info.video = video;
                info.csv_path = session.csv_path;
                latest_videos[video.video_index] = info;
                std::cout << "[Tobii] Video " << video.video_index
                          << " from session " << session.session_name << "\n";
            }
        }

        std::cout << "[Tobii] Using " << latest_videos.size()
                  << " video(s) from latest recordings\n";

        // Verify each video with its corresponding CSV file
        result_ = _verifyCsvsByVideoIndividually(latest_videos);

        _writeResult();

        std::cout << "[Tobii] Verify phase " << (result_ ? "completed" : "failed") << "\n";

        return result_;
    }

private:
    struct VideoSessionInfo {
        VideoTimingData video;
        std::string csv_path;
    };

    bool _verifyCsvsByVideoIndividually(const std::map<int, VideoSessionInfo>& video_sessions) {
        bool all_valid = true;

        for (const auto& [video_index, info] : video_sessions) {
            TobiiVideoResult result;
            result.video_name = info.video.getVideoName();
            result.duration = info.video.getDuration();
            result.expected_frames = (int)(result.duration * 60); // 60fps

            std::cout << "\n[Tobii] Processing " << result.video_name
                      << " from CSV: " << info.csv_path << "\n";

            // Process CSV file for this specific video
            if (!_processCsvFileForVideo(info.csv_path, info.video, result)) {
                std::cout << "[Tobii] Failed to process CSV for " << result.video_name << "\n";
                result.valid = false;
                all_valid = false;
            } else {
                std::cout << "  Duration: " << result.duration << "s\n";
                std::cout << "  Total frames: " << result.total_frames << "\n";
                std::cout << "  Expected frames: " << result.expected_frames << "\n";
                std::cout << "  Tracking success: " << result.tracking_success_frames << "\n";
                std::cout << "  Tracking failed: " << result.tracking_failed_frames << "\n";

                // Validation: check if total rows match expected frames (with tolerance)
                if (result.total_frames < result.expected_frames * 0.95) {
                    std::cout << "  Status: FAILED (insufficient frames)\n";
                    result.valid = false;
                    all_valid = false;
                } else if (result.total_frames > result.expected_frames * 1.1) {
                    std::cout << "  Status: WARNING (too many frames)\n";
                    result.valid = true; // Still valid but with warning
                } else {
                    std::cout << "  Status: PASSED\n";
                    result.valid = true;
                }
            }

            video_results_.push_back(result);
        }

        return all_valid;
    }

    bool _processCsvFileForVideo(const std::string& csv_path, const VideoTimingData& video,
                                  TobiiVideoResult& result) {
        if (!std::filesystem::exists(csv_path)) {
            std::cout << "[Tobii] CSV file does not exist: " << csv_path << "\n";
            return false;
        }

        try {
            std::ifstream file(csv_path);
            std::string line;

            // Read and verify header
            if (!std::getline(file, line)) {
                std::cout << "[Tobii] Could not read header\n";
                return false;
            }

            if (line.find("index,") != 0) {
                std::cout << "[Tobii] Invalid CSV header format\n";
                return false;
            }

            // Parse CSV rows for this specific video based on timestamp
            while (std::getline(file, line)) {
                if (line.empty()) continue;

                // Parse CSV row (index,timestamp,hw_ts,left_x,left_y,...,left_validity,...,right_x,right_y,...,right_validity,...)
                std::istringstream iss(line);
                std::vector<std::string> fields;
                std::string field;
                while (std::getline(iss, field, ',')) {
                    fields.push_back(field);
                }

                if (fields.size() < 20) continue; // Need at least validity fields

                double frame_timestamp = 0.0;
                try {
                    frame_timestamp = std::stod(fields[1]); // frame_timestamp column
                } catch (...) {
                    continue; // Skip invalid rows
                }

                // Convert timestamp from milliseconds to seconds
                double frame_time_sec = frame_timestamp / 1000.0;

                // Check if this frame belongs to this specific video
                if (frame_time_sec >= video.start_time && frame_time_sec <= video.end_time) {
                    result.total_frames++;

                    // Check tracking quality: both eyes must be invalid for tracking_failed
                    // CSV columns: left_gaze_validity (index 8), right_gaze_validity (index 19)
                    bool left_valid = (fields.size() > 8 && fields[8] == "1");
                    bool right_valid = (fields.size() > 19 && fields[19] == "1");

                    if (!left_valid && !right_valid) {
                        // Both eyes failed tracking
                        result.tracking_failed_frames++;
                    } else {
                        // At least one eye tracked successfully
                        result.tracking_success_frames++;
                    }
                }
            }

            return true;

        } catch (const std::exception& e) {
            std::cout << "[Tobii] CSV processing failed: " << e.what() << "\n";
            return false;
        }
    }

    bool _verifyCsvsByVideo(const std::vector<std::string>& csv_paths, const FrameTimingData& timing) {
        // Initialize result for each video
        std::map<int, TobiiVideoResult> video_results_map;
        for (const auto& video : timing.videos) {
            TobiiVideoResult result;
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
            std::cout << "\n[Tobii] " << result.video_name << ":\n";
            std::cout << "  Duration: " << result.duration << "s\n";
            std::cout << "  Total frames: " << result.total_frames << "\n";
            std::cout << "  Expected frames: " << result.expected_frames << "\n";
            std::cout << "  Tracking success: " << result.tracking_success_frames << "\n";
            std::cout << "  Tracking failed: " << result.tracking_failed_frames << "\n";

            // Validation: check if total rows match expected frames (with tolerance)
            if (result.total_frames < result.expected_frames * 0.95) {
                std::cout << "  Status: FAILED (insufficient frames)\n";
                result.valid = false;
                all_valid = false;
            } else if (result.total_frames > result.expected_frames * 1.1) {
                std::cout << "  Status: WARNING (too many frames)\n";
                result.valid = true; // Still valid but with warning
            } else {
                std::cout << "  Status: PASSED\n";
                result.valid = true;
            }

            video_results_.push_back(result);
        }

        return all_valid;
    }

    bool _processCsvFile(const std::string& csv_path, const FrameTimingData& timing,
                         std::map<int, TobiiVideoResult>& video_results_map) {
        std::cout << "[Tobii] Processing CSV file: " << csv_path << "\n";

        if (!std::filesystem::exists(csv_path)) {
            std::cout << "[Tobii] File does not exist\n";
            return false;
        }

        try {
            std::ifstream file(csv_path);
            std::string line;

            // Read and verify header
            if (!std::getline(file, line)) {
                std::cout << "[Tobii] Could not read header\n";
                return false;
            }

            if (line.find("index,") != 0) {
                std::cout << "[Tobii] Invalid CSV header format\n";
                return false;
            }

            // Parse CSV rows and assign to videos based on timestamp
            while (std::getline(file, line)) {
                if (line.empty()) continue;

                // Parse timestamp from CSV (format: index,frame_timestamp,...)
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

                        // Check tracking quality
                        if (line.find("-nan(ind)") != std::string::npos) {
                            result.tracking_failed_frames++;
                        } else {
                            result.tracking_success_frames++;
                        }

                        break;
                    }
                }
            }

            return true;

        } catch (const std::exception& e) {
            std::cout << "[Tobii] CSV processing failed: " << e.what() << "\n";
            return false;
        }
    }

    void _writeResult() {
        if (!std::filesystem::exists(gonfig.verified_path)) {
            std::filesystem::create_directories(gonfig.verified_path);
        }

        std::string csv_path = gonfig.verified_path + "tobii_verify_result.csv";
        std::ofstream csv(csv_path);

        if (!csv.is_open()) {
            std::cout << "[Tobii] Failed to create result CSV file: " << csv_path << "\n";
            return;
        }

        // Write header
        csv << "video_name,duration,total_frames,expected_frames,tracking_success_frames,tracking_failed_frames\n";

        // Write each video result
        for (const auto& video : video_results_) {
            csv << video.video_name << ","
                << std::fixed << std::setprecision(3) << video.duration << ","
                << video.total_frames << ","
                << video.expected_frames << ","
                << video.tracking_success_frames << ","
                << video.tracking_failed_frames << "\n";
        }

        csv.close();
        std::cout << "[Tobii] Results written to " << csv_path << "\n";
    }
};
