#pragma once

// installed
#include <librealsense2/rs.hpp>

// local
#include <syncorder/devices/realsense/buffer.cpp> //TODO: include buffer
#include <syncorder/error/exception.h>
#include <syncorder/monitoring/realsense_monitor.h>


/**
 * @class Callback
 */

class RealsenseCallback {
private:
    static inline RealsenseCallback* instance_ = nullptr;
    void* buffer_;
    void* monitor_;

    // flag
    std::atomic<bool> first_frame_received_;

public:
    RealsenseCallback() {}
    ~RealsenseCallback() {}

public:
    void setup(void* buffer, void* monitor = nullptr) {
        instance_ = this;
        buffer_ = buffer;
        monitor_ = monitor;

        first_frame_received_.store(false);
    }

    bool warmup() {
        auto start = std::chrono::steady_clock::now();
        auto end = std::chrono::milliseconds(10000);
        
        while (!first_frame_received_.load()) {
            auto elapsed = std::chrono::steady_clock::now() - start;
            if (elapsed >= end) {
                if (monitor_) {
                    auto* realsense_monitor = static_cast<RealsenseMonitor*>(monitor_);
                    realsense_monitor->onError("Warmup timeout - no frames received within 10 seconds");
                }
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
        try {
            // flag
            if (!first_frame_received_.load()) {
                first_frame_received_.store(true);
                if (monitor_) {
                    auto* realsense_monitor = static_cast<RealsenseMonitor*>(monitor_);
                    realsense_monitor->onDeviceEvent("FIRST_FRAME_RECEIVED", "First frame received successfully");
                }
            }

            // Monitor frame event
            if (monitor_) {
                try {
                    auto current_time = std::chrono::duration<double, std::milli>(
                        std::chrono::system_clock::now().time_since_epoch()).count();

                    auto frame_timestamp = frame.get_timestamp();
                    auto latency = current_time - frame_timestamp;

                    auto* realsense_monitor = static_cast<RealsenseMonitor*>(monitor_);
                    realsense_monitor->onFrameReceived(frame_timestamp, latency);

                } catch (const std::exception& e) {
                    if (monitor_) {
                        auto* realsense_monitor = static_cast<RealsenseMonitor*>(monitor_);
                        realsense_monitor->onError("Frame monitoring error: " + std::string(e.what()));
                    }
                }
            }

            // buffer - enqueue frame data
            if (buffer_) {
                try {
                    // Check if this is a frameset
                    if (auto fs = frame.as<rs2::frameset>()) {
                        auto color = fs.get_color_frame();
                        auto depth = fs.get_depth_frame();

                        if (color && depth) {
                            auto* realsense_buffer = static_cast<RealsenseBuffer*>(buffer_);
                            RealsenseBufferData data(color, depth);
                            realsense_buffer->enqueue(std::move(data));
                        }
                    }
                } catch (const std::exception& e) {
                    if (monitor_) {
                        auto* realsense_monitor = static_cast<RealsenseMonitor*>(monitor_);
                        realsense_monitor->onError("Buffer enqueue error: " + std::string(e.what()));
                    }
                }
            }

        } catch (const std::exception& e) {
            // Report to monitor if available
            if (monitor_) {
                try {
                    auto* realsense_monitor = static_cast<RealsenseMonitor*>(monitor_);
                    realsense_monitor->onError("Critical frame error: " + std::string(e.what()));
                } catch (...) {
                    // Even monitor reporting failed - no fallback needed as monitor handles its own errors
                }
            }
        }
    }
};