#pragma once

#include <any>
#include <atomic>
#include <deque>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <thread>
#include <windows.h>

// local
#include <syncorder/gonfig/gonfig.h>
#include <syncorder/error/exception.h>
#include <syncorder/devices/common/broker_base.h>
#include <syncorder/devices/tobii/model.h>


/**
 * @class
 */

class TSConverter {
private:
    std::atomic<bool> _option_is_enabled;
    int64_t _boot_utc_offset_us;
    bool _boot_offset_initialized;

public:
    TSConverter() :
        _option_is_enabled(true),
        _boot_utc_offset_us(0),
        _boot_offset_initialized(false)
    {}

    void enable_global_time(bool enable) {
        _option_is_enabled.store(enable);
    }

    void update_calibration(int64_t system_request_us, int64_t device_us, int64_t system_response_us) {
        if (!_boot_offset_initialized) {
            _initialize_boot_offset(system_request_us, system_response_us);
        }
    }

    double get_frame_timestamp(int64_t timestamp_us) {
        if (_option_is_enabled.load() && _boot_offset_initialized) {
            double timestamp_ms = timestamp_us / 1000.0;
            return timestamp_ms + (_boot_utc_offset_us / 1000.0);
        } else {
            return static_cast<double>(timestamp_us) / 1000.0;
        }
    }

    bool is_ready() const {
        return _boot_offset_initialized;
    }

private:
    void _initialize_boot_offset(int64_t system_request_us, int64_t system_response_us) {
        auto now_utc = std::chrono::system_clock::now();
        auto utc_us = std::chrono::duration_cast<std::chrono::microseconds>(
            now_utc.time_since_epoch()).count();
        
        int64_t avg_system_time_us = (system_request_us + system_response_us) / 2;
        _boot_utc_offset_us = utc_us - avg_system_time_us;
        
        _boot_offset_initialized = true;
    }
};


/**
 * @class Broker
 */

class TobiiBroker : public TBBroker<TobiiBufferData> {
private:
    std::ofstream csv_;
    std::string output_;

    TSConverter* converter_;

    // for csv
    size_t index_ = 0;


public:
    TobiiBroker() {
        output_ = gonfig.output_path + "tobii/";

        std::filesystem::create_directories(output_);

        csv_.open(output_ + "tobii_data.csv");
        csv_
            <<"index,"

            <<"frame_timestamp,"
            <<"frame_hardware_timestamp,"

            <<"left_gaze_display_x,"
            <<"left_gaze_display_y,"
            <<"left_gaze_3d_x,"
            <<"left_gaze_3d_y,"
            <<"left_gaze_3d_z,"
            <<"left_gaze_validity,"

            <<"left_gaze_origin_x,"
            <<"left_gaze_origin_y,"
            <<"left_gaze_origin_z,"
            <<"left_gaze_origin_validity,"

            <<"left_pupil_diameter,"
            <<"left_pupil_validity,"

            <<"right_gaze_display_x,"
            <<"right_gaze_display_y,"
            <<"right_gaze_3d_x,"
            <<"right_gaze_3d_y,"
            <<"right_gaze_3d_z,"
            <<"right_gaze_validity,"

            <<"right_gaze_origin_x,"
            <<"right_gaze_origin_y,"
            <<"right_gaze_origin_z,"
            <<"right_gaze_origin_validity,"

            <<"right_pupil_diameter,"
            <<"right_pupil_validity\n";
    }
    ~TobiiBroker() {}

public:
    void pre_setup(TSConverter* converter) {
        converter_ = converter;
        converter_->enable_global_time(true);
    }

    void cleanup() {
        csv_.flush();
        csv_.close();
    }

protected:
    void _process(const TobiiBufferData& data) override {
        _write(data);
    }

private:
    void _write(const TobiiBufferData& data) {
        std::ostringstream system_time_stamp;
        system_time_stamp << std::fixed << std::setprecision(14) << converter_->get_frame_timestamp(data.gazed.system_time_stamp);

        csv_
            << index_ << ","

            << system_time_stamp.str() << ","
            << data.gazed.device_time_stamp << ","

            << data.gazed.left_eye.gaze_point.position_on_display_area.x << ","
            << data.gazed.left_eye.gaze_point.position_on_display_area.y << ","
            << data.gazed.left_eye.gaze_point.position_in_user_coordinates.x << ","
            << data.gazed.left_eye.gaze_point.position_in_user_coordinates.y << ","
            << data.gazed.left_eye.gaze_point.position_in_user_coordinates.z << ","
            << data.gazed.left_eye.gaze_point.validity << ","
            
            << data.gazed.left_eye.gaze_origin.position_in_user_coordinates.x << ","
            << data.gazed.left_eye.gaze_origin.position_in_user_coordinates.y << ","
            << data.gazed.left_eye.gaze_origin.position_in_user_coordinates.z << ","
            << data.gazed.left_eye.gaze_origin.validity << ","

            << data.gazed.left_eye.pupil_data.diameter << ","
            << data.gazed.left_eye.pupil_data.validity << ","

            << data.gazed.right_eye.gaze_point.position_on_display_area.x << ","
            << data.gazed.right_eye.gaze_point.position_on_display_area.y << ","
            << data.gazed.right_eye.gaze_point.position_in_user_coordinates.x << ","
            << data.gazed.right_eye.gaze_point.position_in_user_coordinates.y << ","
            << data.gazed.right_eye.gaze_point.position_in_user_coordinates.z << ","
            << data.gazed.right_eye.gaze_point.validity << ","

            << data.gazed.right_eye.gaze_origin.position_in_user_coordinates.x << ","
            << data.gazed.right_eye.gaze_origin.position_in_user_coordinates.y << ","
            << data.gazed.right_eye.gaze_origin.position_in_user_coordinates.z << ","
            << data.gazed.right_eye.gaze_origin.validity << ","

            << data.gazed.right_eye.pupil_data.diameter << ","
            << data.gazed.right_eye.pupil_data.validity << ","
            
            << "\n";

        index_++;
    }
};