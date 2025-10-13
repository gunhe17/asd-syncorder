#include "gonfig.h"

// Global
Config gonfig;

/**
 * @class
 */

Config Config::parseArgs(int argc, char* argv[]) {
    Config conf;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (false) {
            // ...
        }
        else if (arg == "--output_path" && i + 1 < argc) {
            conf.output_path = argv[++i];
        }
        else if (arg == "--verified_path" && i + 1 < argc) {
            conf.verified_path = argv[++i];
        }
        else if (arg == "--calibration_path" && i + 1 < argc) {
            conf.calibration_path = argv[++i];
        }
        else if (arg == "--record_duration" && i + 1 < argc) {
            conf.record_duration = std::stoi(argv[++i]);
        }
    }

    return conf;
}