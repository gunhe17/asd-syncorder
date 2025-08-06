#pragma once

#include <chrono>
#include <array>
#include <atomic>
#include <optional>
#include <iostream>

// installed
#include "tobii_research.h"

// local
#include <syncorder/error/exception.h>
#include <syncorder/devices/common/buffer_base.h>
#include <syncorder/devices/tobii/model.h>


/**
 * @class Buffer
 */

constexpr std::size_t TOBII_RING_BUFFER_SIZE = 2048;

class TobiiBuffer : public BBuffer<TobiiBufferData, TOBII_RING_BUFFER_SIZE> {
public:
    static void* dequeue(void* instance) {
        auto* buffer = static_cast<TobiiBuffer*>(instance);
        auto result = buffer->_dequeue();
        if (!result.has_value()) return nullptr;
        
        return new TobiiBufferData(std::move(result.value()));
    }
protected:
    void onOverflow() noexcept override {
        std::cout << "[TobiiBuffer Warning] Buffer overflow\n";
    }
};