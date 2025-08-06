#pragma once

#include <algorithm>
#include <any>

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
#include <syncorder/error/exception.h>
#include <syncorder/devices/common/device_base.h>


/**
 * @class Device
 */

class CameraDevice : public BDevice {
private:
    ComPtr<IMFActivate> device_;
    ComPtr<IMFSourceReader> reader_;
    
    std::any callback_;

public:
    CameraDevice(int device_id = 0)
    :
        BDevice(device_id)
    {}
    
    ~CameraDevice() {
        cleanup();
    }

public:      
    bool _setup() override {
        _startMF();

        device_ = _createDevice();
        reader_ = _createSourceReader();

        return true;
    }

    bool pre_setup(std::any callback) {
        callback_ = callback;

        return true;
    }
    
    bool _warmup() override {
        _readSource();

        return true;
    }
    
    bool _start() override {
        return true;
    }
    
    bool _stop() override {
        return true;
    }
    
    bool _cleanup() override {
        reader_.Reset();
        device_.Reset();
        
        _endMF();

        return true;
    }

    // get
    ComPtr<IMFSourceReader> getReader() {
        return reader_;
    }

private:
    void _startMF() {
        HRESULT hr = MFStartup(MF_VERSION);
        if (FAILED(hr)) throw CameraDeviceError("MFStartup failed");
    }
    
    void _endMF() {
        MFShutdown();
    }
    
    ComPtr<IMFActivate> _createDevice() {
        HRESULT hr = S_OK;

        ComPtr<IMFAttributes> attributes;
        
        IMFActivate** devices_raw = nullptr;
        UINT32 device_count = 0;

        hr = MFCreateAttributes(&attributes, 1);
        if (FAILED(hr)) throw CameraDeviceError("Device attributes creation failed");

        hr = attributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
        if (FAILED(hr)) throw CameraDeviceError("Device attributes setup failed");

        // every device
        hr = MFEnumDeviceSources(attributes.Get(), &devices_raw, &device_count);
        if (FAILED(hr)) throw CameraDeviceError("Device enumeration failed");

        if (device_id_ < 0 || device_id_ >= static_cast<int>(device_count)) {
            CoTaskMemFree(devices_raw);
            throw CameraDeviceError("Device index " + std::to_string(device_id_) + " out of range (0-" + std::to_string(device_count-1) + ")");
        }
        
        IMFActivate* target_device = devices_raw[device_id_];
        ComPtr<IMFActivate> selected_device = target_device;

        // validation
        BOOL match = FALSE;
        bool is_valid = std::any_of(
            devices_raw, devices_raw + device_count,
            [&](IMFActivate* dev) {
                match = FALSE;
                return SUCCEEDED(target_device->Compare(dev, MF_ATTRIBUTES_MATCH_INTERSECTION, &match)) && match;
            }
        );

        CoTaskMemFree(devices_raw);

        if (!is_valid) throw CameraDeviceError("Target device not matched in enumeration");

        return selected_device;
    }
    
    ComPtr<IMFSourceReader> _createSourceReader() {
        HRESULT hr = S_OK;
        
        ComPtr<IMFMediaSource> source;
        ComPtr<IMFAttributes> attributes;
        ComPtr<IMFSourceReader> reader;
        ComPtr<IMFMediaType> type;

        hr = device_->ActivateObject(IID_PPV_ARGS(&source));
        if (FAILED(hr)) throw CameraDeviceError("Device activation failed");
        
        hr = MFCreateAttributes(&attributes, 1);
        if (FAILED(hr)) throw CameraDeviceError("Reader attributes creation failed");
        
        // std::any -> COM
        auto raw_callback = std::any_cast<IUnknown*>(callback_);

        hr = attributes->SetUnknown(MF_SOURCE_READER_ASYNC_CALLBACK, raw_callback);
        if (FAILED(hr)) throw CameraDeviceError("Callback setup failed");
        
        hr = MFCreateSourceReaderFromMediaSource(source.Get(), attributes.Get(), &reader);
        if (FAILED(hr)) throw CameraDeviceError("SourceReader creation failed");

        hr = MFCreateMediaType(&type);
        if (FAILED(hr)) throw CameraDeviceError("Type creation failed");

        type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_MJPG); //TODO: Config
        type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        type->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);

        MFSetAttributeSize(type.Get(), MF_MT_FRAME_SIZE, 1280, 720); //TODO: Config
        MFSetAttributeRatio(type.Get(), MF_MT_FRAME_RATE, 30, 1); //TODO: Config

        hr = reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, NULL, type.Get());
        if (FAILED(hr)) {
            std::cout << "Type setting failed. HRESULT = 0x" << std::hex << hr;
            throw CameraDeviceError("Type setting failed\n");
        }

        return reader;
    }

    void _readSource() {
        if (!callback_.has_value()) {
            throw CameraDeviceError("Callback not set before warmup");
        }
        
        HRESULT hr = reader_->ReadSample(
            MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, nullptr, nullptr, nullptr, nullptr
        );
        
        if (FAILED(hr)) {
            throw CameraDeviceError("Failed to start reading samples. HRESULT: " + std::to_string(hr));
        }
    }
};