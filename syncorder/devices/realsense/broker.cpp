#pragma once

#include <atomic>
#include <thread>
#include <chrono>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <mutex>
#include <sstream>
#include <iomanip>

// installed
#include <librealsense2/rs.hpp>

// local
#include <syncorder/gonfig/gonfig.h>
#include <syncorder/error/exception.h>
#include <syncorder/devices/common/broker_base.h>
#include <syncorder/devices/realsense/model.h>

// third-party
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>


/**
 * @class Broker
 */

class RealsenseBroker : public TBBroker<RealsenseBufferData> {
private:
    std::ofstream csv_;
    std::string output_;
    size_t index_ = 0;

    // image saver
    std::thread image_thread_;
    std::atomic<bool> image_running_{false};
    rs2::frame current_frame_;
    std::mutex frame_mutex_;

public:
    RealsenseBroker(bool create_output) {
        output_ = gonfig.output_path + "realsense/";

        if (create_output) {
            std::filesystem::create_directories(output_);
            
            csv_.open(output_ + "realsense_data.csv");
            csv_ << "index,color_timestamp,depth_timestamp,color_frame_number,depth_frame_number\n";
        }
    }

    ~RealsenseBroker() {}

public:
    void start() {
        TBBroker<RealsenseBufferData>::start();
        image_running_ = true;
        image_thread_ = std::thread(&RealsenseBroker::_imageSaver, this);
    }

    void stop() {
        TBBroker<RealsenseBufferData>::stop();
        image_running_ = false;
        if (image_thread_.joinable()) image_thread_.join();
    }

    void cleanup() {
        csv_.flush();
        csv_.close();
    }

protected:
    void _process(const RealsenseBufferData& data) override {
        _write(data);

        // Update current frame for image saver
        std::lock_guard<std::mutex> lock(frame_mutex_);
        current_frame_ = data.color_frame;
    }

private:
    void _write(const RealsenseBufferData& data) {
        // Use high precision output for timestamps
        csv_ << index_ << ","
             << std::fixed << std::setprecision(14) << data.color_frame.get_timestamp() << ","
             << std::fixed << std::setprecision(14) << data.depth_frame.get_timestamp() << ","
             << data.color_frame.get_frame_number() << ","
             << data.depth_frame.get_frame_number() << "\n";
        index_++;
    }

    void _imageSaver() {
        std::string filename = output_ + "monitor.png";

        while (image_running_) {
            rs2::frame frame;
            {
                std::lock_guard<std::mutex> lock(frame_mutex_);
                frame = current_frame_;
            }

            if (frame) {
                auto vf = frame.as<rs2::video_frame>();
                if (vf) {
                    stbi_write_png(
                        filename.c_str(),
                        vf.get_width(), vf.get_height(),
                        vf.get_bytes_per_pixel(), vf.get_data(),
                        vf.get_width() * vf.get_bytes_per_pixel()
                    );
                }
            }

            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
};