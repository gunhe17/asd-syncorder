#pragma once

// installed
#include "tobii_research.h"
#include "tobii_research_eyetracker.h"
#include "tobii_research_streams.h"

// local
#include <syncorder/devices/tobii/buffer.cpp> //TODO: include buffer
#include <syncorder/error/exception.h>


/**
 * @class Callback
 */

class TobiiCallback {
private:
    static inline TobiiCallback* instance_ = nullptr;
    void* buffer_;

    // flag
    std::atomic<bool> first_frame_received_;

public:
    TobiiCallback() {}
    ~TobiiCallback() {}

public:
    void setup(void* buffer) {
        instance_ = this;
        buffer_ = buffer;
    }

    bool warmup() {
        auto start = std::chrono::steady_clock::now();
        auto end = std::chrono::milliseconds(10000);
        
        while (!first_frame_received_.load()) {
            auto elapsed = std::chrono::steady_clock::now() - start;
            if (elapsed >= end) {
                std::cout << "[ERROR] warmup timeout\n";
                return false;
            }
        }

        return true;
    }

    static void onGaze(TobiiResearchGazeData* gaze_data, void* user_data) {
        auto* callback_instance = static_cast<TobiiCallback*>(user_data);
        if (callback_instance) {
            callback_instance->_onGaze(gaze_data);
        }
    }

private:
    void _onGaze(TobiiResearchGazeData* gaze_data) {
        if (!first_frame_received_.load()) first_frame_received_.store(true);
        if (!gaze_data || !buffer_) return;

        auto* tobii_buffer = static_cast<TobiiBuffer*>(buffer_);
        TobiiBufferData data = _map(gaze_data);
        tobii_buffer->enqueue(std::move(data));
    }

private:
    TobiiBufferData _map(TobiiResearchGazeData* gaze_data) {
        TobiiBufferData data = {};

        data.gazed = *gaze_data;
            
        return data;
    }
};