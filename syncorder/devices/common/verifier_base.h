#pragma once

#include <string>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <vector>


/**
 * @struct VideoTimingData
 * Timing data for a single video
 */
struct VideoTimingData {
    int video_index{-1};
    double start_time{0.0};
    double end_time{0.0};
    std::string end_type;
    bool valid{false};

    double getDuration() const {
        return end_time - start_time;
    }

    std::string getVideoName() const {
        return "VIDEO_INDEX_" + std::to_string(video_index);
    }
};

/**
 * @struct FrameTimingData
 * Parsed data from frame_timing.log (contains all videos)
 */
struct FrameTimingData {
    std::vector<VideoTimingData> videos;
    bool valid{false};
};


/**
 * @class Base Verifier
 * Validates session structure recordings (multi-session recordings)
 */

class BVerifier {
protected:
    bool result_{true};
    int valid_sessions_{0};
    int total_sessions_{0};

public:
    BVerifier() = default;
    virtual ~BVerifier() = default;

public:
    virtual bool verify() = 0;

protected:
    // Parse frame_timing.log file
    FrameTimingData _parseFrameTiming(const std::string& timing_path) {
        FrameTimingData data;

        if (!std::filesystem::exists(timing_path)) {
            std::cout << "[Verifier] frame_timing.log not found: " << timing_path << "\n";
            return data;
        }

        try {
            std::ifstream file(timing_path);
            std::string line;

            std::map<int, VideoTimingData> video_map;

            while (std::getline(file, line)) {
                std::istringstream iss(line);
                std::string token;
                iss >> token;

                if (token == "FIRST_FRAME") {
                    double timestamp;
                    std::string video_index_str;
                    iss >> timestamp >> video_index_str; // "VIDEO_INDEX_N"

                    // Extract number from "VIDEO_INDEX_1" format
                    size_t underscore_pos = video_index_str.rfind('_');
                    if (underscore_pos != std::string::npos) {
                        int video_index = std::stoi(video_index_str.substr(underscore_pos + 1));
                        video_map[video_index].video_index = video_index;
                        video_map[video_index].start_time = timestamp;
                    }
                }
                else if (token == "LAST_FRAME") {
                    double timestamp;
                    std::string video_index_str;
                    std::string end_type;
                    iss >> timestamp >> video_index_str >> end_type;

                    size_t underscore_pos = video_index_str.rfind('_');
                    if (underscore_pos != std::string::npos) {
                        int video_index = std::stoi(video_index_str.substr(underscore_pos + 1));
                        video_map[video_index].video_index = video_index;
                        video_map[video_index].end_time = timestamp;
                        video_map[video_index].end_type = end_type;
                        video_map[video_index].valid = true;
                    }
                }
            }

            // Convert map to vector (sorted by video_index)
            for (const auto& [index, video] : video_map) {
                if (video.valid) {
                    data.videos.push_back(video);
                }
            }

            data.valid = !data.videos.empty();

            if (data.valid) {
                std::cout << "[Verifier] Parsed " << data.videos.size() << " video(s) from frame_timing\n";
                for (const auto& video : data.videos) {
                    std::cout << "[Verifier]   " << video.getVideoName()
                              << " (duration: " << video.getDuration() << "s)\n";
                }
            }

        } catch (const std::exception& e) {
            std::cout << "[Verifier] Error parsing frame_timing.log: " << e.what() << "\n";
            data.valid = false;
        }

        return data;
    }
};
