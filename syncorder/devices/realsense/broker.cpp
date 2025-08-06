#pragma once

#include <atomic>
#include <thread>
#include <chrono>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>

// installed
#include <librealsense2/rs.hpp>

// local
#include <syncorder/gonfig/gonfig.h>
#include <syncorder/error/exception.h>
#include <syncorder/devices/common/broker_base.h>
#include <syncorder/devices/realsense/model.h>


/**
 * @class Broker
 */

class RealsenseBroker : public TBBroker<RealsenseBufferData> {
private:
    std::ofstream csv_;
    std::string output_;

public:
    RealsenseBroker() {
        output_ = gonfig.output_path + "realsense/";

        std::filesystem::create_directories(output_);
    }

    ~RealsenseBroker() {}

public:
    void cleanup() {}

protected:
    void _process(const RealsenseBufferData& data) override {}
};