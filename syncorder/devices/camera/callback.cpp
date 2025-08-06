#pragma once

// installed
#include <windows.h>
#include <wrl/client.h>
#include <wrl/implements.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfobjects.h>
#include <mfreadwrite.h>
#include <mferror.h>

#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "ole32.lib")

using namespace Microsoft::WRL;

// local
#include <syncorder/devices/camera/buffer.cpp> //TODO: include buffer
#include <syncorder/error/exception.h>


/**
 * @class Callback
 */

class CameraCallback : public RuntimeClass<RuntimeClassFlags<ClassicCom>, IMFSourceReaderCallback> {
private:
    ComPtr<IMFSourceReader> reader_;
    void* buffer_;

    // flag
    std::atomic<bool> first_frame_received_;

public:
    CameraCallback() {}
    ~CameraCallback() {}
    
public:
    void setup(ComPtr<IMFSourceReader> reader, void* buffer) {
        reader_ = reader;
        buffer_ = buffer;

        first_frame_received_.store(false);        
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
        
        std::cout << "[Camera] warmup clear\n";

        return true;
    }

    // get
    IUnknown* getIUnknown() {
        return static_cast<IUnknown*>(this);
    }

public:
    HRESULT STDMETHODCALLTYPE OnReadSample(HRESULT hr, DWORD, DWORD, LONGLONG timestamp, IMFSample* sample) override {
        // flag
        if (!first_frame_received_.load()) first_frame_received_.store(true);

        // data
        if (buffer_ && sample) {
            CameraBufferData data = _map(sample, timestamp);

            auto* cam_buffer = static_cast<CameraBuffer*>(buffer_);
            cam_buffer->enqueue(std::move(data));
        }
        
        // loop
        reader_->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, nullptr, nullptr, nullptr, nullptr);

        // return
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnEvent(DWORD, IMFMediaEvent*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnFlush(DWORD) override { return S_OK; }

private: 
    CameraBufferData _map(IMFSample* sample, LONGLONG ts) {
        return CameraBufferData(
            ComPtr<IMFSample>(sample),
            std::chrono::system_clock::now(),
            ts
        );
    }
};