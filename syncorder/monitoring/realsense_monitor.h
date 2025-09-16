#pragma once

#include <thread>
#include <atomic>
#include <iostream>
#include <chrono>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <mutex>
#include <vector>
#include <algorithm>
#include <cmath>
#include <librealsense2/rs.hpp>
#include "../gonfig/gonfig.h"

class RealsenseMonitor {
private:
    std::thread monitor_thread_;
    std::atomic<bool> running_{false};
    std::ofstream log_file_;
    std::mutex log_mutex_;

    // Realsense-specific monitoring
    rs2::context ctx_;
    std::vector<rs2::device> devices_;

    // Performance counters
    std::atomic<int> frame_count_{0};
    std::atomic<int> error_count_{0};
    std::atomic<double> last_fps_{0.0};
    std::atomic<double> avg_latency_{0.0};

    // Temperature monitoring
    std::atomic<float> temperature_{0.0f};

    // Timing statistics
    std::chrono::steady_clock::time_point last_frame_time_;
    std::chrono::steady_clock::time_point start_time_;

    // Recording session metrics
    std::atomic<int> frame_drops_{0};
    std::atomic<int> queue_overflows_{0};
    std::atomic<double> max_latency_{0.0};
    std::atomic<double> min_latency_{999999.0};
    std::atomic<float> max_temperature_{0.0f};
    std::vector<double> latency_history_;
    std::mutex latency_mutex_;

    // Recording quality tracking
    std::atomic<int> color_frame_count_{0};
    std::atomic<int> depth_frame_count_{0};
    std::atomic<int> motion_frame_count_{0};
    std::chrono::steady_clock::time_point recording_start_time_;
    std::chrono::steady_clock::time_point recording_stop_time_;

public:
    void start() {
        if (running_) {
            return;
        }

        if (_initializeDevices()) {
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            std::string log_path = gonfig.output_path + "realsense_monitor_" + std::to_string(time_t) + ".log";

            std::filesystem::create_directories(gonfig.output_path);
            log_file_.open(log_path, std::ios::out | std::ios::app);

            if (log_file_.is_open()) {
                running_ = true;
                start_time_ = std::chrono::steady_clock::now();
                last_frame_time_ = start_time_;

                _logDeviceInfo();
                monitor_thread_ = std::thread(&RealsenseMonitor::_monitor, this);

                // Log to file instead of console
                std::lock_guard<std::mutex> lock(log_mutex_);
                log_file_ << "[" << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())
                          << "] MONITOR_STARTED: Logging to " << log_path << "\n";
                log_file_.flush();
            }
        }
    }

    void stop() {
        if (!running_) {
            return;
        }

        _logShutdownStart();
        running_ = false;

        // Wait for monitor thread to finish gracefully
        if (monitor_thread_.joinable()) {
            auto start_time = std::chrono::steady_clock::now();
            monitor_thread_.join();
            auto end_time = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
            _logDeviceEvent("THREAD_SHUTDOWN", "Monitor thread stopped gracefully in " + std::to_string(duration) + "ms");
        }

        // Final device status check before shutdown
        _logDeviceShutdownStatus();

        if (log_file_.is_open()) {
            _logFinalStats();
            _logShutdownComplete();
            log_file_.close();
        }
    }

    ~RealsenseMonitor() {
        stop();
    }

    // Public methods for external components to report events
    void onFrameReceived(double timestamp, double latency) {
        frame_count_++;

        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_frame_time_).count();

        if (duration > 0) {
            last_fps_ = 1000.0 / duration;
        }

        // Update latency metrics
        avg_latency_ = latency;
        if (latency > max_latency_.load()) {
            max_latency_ = latency;
        }
        if (latency < min_latency_.load()) {
            min_latency_ = latency;
        }

        // Store latency history for analysis (keep last 1000 samples)
        {
            std::lock_guard<std::mutex> lock(latency_mutex_);
            latency_history_.push_back(latency);
            if (latency_history_.size() > 1000) {
                latency_history_.erase(latency_history_.begin());
            }
        }

        // Detect frame drops (gaps > 50ms indicate potential drops)
        if (duration > 50) {
            frame_drops_++;
            _logRecordingEvent("FRAME_DROP_DETECTED", "Gap of " + std::to_string(duration) + "ms detected");
        }

        last_frame_time_ = now;
        _logFrameEvent(timestamp, latency);
    }

    void onError(const std::string& error_msg) {
        error_count_++;
        _logError(error_msg);
    }

    void onDeviceEvent(const std::string& event_type, const std::string& details) {
        _logDeviceEvent(event_type, details);
    }

    void onRecordingStart() {
        recording_start_time_ = std::chrono::steady_clock::now();
        _logRecordingEvent("RECORDING_STARTED", "Recording session initiated");
    }

    void onRecordingStop() {
        recording_stop_time_ = std::chrono::steady_clock::now();
        _logRecordingEvent("RECORDING_STOPPED", "Recording session ended");
        _logRecordingAnalysis();
    }

    void onQueueOverflow() {
        queue_overflows_++;
        _logRecordingEvent("QUEUE_OVERFLOW", "Frame queue overflow detected");
    }

    void onFrameByType(const std::string& frame_type) {
        if (frame_type == "color") {
            color_frame_count_++;
        } else if (frame_type == "depth") {
            depth_frame_count_++;
        } else if (frame_type == "motion") {
            motion_frame_count_++;
        }
    }

