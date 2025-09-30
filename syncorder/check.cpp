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

    // gonfig
    gonfig = Config::parseArgs(argc, argv);
    
    // Set up
    char tempPath[MAX_PATH];
    GetTempPathA(MAX_PATH, tempPath);
    std::string output_path = std::string(tempPath) + "syncorder\\";
    
    // Clean up
    if (GetFileAttributesA(output_path.c_str()) != INVALID_FILE_ATTRIBUTES) {
        std::string cleanup_cmd = "rmdir /s /q \"" + output_path + "\" 2>nul";
        system(cleanup_cmd.c_str());
    }
    
    gonfig.output_path = output_path;
    std::cout << "[INFO] Using temp directory: " << gonfig.output_path << "\n";
    
    gonfig.record_duration = 5;
    std::cout << "[INFO] test 5s: " << gonfig.record_duration << "\n";

    try {
        Syncorder syncorder;

        /**
         * ::Initalize
         */
        syncorder.setTimeout(std::chrono::milliseconds(10000));
        syncorder.addDevice(std::make_unique<RealsenseManager>(0));
        syncorder.addDevice(std::make_unique<TobiiManager>(0));
        
        /**
         * ::Setup()
         */
        if (!syncorder.executeSetup()) return -1;

        /**
         * ::Warmup()
         */
        if (!syncorder.executeWarmup()) return -1;
        std::this_thread::sleep_for(std::chrono::seconds(3));

        /**
         * ::Start()
         */
        if (!syncorder.executeStart()) return -1;
        
        /**
         * ::Stop()
         */
        for (int i = gonfig.record_duration; i > 0 && !should_exit; --i) {
            std::cout << "  " << i << " seconds remaining...\r" << std::flush;

            for (int j = 0; j < 10 && !should_exit; ++j) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                if (stopEvent && WaitForSingleObject(stopEvent, 0) == WAIT_OBJECT_0) {
                    std::cout << "\n[INFO] External stop signal received via Named Event\n";
                    should_exit = true;
                    break;
                }
            }
        }

        if (should_exit) {
            std::cout << "\n[INFO] Early termination requested. Stopping recording...\n";
        } else {
            std::cout << "\n[INFO] Recording duration completed. Stopping recording...\n";
        }

        std::cout << "[INFO] Executing stop sequence...\n";
        syncorder.executeStop();
        std::cout << "[INFO] Executing cleanup sequence...\n";
        syncorder.executeCleanup();

        /**
         * ::Verify()
         */
        std::cout << "[INFO] Starting verify phase...\n";
        if(!syncorder.executeVerify()) {
            std::cout << "[ERROR] Verify failed\n";
            return -1;
        }
        std::cout << "[INFO] Verify completed successfully\n";

    } catch (const std::exception& e) {
        std::cout << "[ERROR] Main.cpp error: " << e.what() << "\n";
        if (stopEvent) CloseHandle(stopEvent);
        return -1;
    }

    // Cleanup
    if (stopEvent) CloseHandle(stopEvent);
    return 0;
}