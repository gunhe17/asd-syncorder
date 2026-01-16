#pragma once

#include <iostream>
#include <fstream>
#include <filesystem>

// local
#include <syncorder/gonfig/gonfig.h>
#include <syncorder/devices/common/checker_base.h>


/**
 * @class Tobii Checker
 */

class TobiiChecker : public BChecker {
private:
    std::string output_path_;

public:
    TobiiChecker() {
        output_path_ = gonfig.output_path;
    }

    ~TobiiChecker() = default;

public:
    bool check() override {
        std::cout << "[Tobii] Starting check for flat structure\n";

        result_ = true;

        std::string csv_path = "";

        // Scan tobii directory
        std::string tobii_path = output_path_ + "/tobii";

        try {
            if (std::filesystem::exists(tobii_path)) {
                for (const auto& entry : std::filesystem::directory_iterator(tobii_path)) {
                    if (entry.is_regular_file()) {
                        auto ext = entry.path().extension().string();
                        if (ext == ".csv") {
                            csv_path = entry.path().generic_string();
                            break;
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
                std::cout << "[Tobii] Warning: No CSV file found\n";
                result_ = false;
            }

        } catch (const std::exception& e) {
            std::cout << "[Tobii] Check error: " << e.what() << "\n";
            result_ = false;
        }

        _writeResult();

        std::cout << "[Tobii] Check phase " << (result_ ? "completed" : "failed") << "\n";
        return result_;
    }

private:
    bool _checkCsv(const std::string& csv_path) {
        std::cout << "[Tobii] Verifying file: " << csv_path << "\n";

        if (!std::filesystem::exists(csv_path)) {
            std::cout << "[Tobii] File does not exist\n";
            return false;
        }

        auto file_size = std::filesystem::file_size(csv_path);
        std::cout << "[Tobii] File size: " << file_size << " bytes\n";

        if (file_size == 0) {
            std::cout << "[Tobii] File is empty\n";
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
                std::cout << "[Tobii] First line: " << line << "\n";
                if (line.find("index,") == 0) {
                    header_valid = true;
                } else {
                    std::cout << "[Tobii] Invalid CSV header format\n";
                    return false;
                }
            } else {
                std::cout << "[Tobii] Could not read first line\n";
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

            std::cout << "[Tobii] Data rows: " << data_row_count << "\n";
            std::cout << "[Tobii] Expected frames (60fps * " << gonfig.record_duration << "s): " << expected_frames << "\n";

            if (data_row_count < expected_frames) {
                std::cout << "[Tobii] Insufficient frames (expected: >=" << expected_frames << ", actual: " << data_row_count << ")\n";
                return false;
            }

            if (data_row_count > expected_frames) {
                std::cout << "[Tobii] Extra frames recorded: +" << (data_row_count - expected_frames) << " frames (acceptable due to stop timing)\n";
            }

            std::cout << "[Tobii] File verification successful\n";
            return true;

        } catch (const std::exception& e) {
            std::cout << "[Tobii] File verification failed: " << e.what() << "\n";
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

        csv << "valid\n";
        csv << result_;

        csv.close();
        std::cout << "[Tobii] Results written to " << csv_path << "\n";
    }
};
