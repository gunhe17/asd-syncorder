#pragma once

#include <iostream>
#include <string>


/**
 * @class
 */

class Config {
public:
    std::string output_path = "./output/";
    std::string verified_path = "./output/verified/";
    std::string calibration_path = "./calibration.bin";

    int record_duration = 5;
    int tobii_sampling_rate = 120;  // Tobii eye tracker sampling rate (Hz)

    static Config parseArgs(int argc, char* argv[]);
};

// Global
extern Config gonfig;