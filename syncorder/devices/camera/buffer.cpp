#pragma once

#include <array>
#include <atomic>
#include <optional>
#include <iostream>

// installed
#include <windows.h>
#include <wrl/client.h>
#include <mfobjects.h>

using namespace Microsoft::WRL;

// local
#include <syncorder/error/exception.h>
#include <syncorder/devices/common/buffer_base.h>
#include <syncorder/devices/camera/model.h>


/**
 * @class Buffer
 */

constexpr std::size_t CAMERA_RING_BUFFER_SIZE = 1024;
class CameraBuffer 
:
    public BBuffer<CameraBufferData, CAMERA_RING_BUFFER_SIZE> 
{
public:
    static void* dequeue(void* instance) {
        auto* buffer = static_cast<CameraBuffer*>(instance);        
        auto result = buffer->_dequeue();
        if (!result.has_value()) return nullptr;

        return new CameraBufferData(std::move(result.value()));
    }
    
protected:
    void onOverflow() noexcept override { std::cout << "[CameraBuffer Warning] Buffer overflow\n"; }
};