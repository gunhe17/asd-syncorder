#pragma once

#include <iostream>
#include <stdexcept>
#include <string>


/**
 * @macro EXCEPTION - Exception handling wrapper
 */

#define EXCEPTION(...)                                  \
    try {                                               \
        __VA_ARGS__                                     \
    } catch (const std::exception& e) {                 \
        std::cout << "[Error] " << e.what() << "\n";    \
        return false;                                   \
    }

/**
 * @class DeviceError - Base device error class
 */

class DeviceError : public std::runtime_error {
public:
    DeviceError(const std::string& message, int code = 1000) 
    : 
        std::runtime_error(message), 
        code_(code) 
    {}
private:
    int code_;
};

class ArducamDeviceError : public std::runtime_error {
public:
    ArducamDeviceError(const std::string& msg) : std::runtime_error("Device Ardu: " + msg) {}
};

class CameraDeviceError : public std::runtime_error {
public:
    CameraDeviceError(const std::string& msg) : std::runtime_error("Device Camera: " + msg) {}
};

class RealsenseDeviceError : public std::runtime_error {
public:
    RealsenseDeviceError(const std::string& msg) : std::runtime_error("Device Realsense: " + msg) {}
};

class TobiiDeviceError : public std::runtime_error {
public:
    TobiiDeviceError(const std::string& msg) : std::runtime_error("Device Tobii: " + msg) {}
};