#pragma once

#include <librealsense2/rs.hpp>
#include <chrono>


/**
 * @struct RealsenseBufferData
 */

struct RealsenseBufferData {
    rs2::frame color_frame;
    rs2::frame depth_frame;

    RealsenseBufferData() = default;

    RealsenseBufferData(const rs2::frame& color, const rs2::frame& depth)
        : color_frame(color), depth_frame(depth) {}
};