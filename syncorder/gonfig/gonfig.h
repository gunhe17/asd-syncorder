#pragma once

#include <iostream>
#include <string>


/**
 * @class
 */

class Config {
public:
    std::string output_path = "./output/";
    std::string calibration_path = "./calibration.bin";

    int record_duration = 5;

    static Config parseArgs(int argc, char* argv[]);
};

// Global
extern Config gonfig;