private:
    void _monitor() {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        while (running_) {
            _updateDeviceStatus();
            _updateTemperature();
            _logPeriodicStats();

            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    bool _initializeDevices() {
        try {
            auto device_list = ctx_.query_devices();

            if (device_list.size() == 0) {
                return true; // Still allow monitoring even without devices
            }

            for (uint32_t i = 0; i < device_list.size(); ++i) {
                devices_.push_back(device_list[i]);
            }

            return true;
        } catch (const std::exception&) {
            return false;
        }
    }

    void _logDeviceInfo() {
        std::lock_guard<std::mutex> lock(log_mutex_);

        auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

        if (log_file_.is_open()) {
            log_file_ << "[" << now << "] === REALSENSE MONITOR STARTED ===\n";
            log_file_ << "[" << now << "] Devices found: " << devices_.size() << "\n";

            for (uint32_t i = 0; i < devices_.size(); ++i) {
                try {
                    auto& device = devices_[i];
                    std::string name = device.get_info(RS2_CAMERA_INFO_NAME);
                    std::string serial = device.get_info(RS2_CAMERA_INFO_SERIAL_NUMBER);
                    std::string firmware = device.get_info(RS2_CAMERA_INFO_FIRMWARE_VERSION);

                    log_file_ << "[" << now << "] Device " << i << ": " << name
                              << " (S/N: " << serial << ", FW: " << firmware << ")\n";

                    // Log available sensors
                    auto sensors = device.query_sensors();
                    for (uint32_t j = 0; j < sensors.size(); ++j) {
                        std::string sensor_name = sensors[j].get_info(RS2_CAMERA_INFO_NAME);
                        log_file_ << "[" << now << "]   Sensor " << j << ": " << sensor_name << "\n";
                    }
                } catch (const std::exception& e) {
                    log_file_ << "[" << now << "] Device " << i << ": Error getting info - " << e.what() << "\n";
                }
            }

            log_file_.flush();
        }
    }

    void _updateDeviceStatus() {
        for (uint32_t i = 0; i < devices_.size(); ++i) {
            try {
                auto& device = devices_[i];

                // Check if device is still connected
                if (!device) {
                    _logDeviceEvent("DEVICE_DISCONNECTED", "Device " + std::to_string(i) + " disconnected");
                    continue;
                }

                // Check sensors status (simplified - just log that device is connected)
                auto sensors = device.query_sensors();
                for (uint32_t j = 0; j < sensors.size(); ++j) {
                    _logDeviceEvent("SENSOR_STATUS", "Device " + std::to_string(i) + " Sensor " + std::to_string(j) + " available");
                }

            } catch (const std::exception& e) {
                _logError("Device status check failed for device " + std::to_string(i) + ": " + e.what());
            }
        }
    }

    void _updateTemperature() {
        for (uint32_t i = 0; i < devices_.size(); ++i) {
            try {
                auto& device = devices_[i];
                auto sensors = device.query_sensors();

                for (auto& sensor : sensors) {
                    if (sensor.supports(RS2_OPTION_ASIC_TEMPERATURE)) {
                        float temp = sensor.get_option(RS2_OPTION_ASIC_TEMPERATURE);
                        temperature_ = temp;

                        // Track maximum temperature
                        if (temp > max_temperature_.load()) {
                            max_temperature_ = temp;
                        }

                        if (temp > 70.0f) { // Warning threshold
                            _logRecordingEvent("HIGH_TEMPERATURE", "Device " + std::to_string(i) + " temperature: " + std::to_string(temp) + "°C");
                        }
                        break;
                    }
                }
            } catch (const std::exception&) {
                // Temperature reading failed, continue monitoring
            }
        }
    }

    void _logPeriodicStats() {
        static int counter = 0;
        if (++counter % 30 == 0) { // Every 30 seconds
            auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - start_time_).count();

            std::ostringstream oss;
            oss << "Realsense Stats - Uptime: " << uptime << "s, "
                << "Frames: " << frame_count_.load() << ", "
                << "Errors: " << error_count_.load() << ", "
                << "FPS: " << std::fixed << std::setprecision(1) << last_fps_.load() << ", "
                << "Avg Latency: " << std::fixed << std::setprecision(2) << avg_latency_.load() << "ms";

            if (temperature_.load() > 0) {
                oss << ", Temp: " << std::fixed << std::setprecision(1) << temperature_.load() << "°C";
            }

            std::lock_guard<std::mutex> lock(log_mutex_);
            if (log_file_.is_open()) {
                log_file_ << "[" << now << "] STATS: " << oss.str() << "\n";
                log_file_.flush();
            }
        }
    }

    void _logFrameEvent(double timestamp, double latency) {
        static int frame_log_counter = 0;
        if (++frame_log_counter % 100 == 0) { // Log every 100th frame to avoid spam
            auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

            std::lock_guard<std::mutex> lock(log_mutex_);
            if (log_file_.is_open()) {
                log_file_ << "[" << now << "] FRAME: timestamp=" << std::fixed << std::setprecision(3)
                          << timestamp << "ms, latency=" << latency << "ms, fps=" << last_fps_.load() << "\n";
                log_file_.flush();
            }
        }
    }

    void _logError(const std::string& error_msg) {
        auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

        std::cout << "[ERROR] Realsense: " << error_msg << "\n";

        std::lock_guard<std::mutex> lock(log_mutex_);
        if (log_file_.is_open()) {
            log_file_ << "[" << now << "] ERROR: " << error_msg << "\n";
            log_file_.flush();
        }
    }

    void _logDeviceEvent(const std::string& event_type, const std::string& details) {
        auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

        std::lock_guard<std::mutex> lock(log_mutex_);
        if (log_file_.is_open()) {
            log_file_ << "[" << now << "] " << event_type << ": " << details << "\n";
            log_file_.flush();
        }
    }

    void _logShutdownStart() {
        auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

        std::lock_guard<std::mutex> lock(log_mutex_);
        if (log_file_.is_open()) {
            log_file_ << "[" << now << "] === REALSENSE MONITOR SHUTDOWN INITIATED ===\n";
            log_file_ << "[" << now << "] Current frame count: " << frame_count_.load() << "\n";
            log_file_ << "[" << now << "] Current error count: " << error_count_.load() << "\n";
            log_file_ << "[" << now << "] Last FPS: " << last_fps_.load() << "\n";
            log_file_.flush();
        }
    }

    void _logDeviceShutdownStatus() {
        auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

        std::lock_guard<std::mutex> lock(log_mutex_);
        if (log_file_.is_open()) {
            log_file_ << "[" << now << "] === DEVICE SHUTDOWN STATUS ===\n";

            for (uint32_t i = 0; i < devices_.size(); ++i) {
                try {
                    auto& device = devices_[i];
                    if (device) {
                        std::string name = device.get_info(RS2_CAMERA_INFO_NAME);
                        std::string serial = device.get_info(RS2_CAMERA_INFO_SERIAL_NUMBER);
                        log_file_ << "[" << now << "] Device " << i << " (" << name << ", S/N: " << serial << ") - CONNECTED\n";

                        // Check if any sensors are still active
                        auto sensors = device.query_sensors();
                        for (uint32_t j = 0; j < sensors.size(); ++j) {
                            try {
                                std::string sensor_name = sensors[j].get_info(RS2_CAMERA_INFO_NAME);
                                log_file_ << "[" << now << "]   Sensor " << j << " (" << sensor_name << ") - AVAILABLE\n";
                            } catch (const std::exception& e) {
                                log_file_ << "[" << now << "]   Sensor " << j << " - ERROR: " << e.what() << "\n";
                            }
                        }
                    } else {
                        log_file_ << "[" << now << "] Device " << i << " - DISCONNECTED\n";
                    }
                } catch (const std::exception& e) {
                    log_file_ << "[" << now << "] Device " << i << " - ERROR during shutdown check: " << e.what() << "\n";
                }
            }

            log_file_.flush();
        }
    }

    void _logShutdownComplete() {
        auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

        if (log_file_.is_open()) {
            log_file_ << "[" << now << "] === REALSENSE MONITOR SHUTDOWN COMPLETE ===\n";
            log_file_ << "[" << now << "] All monitoring threads stopped\n";
            log_file_ << "[" << now << "] All resources cleaned up\n";
            log_file_ << "[" << now << "] Log file will be closed\n";
            log_file_.flush();
        }
    }

    void _logFinalStats() {
        auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        auto total_uptime = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - start_time_).count();

        if (log_file_.is_open()) {
            log_file_ << "[" << now << "] === FINAL STATISTICS ===\n";
            log_file_ << "[" << now << "] Total uptime: " << total_uptime << " seconds\n";
            log_file_ << "[" << now << "] Total frames: " << frame_count_.load() << "\n";
            log_file_ << "[" << now << "] Total errors: " << error_count_.load() << "\n";

            if (total_uptime > 0) {
                double avg_fps = static_cast<double>(frame_count_.load()) / total_uptime;
                log_file_ << "[" << now << "] Average FPS: " << std::fixed << std::setprecision(2) << avg_fps << "\n";
            }

            if (frame_count_.load() > 0) {
                double error_rate = (static_cast<double>(error_count_.load()) / frame_count_.load()) * 100.0;
                log_file_ << "[" << now << "] Error rate: " << std::fixed << std::setprecision(2) << error_rate << "%\n";
            }

            log_file_ << "[" << now << "] Final temperature: " << temperature_.load() << "°C\n";
            log_file_ << "[" << now << "] Final average latency: " << avg_latency_.load() << "ms\n";

            log_file_.flush();
        }
    }

    void _logRecordingEvent(const std::string& event_type, const std::string& details) {
        auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

        std::lock_guard<std::mutex> lock(log_mutex_);
        if (log_file_.is_open()) {
            log_file_ << "[" << now << "] RECORDING_" << event_type << ": " << details << "\n";
            log_file_.flush();
        }
    }

    void _logRecordingAnalysis() {
        auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

        // Calculate recording duration
        auto recording_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            recording_stop_time_ - recording_start_time_).count();

        std::lock_guard<std::mutex> lock(log_mutex_);
        if (log_file_.is_open()) {
            log_file_ << "[" << now << "] === DETAILED RECORDING ANALYSIS ===\n";
            log_file_ << "[" << now << "] Recording duration: " << recording_duration << "ms\n";
            log_file_ << "[" << now << "] Total frames captured: " << frame_count_.load() << "\n";
            log_file_ << "[" << now << "] Color frames: " << color_frame_count_.load() << "\n";
            log_file_ << "[" << now << "] Depth frames: " << depth_frame_count_.load() << "\n";
            log_file_ << "[" << now << "] Motion frames: " << motion_frame_count_.load() << "\n";
            log_file_ << "[" << now << "] Frame drops detected: " << frame_drops_.load() << "\n";
            log_file_ << "[" << now << "] Queue overflows: " << queue_overflows_.load() << "\n";
            log_file_ << "[" << now << "] Max temperature reached: " << max_temperature_.load() << "°C\n";
            log_file_ << "[" << now << "] Latency - Min: " << min_latency_.load() << "ms, Max: " << max_latency_.load() << "ms\n";

            // Calculate latency statistics
            {
                std::lock_guard<std::mutex> latency_lock(latency_mutex_);
                if (!latency_history_.empty()) {
                    double sum = 0.0;
                    for (double latency : latency_history_) {
                        sum += latency;
                    }
                    double avg = sum / latency_history_.size();

                    // Calculate standard deviation
                    double variance = 0.0;
                    for (double latency : latency_history_) {
                        variance += (latency - avg) * (latency - avg);
                    }
                    variance /= latency_history_.size();
                    double std_dev = std::sqrt(variance);

                    log_file_ << "[" << now << "] Latency analysis - Average: " << std::fixed << std::setprecision(2)
                              << avg << "ms, Std Dev: " << std_dev << "ms\n";

                    // Calculate percentiles
                    std::vector<double> sorted_latency = latency_history_;
                    std::sort(sorted_latency.begin(), sorted_latency.end());

                    size_t p50_idx = static_cast<size_t>(sorted_latency.size() * 0.5);
                    size_t p95_idx = static_cast<size_t>(sorted_latency.size() * 0.95);
                    size_t p99_idx = static_cast<size_t>(sorted_latency.size() * 0.99);

                    log_file_ << "[" << now << "] Latency percentiles - P50: " << sorted_latency[p50_idx]
                              << "ms, P95: " << sorted_latency[p95_idx]
                              << "ms, P99: " << sorted_latency[p99_idx] << "ms\n";
                }
            }

            // Recording quality assessment
            double drop_rate = (frame_count_.load() > 0) ?
                (static_cast<double>(frame_drops_.load()) / frame_count_.load()) * 100.0 : 0.0;

            log_file_ << "[" << now << "] Recording quality metrics:\n";
            log_file_ << "[" << now << "]   Frame drop rate: " << std::fixed << std::setprecision(2) << drop_rate << "%\n";
            log_file_ << "[" << now << "]   Queue overflow rate: " << queue_overflows_.load() << " events\n";
            log_file_ << "[" << now << "]   Error rate: " << ((frame_count_.load() > 0) ?
                (static_cast<double>(error_count_.load()) / frame_count_.load()) * 100.0 : 0.0) << "%\n";

            // Quality assessment
            if (drop_rate < 1.0 && queue_overflows_.load() == 0 && error_count_.load() == 0) {
                log_file_ << "[" << now << "] RECORDING_QUALITY: EXCELLENT\n";
            } else if (drop_rate < 5.0 && queue_overflows_.load() < 10) {
                log_file_ << "[" << now << "] RECORDING_QUALITY: GOOD\n";
            } else if (drop_rate < 10.0) {
                log_file_ << "[" << now << "] RECORDING_QUALITY: ACCEPTABLE\n";
            } else {
                log_file_ << "[" << now << "] RECORDING_QUALITY: POOR\n";
            }

            log_file_.flush();
        }
    }
};