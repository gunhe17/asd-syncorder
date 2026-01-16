#pragma once

#include <iostream>
#include <chrono>
#include <thread>
#include <signal.h>
#include <atomic>
#include <windows.h>

// local
#include <syncorder/gonfig/gonfig.h>
#include <syncorder/error/exception.h>
#include <syncorder/core/syncorder.cpp>
#include <syncorder/devices/tobii/device.cpp>
#include <syncorder/devices/tobii/manager.cpp>
#include <syncorder/devices/realsense/device.cpp>
#include <syncorder/devices/realsense/manager.cpp>
#include <syncorder/monitoring/cpu_monitor.h>
#include <syncorder/monitoring/realsense_monitor.h>

// shut down
std::atomic<bool> should_exit{false};
HANDLE stopEvent = nullptr;

void signal_handler(int signal) {
    std::cout << "\n[INFO] Signal " << signal << " received. Initiating graceful shutdown...\n";
    should_exit = true;
}


/**
 * @main
 */

int main(int argc, char* argv[]) {
    // shut down
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);

    // Open Named Event for external stop signal
    stopEvent = OpenEventW(EVENT_ALL_ACCESS, FALSE, L"Global\\SyncorderStopEvent");
    if (stopEvent) {
        std::cout << "[INFO] Named Event connection established\n";
    }

    // gonfig
    gonfig = Config::parseArgs(argc, argv);

    try {
        Syncorder syncorder;

        /**
         * ::Initalize
         */
        syncorder.setTimeout(std::chrono::milliseconds(10000));
        syncorder.addDevice(std::make_unique<RealsenseManager>(0, false));
        syncorder.addDevice(std::make_unique<TobiiManager>(0, false));

        /**
         * ::Verify()
         */
        std::cout << "[INFO] Starting verify phase...\n";
        if(!syncorder.executeVerify()) {
            std::cout << "[ERROR] Verify failed\n";
            return 1;
        }
        std::cout << "[INFO] Verify completed successfully\n";

        return 0;

    } catch (const std::exception& e) {
        std::cout << "[ERROR] Main.cpp error: " << e.what() << "\n";
        if (stopEvent) CloseHandle(stopEvent);
        return -1;
    }

    // Cleanup
    if (stopEvent) CloseHandle(stopEvent);
    return 0;
}