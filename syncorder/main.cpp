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

// shut down
std::atomic<bool> should_exit{false};

void signal_handler(int signal) {
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

        if (should_exit) {} // TODO: stop to cleanup()

        syncorder.executeStop();
        syncorder.executeCleanup();


        std::cout << "[LOG] Program complete\n";
        
    } catch (const std::exception& e) {
        std::cout << "[ERROR] Main.cpp error: " << e.what() << "\n";
        return -1;
    }
    
    return 0;
}