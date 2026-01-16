#pragma once

#include <thread>
#include <iostream>
#include <fstream>
#include <filesystem>

// installed
#include <librealsense2/rs.hpp>

// local
#include <syncorder/gonfig/gonfig.h>
#include <syncorder/devices/common/checker_base.h>


/**
 * @class Realsense Checker
 */

class RealsenseChecker : public BChecker {
private:
    std::string output_path_;

public:
    RealsenseChecker() {
        output_path_ = gonfig.output_path;
    }

    ~RealsenseChecker() = default;

public:
    bool check() override {
        std::cout << "[Realsense] Starting check for flat structure\n";

        result_ = true;

        std::string csv_path = "";
        std::string bag_path = "";

        // Scan realsense directory
        std::string realsense_path = output_path_ + "/realsense";

        try {
            if (std::filesystem::exists(realsense_path)) {
                for (const auto& entry : std::filesystem::directory_iterator(realsense_path)) {
                    if (entry.is_regular_file()) {
                        auto ext = entry.path().extension().string();
                        if (ext == ".csv") {
                            csv_path = entry.path().generic_string();
                        } else if (ext == ".bag") {
                            bag_path = entry.path().generic_string();
                        }
                    }
                }
            }

            // Verify CSV
            if (!csv_path.empty()) {
                if (!_checkCsv(csv_path)) {
                    result_ = false;
                }
            } else {
                std::cout << "[Realsense] Warning: No CSV file found\n";
                result_ = false;
            }

            // Verify BAG
            if (!bag_path.empty()) {
                if (!_checkBag(bag_path)) {
                    result_ = false;
                }
            } else {
                std::cout << "[Realsense] Warning: No BAG file found\n";
                result_ = false;
            }

        } catch (const std::exception& e) {
            std::cout << "[Realsense] Check error: " << e.what() << "\n";
            result_ = false;
        }

        _writeResult();

        std::cout << "[Realsense] Check phase " << (result_ ? "completed" : "failed") << "\n";
        return result_;
    }

private:
    bool _checkCsv(const std::string& csv_path) {
        std::cout << "[Realsense] Verifying CSV file: " << csv_path << "\n";

        if (!std::filesystem::exists(csv_path)) {
            std::cout << "[Realsense] File does not exist\n";
            return false;
        }

        auto file_size = std::filesystem::file_size(csv_path);
        std::cout << "[Realsense] File size: " << file_size << " bytes\n";

        if (file_size == 0) {
            std::cout << "[Realsense] File is empty\n";
            return false;
        }

        try {
            std::ifstream file(csv_path);
            std::string line;
            int line_count = 0;
            bool header_valid = false;

            // Read and verify header
            if (std::getline(file, line)) {
                line_count++;
                std::cout << "[Realsense] First line: " << line << "\n";
                if (line.find("index,") == 0) {
                    header_valid = true;
                } else {
                    std::cout << "[Realsense] Invalid CSV header format\n";
                    return false;
                }
            } else {
                std::cout << "[Realsense] Could not read first line\n";
                return false;
            }

            // Count data rows (excluding header)
            while (std::getline(file, line)) {
                if (!line.empty()) {
                    line_count++;
                }
            }

            int data_row_count = line_count - 1; // Exclude header
            int expected_frames = gonfig.record_duration * 60; // 60 fps

            std::cout << "[Realsense] Data rows: " << data_row_count << "\n";
            std::cout << "[Realsense] Expected frames (60fps * " << gonfig.record_duration << "s): " << expected_frames << "\n";

            if (data_row_count < expected_frames) {
                std::cout << "[Realsense] Insufficient frames (expected: >=" << expected_frames << ", actual: " << data_row_count << ")\n";
                return false;
            }

            if (data_row_count > expected_frames) {
                std::cout << "[Realsense] Extra frames recorded: +" << (data_row_count - expected_frames) << " frames (acceptable due to stop timing)\n";
            }

            std::cout << "[Realsense] File verification successful\n";
            return true;

        } catch (const std::exception& e) {
            std::cout << "[Realsense] File verification failed: " << e.what() << "\n";
            return false;
        }
    }

    bool _checkBag(const std::string& bag_path) {
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

        // Verify BAG file by attempting to open with RealSense SDK
        try {
            std::string temp_path = bag_path + ".verify.bag";
            std::cout << "[Realsense] Creating temporary copy for verification: " << temp_path << "\n";

            std::filesystem::copy_file(bag_path, temp_path, std::filesystem::copy_options::overwrite_existing);

            rs2::config cfg;
            cfg.enable_device_from_file(temp_path, false);

            rs2::pipeline pipe;
            pipe.start(cfg);
            pipe.stop();

            std::filesystem::remove(temp_path);
            std::cout << "[Realsense] BAG file verification successful\n";
            return true;

        } catch (const rs2::error& e) {
            std::cout << "[Realsense] RealSense error during BAG verification: " << e.what() << "\n";
            return false;
        } catch (const std::filesystem::filesystem_error& e) {
            std::cout << "[Realsense] Filesystem error during BAG verification: " << e.what() << "\n";
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

        csv << "valid\n";
        csv << result_;

        csv.close();
        std::cout << "[Realsense] Results written to " << csv_path << "\n";
    }
};
