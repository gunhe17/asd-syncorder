#pragma once

// Standard library includes
#include <thread>
#include <atomic>
#include <iostream>
#include <chrono>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <iomanip>

// Windows API includes
#include <windows.h>
#include <pdh.h>
#include <psapi.h>

// Project includes
#include "../gonfig/gonfig.h"

#pragma comment(lib, "pdh.lib")
#pragma comment(lib, "psapi.lib")

/**
 * @brief CPU and RAM usage monitor
 *
 * Monitors system CPU and RAM usage at regular intervals and logs the data to a file.
 * Uses Windows Performance Data Helper (PDH) API for accurate measurements.
 */
class CpuRamMonitor {
private:
    // Constants
    static constexpr int MONITORING_INTERVAL_SECONDS = 1;
    static constexpr int CONSOLE_LOG_INTERVAL_SECONDS = 30;
    static constexpr int BYTES_TO_MB_DIVISOR = 1024 * 1024;

    // Thread management
    std::thread monitor_thread_;
    std::atomic<bool> running_{false};

    // File logging
    std::ofstream log_file_;

    // Performance counters
    PDH_HQUERY query_;
    PDH_HCOUNTER cpu_counter_;
    PDH_HCOUNTER memory_counter_;

public:
    /**
     * @brief Start monitoring CPU and RAM usage
     *
     * Initializes performance counters, creates log file, and starts monitoring thread.
     * If already running, this method returns without doing anything.
     */
    void start() {
        // Prevent starting if already running
        if (running_) {
            return;
        }

        // Initialize Windows performance counters
        if (!_initPerfCounters()) {
            std::cout << "[ERROR] Failed to initialize performance counters for CPU/RAM monitor\n";
            return;
        }

        // Create log file with timestamp
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::system_clock::to_time_t(now);
        std::string log_path = gonfig.output_path + "cpu_ram_monitor_"
                             + std::to_string(timestamp) + ".log";

        // Ensure output directory exists
        std::filesystem::create_directories(gonfig.output_path);
        log_file_.open(log_path, std::ios::out | std::ios::app);

        if (!log_file_.is_open()) {
            std::cout << "[ERROR] Failed to create CPU/RAM monitor log file: " << log_path << "\n";
            _cleanupPerfCounters();
            return;
        }

        // Write CSV header
        log_file_ << "Time,CPU(%),RAM_Used(MB),RAM_Total(MB),RAM(%)\n";

        // Start monitoring thread
        running_ = true;
        monitor_thread_ = std::thread(&CpuRamMonitor::_monitor, this);
    }

    /**
     * @brief Stop monitoring CPU and RAM usage
     *
     * Stops the monitoring thread, closes log file, and cleans up performance counters.
     * Safe to call multiple times.
     */
    void stop() {
        // Check if already stopped
        if (!running_) {
            std::cout << "[INFO] CPU/RAM monitor already stopped\n";
            return;
        }

        // Signal monitoring thread to stop
        running_ = false;

        // Wait for monitoring thread to finish
        if (monitor_thread_.joinable()) {
            monitor_thread_.join();
            std::cout << "[INFO] CPU/RAM monitor thread joined\n";
        }

        // Close log file
        if (log_file_.is_open()) {
            log_file_.close();
            std::cout << "[INFO] CPU/RAM monitor log file closed\n";
        }

        // Clean up Windows performance counters
        _cleanupPerfCounters();
    }

    /**
     * @brief Destructor - ensures monitoring is stopped
     */
    ~CpuRamMonitor() {
        stop();
    }

private:
    void _monitor() {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        while (running_) {
            PdhCollectQueryData(query_);

            PDH_FMT_COUNTERVALUE cpu_value;
            PDH_FMT_COUNTERVALUE memory_value;

            PdhGetFormattedCounterValue(cpu_counter_, PDH_FMT_DOUBLE, NULL, &cpu_value);
            PdhGetFormattedCounterValue(memory_counter_, PDH_FMT_DOUBLE, NULL, &memory_value);

            MEMORYSTATUSEX mem_status;
            mem_status.dwLength = sizeof(mem_status);
            GlobalMemoryStatusEx(&mem_status);

            auto now = std::chrono::system_clock::now();
            auto time_t_now = std::chrono::system_clock::to_time_t(now);
            auto local_time = std::localtime(&time_t_now);

            ULONGLONG ram_used_mb = (mem_status.ullTotalPhys - mem_status.ullAvailPhys) / (1024 * 1024);
            ULONGLONG ram_total_mb = mem_status.ullTotalPhys / (1024 * 1024);

            if (log_file_.is_open()) {
                // Write readable timestamp instead of epoch time
                log_file_ << std::put_time(local_time, "%Y-%m-%d %H:%M:%S") << ","
                         << std::fixed << std::setprecision(1) << cpu_value.doubleValue << ","
                         << ram_used_mb << ","
                         << ram_total_mb << ","
                         << mem_status.dwMemoryLoad << "\n";
                log_file_.flush();

                // Also log to console occasionally
                static int counter = 0;
                if (++counter % 30 == 0) { // Every 30 seconds
                    std::cout << "[LOG] CPU: " << std::fixed << std::setprecision(1) << cpu_value.doubleValue
                              << "%, RAM: " << mem_status.dwMemoryLoad << "% ("
                              << ram_used_mb << "/" << ram_total_mb << " MB)\n";
                }
            }

            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    bool _initPerfCounters() {
        if (PdhOpenQuery(NULL, NULL, &query_) != ERROR_SUCCESS) {
            std::cout << "[ERROR] Failed to open PDH query\n";
            return false;
        }

        if (PdhAddCounterA(query_, "\\Processor(_Total)\\% Processor Time", NULL, &cpu_counter_) != ERROR_SUCCESS) {
            std::cout << "[ERROR] Failed to add CPU counter\n";
            return false;
        }

        if (PdhAddCounterA(query_, "\\Memory\\Available MBytes", NULL, &memory_counter_) != ERROR_SUCCESS) {
            std::cout << "[ERROR] Failed to add memory counter\n";
            return false;
        }

        std::cout << "[INFO] Performance counters initialized successfully\n";
        return true;
    }

    void _cleanupPerfCounters() {
        if (query_) {
            PdhCloseQuery(query_);
            query_ = NULL;
        }
    }
};