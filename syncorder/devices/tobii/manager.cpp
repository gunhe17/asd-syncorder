#include <chrono>
#include <inttypes.h>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>

// local
#include <syncorder/error/exception.h>
#include <syncorder/devices/common/manager_base.h>
#include <syncorder/devices/tobii/device.cpp>
#include <syncorder/devices/tobii/callback.cpp>
#include <syncorder/devices/tobii/buffer.cpp>
#include <syncorder/devices/tobii/broker.cpp>


/**
 * @class Manager
 */

class TobiiManager : public BManager {
private:
    int device_id_;

    std::unique_ptr<TobiiDevice> device_;
    std::unique_ptr<TobiiCallback> callback_;
    std::unique_ptr<TobiiBuffer> buffer_;
    std::unique_ptr<TobiiBroker> broker_;

    // converter
    std::unique_ptr<TSConverter> converter_;

    // calibrate
    std::thread cb_thread_;
    std::atomic<bool> calibrate_in_progress_{true};

    // monitor
    std::thread mt_thread_;
    std::atomic<bool> monitor_in_progress_{false};

public:
    explicit TobiiManager(int device_id)
    : 
        device_id_(device_id) {
            device_ = std::make_unique<TobiiDevice>(device_id);
            callback_ = std::make_unique<TobiiCallback>();
            buffer_ = std::make_unique<TobiiBuffer>();
            broker_ = std::make_unique<TobiiBroker>();

            converter_ = std::make_unique<TSConverter>();
        }

public:
    bool setup() override {
        // device
        device_->pre_setup(callback_.get(), reinterpret_cast<void*>(&TobiiCallback::onGaze));
        device_->setup();

        // callback
        callback_->setup(static_cast<void*>(buffer_.get()));

        // broker
        broker_->pre_setup(converter_.get());
        broker_->setup(buffer_.get(), reinterpret_cast<void*>(&TobiiBuffer::dequeue));

        // flag
        is_setup_.store(true);

        return true;
    }
    
    bool warmup() override {
        device_->warmup();
        callback_->warmup();

        // ts
        calibrate_in_progress_.store(true);
        _calibrate();

        // monitor
        monitor_in_progress_.store(true);
        // _monitor(); // *optional

        // flag
        is_warmup_.store(true);

        return true;
    }
    
    bool start() override {
        broker_->start();
        buffer_->start();

        // flag
        is_running_.store(true);

        return true;
    }

    bool stop() override {
        broker_->stop();
        buffer_->stop();
        device_->stop();

        // calibrate
        calibrate_in_progress_.store(false);

        // monitor
        monitor_in_progress_.store(false);

        return true;
    }

    bool cleanup() override {
        broker_->cleanup();
        device_->cleanup();

        return true;
    }

    bool verify(std::map<std::string, std::vector<std::string>> files) override {
        auto& csv_files = files["tobii_csvs"];
        std::cout << "[Tobii] Starting verification of " << csv_files.size() << " CSV files\n";

        int valid_files = 0;
        for (const auto& csv_path : csv_files) {
            if (_verify(csv_path)) {
                valid_files++;
            }
        }

        bool result = (valid_files == csv_files.size());
        std::cout << "[Tobii] Summary: " << valid_files << "/" << csv_files.size() << " files valid\n";
        std::cout << "[Tobii] Verify phase " << (result ? "completed" : "failed") << "\n";
        return result;
    }

    std::string __name__() const override {
        return "Tobii";
    }

private:
    bool _verify(const std::string& csv_path) {
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
            std::string first_line;
            if (std::getline(file, first_line)) {
                std::cout << "[Tobii] First line: " << first_line << "\n";
                if (first_line.find("index,") == 0) {
                    std::cout << "[Tobii] File verification successful\n";
                    return true;
                } else {
                    std::cout << "[Tobii] Invalid CSV header format\n";
                    return false;
                }
            } else {
                std::cout << "[Tobii] Could not read first line\n";
                return false;
            }
        } catch (const std::exception& e) {
            std::cout << "[Tobii] File verification failed: " << e.what() << "\n";
            return false;
        }
    }

    void _calibrate() {
        cb_thread_ = std::thread([this]() {
            while (calibrate_in_progress_.load()) {
                auto time = device_->getTime();
                converter_->update_calibration(
                    time.system_request_time_stamp,
                    time.device_time_stamp,
                    time.system_response_time_stamp
                );
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });
    }

    void _monitor() {
        mt_thread_ = std::thread([this]() {
            while (monitor_in_progress_.load()) {

                // monotic ts
                auto monotonic_req = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();
                TobiiResearchGazeData monotonic_gaze = device_->getGaze();
                auto monotonic_res = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();

                std::cout << "[LOG] tobii timestamp monitor" << std::fixed << std::setprecision(3) << "\n"
                    << "monotonic Request:   " << monotonic_req << " ms" << "\n"
                    << "monotonic Converted: " << monotonic_gaze.system_time_stamp << " ms" << "\n"
                    << "monotonic Response:  " << monotonic_res << " ms" << "\n"
                    << "---" << std::endl;

                // global ts
                auto global_req = std::chrono::duration<double, std::milli>(std::chrono::system_clock::now().time_since_epoch()).count();
                TobiiResearchGazeData global_gaze = device_->getGaze();
                auto global_res = std::chrono::duration<double, std::milli>(std::chrono::system_clock::now().time_since_epoch()).count();

                std::cout << "[LOG] tobii timestamp monitor" << std::fixed << std::setprecision(3) << "\n"
                    << "global Request:   " << global_req << " ms" << "\n"
                    << "global Converted: " << converter_->get_frame_timestamp(global_gaze.system_time_stamp) << " ms" << "\n"
                    << "global Response:  " << global_res << " ms" << "\n"
                    << "---" << std::endl;

            }
        });
    }
};