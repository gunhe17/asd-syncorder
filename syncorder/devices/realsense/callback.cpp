#pragma once

// installed
#include <librealsense2/rs.hpp>

// local
#include <syncorder/devices/realsense/buffer.cpp> //TODO: include buffer
#include <syncorder/error/exception.h>


/**
 * @class Callback
 */

class RealsenseCallback {
private:
    static inline RealsenseCallback* instance_ = nullptr;
    void* buffer_;

    // flag
    std::atomic<bool> first_frame_received_;

public:
    RealsenseCallback() {}
    ~RealsenseCallback() {}

public:
    void setup(void* buffer) {
        instance_ = this;
        buffer_ = buffer;

        first_frame_received_.store(false);
    }

    bool warmup() {
        auto start = std::chrono::steady_clock::now();
        auto end = std::chrono::milliseconds(10000);
        
        while (!first_frame_received_.load()) {
            auto elapsed = std::chrono::steady_clock::now() - start;
            if (elapsed >= end) {
                std::cout << "[ERROR] warmup timeout\n";
                return false;
            }
        }

        return true;
    }

    static void onFrameset(const rs2::frame& frame) {
        if (instance_) {
            instance_->_onFrameset(frame);
        }
    }

private:
    void _onFrameset(const rs2::frame& frame) {
        // flag
        if (!first_frame_received_.load()) first_frame_received_.store(true);

        // buffer
        // *.bag를 사용하는 이유로 생략

        std::cout << "LOG|REALSENSE|FRAME|SUCCESS" << "\n";
    }
};