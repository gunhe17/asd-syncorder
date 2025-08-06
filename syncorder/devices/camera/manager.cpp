// local
#include <syncorder/error/exception.h>
#include <syncorder/devices/common/manager_base.h>
#include <syncorder/devices/camera/device.cpp>
#include <syncorder/devices/camera/callback.cpp>
#include <syncorder/devices/camera/buffer.cpp>
#include <syncorder/devices/camera/broker.cpp>


/**
 * @class Manager
 */

class CameraManager : public BManager {
private:
    int device_id_;
    
    std::unique_ptr<CameraDevice> device_;
    Microsoft::WRL::ComPtr<CameraCallback> callback_;
    std::unique_ptr<CameraBuffer> buffer_;
    std::unique_ptr<CameraBroker> broker_;

public:
    explicit CameraManager(int device_id)
    : 
        device_id_(device_id) {
            device_ = std::make_unique<CameraDevice>(device_id);
            callback_ = Microsoft::WRL::Make<CameraCallback>();
            buffer_ = std::make_unique<CameraBuffer>();
            broker_ = std::make_unique<CameraBroker>();
        }

public:
    bool setup() override {
        // device
        device_->pre_setup(callback_->getIUnknown());
        device_->setup();
        
        // callback
        callback_->setup(device_->getReader(), static_cast<void*>(buffer_.get()));

        // broker
        broker_->setup(buffer_.get(), reinterpret_cast<void*>(&CameraBuffer::dequeue));

        // flag
        is_setup_.store(true);

        // return
        return true;
    }
    
    bool warmup() override {
        device_->warmup();
        callback_->warmup();

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

    bool stop() override { return true; }
    bool cleanup() override { return true; }

    std::string __name__() const override {
        return "Camera";
    }
};