// local
#include <syncorder/error/exception.h>
#include <syncorder/devices/common/manager_base.h>
#include <syncorder/devices/realsense/device.cpp>
#include <syncorder/devices/realsense/callback.cpp>
#include <syncorder/devices/realsense/buffer.cpp>
#include <syncorder/devices/realsense/broker.cpp>


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
public:
    explicit RealsenseManager(int device_id)
    : 
        device_id_(device_id) {
            device_ = std::make_unique<RealsenseDevice>(device_id);
            callback_ = std::make_unique<RealsenseCallback>();
            buffer_ = std::make_unique<RealsenseBuffer>();
            broker_ = std::make_unique<RealsenseBroker>();
        }

public:
    bool setup() override {
        // device
        device_->pre_setup(reinterpret_cast<void*>(&RealsenseCallback::onFrameset));
        device_->setup();

        // callback
        callback_->setup(static_cast<void*>(buffer_.get()));

        // broker
        broker_->setup(buffer_.get(), reinterpret_cast<void*>(&RealsenseBuffer::dequeue));

        // flag
        is_setup_.store(true);

        // return
        return true;
    }
    
    bool warmup() override {
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

        // flag
        is_running_.store(true);

        return true;
    }

    bool stop() override {
        // broker_->stop();
        // buffer_->stop(); *buffer not used.
        device_->stop();

        // monitor
        monitor_in_progress_.store(false);

        return true;
    }

    bool cleanup() override {
        broker_->cleanup();
        device_->cleanup();

        return true;
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

                std::cout << std::fixed << std::setprecision(3);
                std::cout << "global Request:   " << global_req << " ms" << std::endl;
                std::cout << "global Converted: " << frame.get_timestamp() << " ms" << std::endl;
                std::cout << "global Response:  " << global_res << " ms" << std::endl;
                std::cout << "---" << std::endl;
            }
        });
    }
};