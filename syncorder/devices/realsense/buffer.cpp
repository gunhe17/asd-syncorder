#pragma once

#include <chrono>
#include <array>
#include <atomic>
#include <optional>
#include <iostream>
#include <memory>

// installed
#include <librealsense2/rs.hpp>

// local
#include <syncorder/error/exception.h>
#include <syncorder/devices/common/buffer_base.h>
#include <syncorder/devices/realsense/model.h>


/**
 * @class Buffer
 */

constexpr std::size_t REALSENSE_RING_BUFFER_SIZE = 1024;

class RealsenseBuffer : public BBuffer<RealsenseBufferData, REALSENSE_RING_BUFFER_SIZE> {
    public:
    static void* dequeue(void* instance) {
        auto* buffer = static_cast<RealsenseBuffer*>(instance);
        auto result = buffer->_dequeue();
        if (!result.has_value()) return nullptr;
        
        return new RealsenseBufferData(std::move(result.value()));
    }
protected:
    void onOverflow() noexcept override { std::cout << "[RealsenseBuffer Warning] Buffer overflow\n"; }
};