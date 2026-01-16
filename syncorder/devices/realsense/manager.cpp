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
#include <syncorder/devices/realsense/checker.cpp>
#include <syncorder/devices/realsense/verifier.cpp>
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

    // checker & verifier
    std::unique_ptr<RealsenseChecker> checker_;
    std::unique_ptr<RealsenseVerifier> verifier_;

    // monitor
    std::thread mt_thread_;
    std::atomic<bool> monitor_in_progress_{false};
    std::unique_ptr<RealsenseMonitor> realsense_monitor_;

public:
    explicit RealsenseManager(int device_id, bool create_output=true)
    :
        device_id_(device_id) {
            device_ = std::make_unique<RealsenseDevice>(device_id);
            callback_ = std::make_unique<RealsenseCallback>();
            buffer_ = std::make_unique<RealsenseBuffer>();
            broker_ = std::make_unique<RealsenseBroker>(create_output);

            checker_ = std::make_unique<RealsenseChecker>();
            verifier_ = std::make_unique<RealsenseVerifier>();

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
        broker_->start();
        buffer_->start();

        // flag
        is_running_.store(true);

        // monitor
        realsense_monitor_->onRecordingStart();

        return true;
    }

    bool stop() override {
        bool success = true;

        try {
            realsense_monitor_->onRecordingStop();

            // Stop broker and buffer first to close gate
            broker_->stop();
            buffer_->stop();

            // Then stop device
            if (!device_->stop()) {
                success = false;
            }

            realsense_monitor_->stop();

            monitor_in_progress_.store(false);
            if (mt_thread_.joinable()) {
                mt_thread_.join();
            }

        } catch (const std::exception&) {
            success = false;

            try {
                realsense_monitor_->stop();
            } catch (const std::exception&) {

            }
        }

        return success;
    }

    bool cleanup() override {
        device_->cleanup();
        broker_->cleanup();

        device_.reset();
        callback_.reset();
        buffer_.reset();
        broker_.reset();

        realsense_monitor_.reset();

        return true;
    }

    bool check() override {
        return checker_->check();
    }

    bool verify() override {
        return verifier_->verify();
    }

    std::string __name__() const override {
        return "Realsense";
    }

private:
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