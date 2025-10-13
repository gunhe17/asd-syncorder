#include <chrono>
#include <inttypes.h>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>

// local
#include <syncorder/error/exception.h>
#include <syncorder/devices/common/manager_base.h>
#include <syncorder/devices/realsense/device.cpp>
#include <syncorder/devices/realsense/callback.cpp>
#include <syncorder/devices/realsense/buffer.cpp>
#include <syncorder/devices/realsense/broker.cpp>
#include <syncorder/monitoring/realsense_monitor.h>


/**
 * @class Manager
 */

class RealsenseManager : public BManager {
private:
    int device_id_;

    std::unique_ptr<RealsenseDevice> device_;
    std::unique_ptr<RealsenseCallback> callback_;
    std::unique_ptr<RealsenseBuffer> buffer_;
    std::unique_ptr<RealsenseBroker> broker_;

    // monitor
    std::thread mt_thread_;
    std::atomic<bool> monitor_in_progress_{false};
    std::unique_ptr<RealsenseMonitor> realsense_monitor_;
public:
    explicit RealsenseManager(int device_id)
    : 
        device_id_(device_id) {
            device_ = std::make_unique<RealsenseDevice>(device_id);
            callback_ = std::make_unique<RealsenseCallback>();
            buffer_ = std::make_unique<RealsenseBuffer>();
            broker_ = std::make_unique<RealsenseBroker>();
            realsense_monitor_ = std::make_unique<RealsenseMonitor>();
        }

public:
    bool setup() override {
        // device
        device_->pre_setup(reinterpret_cast<void*>(&RealsenseCallback::onFrameset));
        device_->setup();

        // callback
        callback_->setup(static_cast<void*>(buffer_.get()), static_cast<void*>(realsense_monitor_.get()));

        // broker
        broker_->setup(buffer_.get(), reinterpret_cast<void*>(&RealsenseBuffer::dequeue));

        // flag
        is_setup_.store(true);

        // return
        return true;
    }
    
    bool warmup() override {
        // Start realsense monitor
        realsense_monitor_->start();

        device_->warmup();
        callback_->warmup();

        // monitor
        monitor_in_progress_.store(true);
        // _monitor(); // *optional

        // flag
        is_warmup_.store(true);

        return true;
    }
    
    bool start() override {
        // buffer_->start(); *buffer not used.
        broker_->start();

        // Notify monitor that recording has started
        realsense_monitor_->onRecordingStart();

        // flag
        is_running_.store(true);

        return true;
    }

    bool stop() override {
        bool success = true;

        try {
            // Notify monitor that recording is stopping
            realsense_monitor_->onRecordingStop();

            // Stop components in reverse order of initialization
            realsense_monitor_->stop();

            if (!device_->stop()) {
                success = false;
            }

            // Note: broker and buffer are not used
            // broker_->stop();
            // buffer_->stop(); *buffer not used.

            // Stop internal monitoring
            monitor_in_progress_.store(false);
            if (mt_thread_.joinable()) {
                mt_thread_.join();
            }
            
        } catch (const std::exception&) {
            success = false;

            // Try to force stop the monitor even if other components failed
            try {
                realsense_monitor_->stop();
            } catch (const std::exception&) {
                // Monitor stop failed during error recovery
            }
        }

        return success;
    }

    bool cleanup() override {
        bool success = true;

        try {
            // Cleanup components in reverse order
            if (!device_->cleanup()) {
                success = false;
            }

            broker_->cleanup();

            device_.reset();
            callback_.reset();
            buffer_.reset();
            broker_.reset();
            realsense_monitor_.reset();
            
        } catch (const std::exception&) {
            success = false;
        }

        return success;
    }

    bool verify(std::map<std::string, std::vector<std::string>> files) override {
        auto& bag_files = files["realsense_bags"];
        std::cout << "[Realsense] Starting verification of " << bag_files.size() << " bag files\n";

        int valid_files = 0;
        for (const auto& bag_path : bag_files) {
            if (_verify(bag_path)) {
                valid_files++;
            }
        }

        bool result = (valid_files == bag_files.size());
        _verified(result);

        std::cout << "[Realsense] Summary: " << valid_files << "/" << bag_files.size() << " files valid\n";
        std::cout << "[Realsense] Verify phase " << (result ? "completed" : "failed") << "\n";

        return result;
    }

private:
    std::string __name__() const override {
        return "Realsense";
    }

private:
    bool _verify(const std::string& bag_path) {
        std::cout << "[Realsense] Verifying file: " << bag_path << "\n";

        if (!std::filesystem::exists(bag_path)) {
            std::cout << "[Realsense] File does not exist\n";
            return false;
        }

        // 1) 파일 크기 점검
        auto size_now = std::filesystem::file_size(bag_path);
        if (size_now == 0) {
            std::cout << "[Realsense] File size is 0 bytes\n";
            return false;
        }

        // 2) 파일 크기 안정화 대기 (쓰기/이동 중 방지)
        using namespace std::chrono_literals;
        {
            bool stable = false;
            for (int i = 0; i < 10; ++i) { // 최대 ~1s
                std::this_thread::sleep_for(100ms);
                auto size_next = std::filesystem::file_size(bag_path);
                if (size_next == size_now) { stable = true; break; }
                size_now = size_next;
            }
            if (!stable) {
                std::cout << "[Realsense] File size is still changing (likely being written)\n";
                return false;
            }
        }

        // 3) 임시 파일로 복사 후 검증
        std::string temp_verify_path = bag_path + ".verify.bag";
        bool verify_result = false;

        try {
            // 파일 복사
            std::filesystem::copy_file(bag_path, temp_verify_path,
                std::filesystem::copy_options::overwrite_existing);

            // 복사 완료 대기
            std::this_thread::sleep_for(200ms);

            // 4) 복사된 파일로 RealSense 파이프라인 검증
            rs2::config cfg;
            rs2::pipeline pipe;

            cfg.enable_device_from_file(temp_verify_path, /*repeat_playback=*/false);
            auto profile = pipe.start(cfg);     // 여기서 rosbag reader 생성/검증
            pipe.stop();                         // 핸들 즉시 반환

            verify_result = true;
        }
        catch (const rs2::error& e) {
            std::cout << "[Realsense] RS2 error: " << e.what()
                    << " (func=" << e.get_failed_function()
                    << ", args=" << e.get_failed_args() << ")\n";
            verify_result = false;
        }
        catch (const std::filesystem::filesystem_error& e) {
            std::cout << "[Realsense] File copy error: " << e.what() << "\n";
            verify_result = false;
        }

        return verify_result;
    }

    void _verified(bool result) {
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
        csv << result;

        csv.close();
        std::cout << "[Realsense] Results written to " << csv_path << "\n";
    }

    void _monitor() {
        mt_thread_ = std::thread([this]() {
            while (monitor_in_progress_.load()) {
                rs2::pipeline pipe;
                
                pipe.start();

                auto global_req = std::chrono::duration<double, std::milli>(std::chrono::system_clock::now().time_since_epoch()).count();
                rs2::frameset frames = pipe.wait_for_frames();
                rs2::frame frame = frames.get_depth_frame();
                auto global_res = std::chrono::duration<double, std::milli>(std::chrono::system_clock::now().time_since_epoch()).count();

                pipe.stop();

                std::cout << "[LOG] realsense timestamp monitor" << std::fixed << std::setprecision(3) << "\n"
                    << "global Request:   " << global_req << " ms" << "\n"
                    << "global Converted: " << frame.get_timestamp() << " ms" << "\n"
                    << "global Response:  " << global_res << " ms" << "\n"
                    << "---" << std::endl;
            }
        });
    }
};