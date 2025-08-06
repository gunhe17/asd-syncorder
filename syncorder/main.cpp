#pragma once

#include <iostream>
#include <chrono>
#include <thread>
#include <signal.h>
#include <atomic>

// local
#include <syncorder/gonfig/gonfig.h>
#include <syncorder/error/exception.h>
#include <syncorder/syncorder.cpp>
#include <syncorder/devices/camera/device.cpp>
#include <syncorder/devices/camera/manager.cpp>
#include <syncorder/devices/tobii/device.cpp>
#include <syncorder/devices/tobii/manager.cpp>
#include <syncorder/devices/realsense/device.cpp>
#include <syncorder/devices/realsense/manager.cpp>

// shut down
std::atomic<bool> should_exit{false};

void signal_handler(int signal) {
    should_exit = true;
    std::cout << "\nShutdown signal received. Cleaning up...\n";
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

    try {
        std::cout << "=== asd-syncorder Multi-Device Recording ===\n\n";
        
        // asd-syncorder 초기화
        Syncorder syncorder;
        syncorder.setTimeout(std::chrono::milliseconds(10000));
        
        // Device 등록
        std::cout << "Registering devices...\n";
        syncorder.addDevice(std::make_unique<RealsenseManager>(0));
        syncorder.addDevice(std::make_unique<TobiiManager>(0));
        std::cout << "Registered " << syncorder.getDeviceCount() << " devices\n\n";
        
        
        /**
         * ::Setup()
         */
        std::cout << "Initializing devices...\n";
        if (!syncorder.executeSetup()) {
            std::cout << "(X) Initialization failed\n";
            return -1;
        }
        std::cout << "(O) Devices initialized\n\n";
        

        /**
         * ::Warmup()
         */
        std::cout << "Preparing devices...\n";
        if (!syncorder.executeWarmup()) {
            std::cout << "(X) Preparation failed\n";
            return -1;
        }
        std::this_thread::sleep_for(std::chrono::seconds(3));
        std::cout << "(O) Devices ready\n\n";

        
        /**
         * ::Start()
         */
        std::cout << "Starting recording...\n";
        if (!syncorder.executeStart()) {
            std::cout << "(X) Recording start failed\n";
            return -1;
        }
        std::cout << "(O) Recording started\n\n";
        

        /**
         * ::Stop()
         */
        std::cout << "* Recording in progress (1 seconds)...\n";
        for (int i = gonfig.record_duration; i > 0 && !should_exit; --i) {
            std::cout << "  " << i << " seconds remaining...\r" << std::flush;
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        std::cout << "\n";

        // TODO: stop to cleanup()
        if (should_exit) {
            std::cout << "Recording interrupted by signal\n";
        }

        std::cout << "Stopping recording...\n";
        syncorder.executeStop();
        syncorder.executeCleanup();
        std::cout << "(O) Recording completed successfully\n\n";
        
        std::cout << ":) All operations completed successfully!\n";
        
    } catch (const std::exception& e) {
        std::cout << "(X) Fatal error: " << e.what() << "\n";
        return -1;
    }
    
    return 0;
}