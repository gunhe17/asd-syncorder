#pragma once

#include <chrono>
#include "tobii_research.h"
#include "tobii_research_calibration.h"
#include "tobii_research_eyetracker.h"
#include "tobii_research_streams.h"


/**
 * @struct TobiiBufferData - Global Tobii Data Structure
 */

struct TobiiBufferData {
    TobiiResearchGazeData gazed;
};