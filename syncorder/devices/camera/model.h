#pragma once

#include <chrono>
#include <windows.h>
#include <wrl/client.h>
#include <mfobjects.h>

using namespace Microsoft::WRL;

/**
 * @struct
 */

struct CameraBufferData {
    // sample
    ComPtr<IMFSample> sample_;
    
    // time
    std::chrono::system_clock::time_point sys_time_;
    LONGLONG mf_ts_;
    
    // metadata
    DWORD stream_index_;
    DWORD flags_;

public:
    CameraBufferData() {}
    CameraBufferData(
        ComPtr<IMFSample> sample,
        std::chrono::system_clock::time_point sys_time,
        LONGLONG mf_ts,
        DWORD stream_index = 0,
        DWORD flags = 0
    ) {
        sample_ = std::move(sample);
        sys_time_ = sys_time;
        mf_ts_ = mf_ts;
    }
};