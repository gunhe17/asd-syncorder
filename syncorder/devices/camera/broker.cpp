#include <any>
#include <atomic>
#include <thread>
#include <chrono>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <sstream>

// local
#include <syncorder/error/exception.h>
#include <syncorder/devices/common/broker_base.h>
#include <syncorder/devices/camera/model.h>


/**
 * @class Broker
 */

class CameraBroker : public TBBroker<CameraBufferData> {
private:
    std::ofstream csv_;

public:
    CameraBroker() {
        csv_.open("camera_data.csv");
        csv_<< "system_time,media_foundation_timestamp\n";
    }
    ~CameraBroker() {}

protected:
    void _process(const CameraBufferData& data) override {
        _write(data);

        std::cout << "[CameraBroker] Processing timestamp\n";
    }

private:
    void _write(const CameraBufferData& data) {
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            data.sys_time_.time_since_epoch()
        ).count();
       
        csv_ 
        << ms << ","
        << data.mf_ts_ << "\n";
    }
};