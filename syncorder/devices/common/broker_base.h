#pragma once

#include <thread>
#include <chrono>

// installed
#include <librealsense2/rs.hpp> // *timestamp 변환을 위한 include


/**
 * @class Helper
 */


/**
 * @class Base Broker
 */

class BBroker {
protected:
    // buffer
    void* buffer_;
    void* dequeue_;

    // flag
    std::atomic<bool> running_;
    std::atomic<int> processed_count_;

    std::thread processing_thread_;

public:
    BBroker() 
    : 
        running_(false), 
        processed_count_(0) 
    {}
    
    virtual ~BBroker() { stop(); }

public:
    void setup(void* buffer, void* dequeue) {
        buffer_ = buffer;
        dequeue_ = dequeue;
    }

    void start() {
        // flag
        running_ = true;

        // thread
        processing_thread_ = std::thread(&BBroker::_loop, this);
    }

    void stop() {
        // flag
        running_ = false;

        // thread
        if (processing_thread_.joinable()) processing_thread_.join();
    }

protected:
    virtual void _broker() = 0;

private:
    void _loop() {
        while (running_) _broker();
    }
};

template<typename DataType>
class TBBroker : public BBroker {
protected:
    void _broker() override {
        if (!buffer_ || !dequeue_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            return;
        }

        typedef void* (*DequeueFunc)(void*);
        auto dequeue_func = reinterpret_cast<DequeueFunc>(dequeue_);
        void* raw_data = dequeue_func(buffer_);
        
        if (raw_data != nullptr) {
            processed_count_++;
            
            std::unique_ptr<DataType> data(static_cast<DataType*>(raw_data));
            _process(*data);
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

protected:
    virtual void _process(const DataType& data) = 0;
};