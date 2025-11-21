/*
 * Copyright (C) 2025 eYs3D Corporation
 * All rights reserved.
 * This project is licensed under the Apache License, Version 2.0.
 */

#pragma once

#include <stdint.h>

namespace libeYs3D {
namespace devices {

/**
 * @brief Action categories for polymorphic endpoint selection
 *
 * For 80363 (ORANGE chip) which has 3 USB endpoints:
 * - base+0: General device info (firmware, serial, bus info)
 * - base+1: Optical sensor, calibration, camera properties
 * - base+2: IR control, depth frames
 *
 * Legacy cameras use single endpoint (base+0) for all operations.
 *
 * Optimized for embedded systems:
 * - uint8_t underlying type (1 byte instead of 4)
 * - No convenience wrapper functions
 * - Direct usage: getDeviceInfoByCategory(ActionCategory::CALIBRATION)
 */
enum class ActionCategory : uint8_t {
    DEVICE_INFO = 0,           // Firmware version, serial, bus info
    CALIBRATION,               // ZD table, rectify, Y offset, log data
    CAMERA_PROPERTY,           // CT/PU properties, exposure, gain
    IR_CONTROL,                // IR mode/value, interleave enable
    STREAMING,                 // OpenDevice, CloseDevice, SetDepthDataType
    STREAMING_MONO,            // OpenDevice, CloseDevice for eSP936 chip mono path
    STREAMING_HARDWARE_ACCESS, // FW/Sensor registers
    ASIC_ACCESS,               // IC registers
    FRAME_COLOR,               // Color frame reads
    FRAME_DEPTH,               // Depth frame reads
    FRAME_PROCESS              // Frame Processing
};

} // namespace devices
} // namespace libeYs3D
