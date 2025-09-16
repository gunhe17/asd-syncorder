// local
#include <syncorder/error/exception.h>
#include <syncorder/devices/common/manager_base.h>
#include <syncorder/devices/realsense/device.cpp>
#include <syncorder/devices/realsense/callback.cpp>
#include <syncorder/devices/realsense/buffer.cpp>
#include <syncorder/devices\realsense\broker.cpp>
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

        } catch (const std::exception&) {
            success = false;
        }

        return success;
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