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
#include <syncorder/devices/tobii/device.cpp>
#include <syncorder/devices/tobii/manager.cpp>
#include <syncorder/devices/realsense/device.cpp>
#include <syncorder/devices/realsense/manager.cpp>
#include <syncorder/monitoring/cpu_monitor.h>
#include <syncorder/monitoring/realsense_monitor.h>

// shut down
std::atomic<bool> should_exit{false};

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

    try {

        /**
         * ::Initalize
         */
        CpuMonitor cpu_monitor;
        cpu_monitor.start();

        Syncorder syncorder;
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
            std::this_thread::sleep_for(std::chrono::seconds(1));
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


        cpu_monitor.stop();
        
    } catch (const std::exception& e) {
        std::cout << "[ERROR] Main.cpp error: " << e.what() << "\n";
        return -1;
    }
    
    return 0;
}