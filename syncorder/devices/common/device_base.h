#pragma once

#include <syncorder/error/exception.h>

/**
 * @class
 */
class BDevice {
protected:
    int device_id_;

public:
    BDevice(int device_id) : device_id_(device_id) {}
    virtual ~BDevice() {}
    
    virtual bool setup() final { 
        EXCEPTION(return _setup();) 
    }
    
    virtual bool warmup() final { 
        EXCEPTION(return _warmup();) 
    }
    
    virtual bool start() final { 
        EXCEPTION(return _start();) 
    }
    
    virtual bool stop() final { 
        EXCEPTION(return _stop();) 
    }
    
    virtual bool cleanup() final { 
        EXCEPTION(return _cleanup();) 
    }

protected:
    virtual bool _setup() = 0;
    virtual bool _warmup() = 0;
    virtual bool _start() = 0;
    virtual bool _stop() = 0;
    virtual bool _cleanup() = 0;
};