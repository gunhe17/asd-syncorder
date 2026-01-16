#pragma once

#include <string>
#include <filesystem>
#include <iostream>
#include <fstream>


/**
 * @class Base Checker
 * Validates flat structure recordings (single recording session)
 */

class BChecker {
protected:
    bool result_{true};

public:
    BChecker() = default;
    virtual ~BChecker() = default;

public:
    virtual bool check() = 0;
};
