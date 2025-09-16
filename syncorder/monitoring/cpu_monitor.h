#pragma once

#include <thread>
#include <atomic>
#include <iostream>
#include <chrono>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <windows.h>
#include <pdh.h>
#include <psapi.h>
#include "../gonfig/gonfig.h"

#pragma comment(lib, "pdh.lib")
#pragma comment(lib, "psapi.lib")

class CpuMonitor {
private:
    std::thread monitor_thread_;
    std::atomic<bool> running_{false};
    std::ofstream log_file_;

    PDH_HQUERY query_;
    PDH_HCOUNTER cpu_counter_;
    PDH_HCOUNTER memory_counter_;

public:
    void start() {
        if (running_) {
            return;
        }

        if (_initPerfCounters()) {
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            std::string log_path = gonfig.output_path + "cpu_monitor_" + std::to_string(time_t) + ".log";

            std::filesystem::create_directories(gonfig.output_path);
            log_file_.open(log_path, std::ios::out | std::ios::app);

            if (log_file_.is_open()) {
                running_ = true;
                monitor_thread_ = std::thread(&CpuMonitor::_monitor, this);
            } else {
                std::cout << "[ERROR] Failed to create CPU monitor log file: " << log_path << "\n";
            }
        } else {
            std::cout << "[ERROR] Failed to initialize performance counters for CPU monitor\n";
        }
    }

    void stop() {
        if (!running_) {
            std::cout << "[INFO] CPU monitor already stopped\n";
            return;
        }

        running_ = false;

        if (monitor_thread_.joinable()) {
            monitor_thread_.join();
            std::cout << "[INFO] CPU monitor thread joined\n";
        }

        if (log_file_.is_open()) {
            log_file_.close();
            std::cout << "[INFO] CPU monitor log file closed\n";
        }

        _cleanupPerfCounters();
    }

    ~CpuMonitor() {
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

            auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

            if (log_file_.is_open()) {
                log_file_ << "[" << now << "] CPU: " << cpu_value.doubleValue << "% | "
                         << "Memory: " << (mem_status.ullTotalPhys - mem_status.ullAvailPhys) / (1024 * 1024) << " MB used / "
                         << mem_status.ullTotalPhys / (1024 * 1024) << " MB total ("
                         << mem_status.dwMemoryLoad << "%)" << std::endl;
                log_file_.flush();

                // Also log to main logger occasionally
                static int counter = 0;
                if (++counter % 30 == 0) { // Every 30 seconds
                    std::ostringstream oss;
                    oss << "CPU: " << std::fixed << std::setprecision(1) << cpu_value.doubleValue
                        << "%, Memory: " << mem_status.dwMemoryLoad << "% ("
                        << (mem_status.ullTotalPhys - mem_status.ullAvailPhys) / (1024 * 1024) << "MB used)";
                    std::cout << "[LOG] " << oss.str() << "\n";
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