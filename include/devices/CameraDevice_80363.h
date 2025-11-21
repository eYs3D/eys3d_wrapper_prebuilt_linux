/*
 * Copyright (C) 2025 eYs3D Corporation
 * All rights reserved.
 * This project is licensed under the Apache License, Version 2.0.
 */

#pragma once

#include "CameraDevice.h"
#include "video/ILMFrameRouter.h"

namespace libeYs3D {

// Forward declarations
namespace video {
    class ILMFrameRouter;
}

namespace devices {

/**
 * @brief Camera device implementation for 80363 module (eSP936/ORANGE chip)
 *
 * Key Features:
 * - ORANGE / GRAPE chip
 * - Interleave mode support (single endpoint, serial number based frame detection)
 * - Three-device in one port architecture: base+0 (unused), base+1 (COLOR_PATH1), base+2 (MONO_PATH)
 * - Multiple resolution configurations with ZD table mapping
 * - Three streaming modes: Interleave, color-only, depth-only, dual-stream
 *
 * PIDs:
 * - APC_PID_80363C  = 0x0202 (Color variant)
 * - APC_PID_80363IR = 0x0211 (IR variant)
 *
 * Device Type:
 * - MUST be detected as GRAPE/ORANGE (type 5), NOT PUMA (type 2)
 */
class CameraDevice80363 : public CameraDevice {
public:
    /**
     * @brief Construct CameraDevice80363 with specified color byte order
     * @param devSelInfo Device selection information
     * @param deviceInfo Device information structure
     * @param colorByteOrder Color data byte order (RGB24, BGR24, etc.)
     */
    CameraDevice80363(DEVSELINFO *devSelInfo,
                      DEVINFORMATION *deviceInfo,
                      COLOR_BYTE_ORDER colorByteOrder);

    /**
     * @brief Construct CameraDevice80363 with default color order (RGB24)
     * @param devSelInfo Device selection information
     * @param deviceInfo Device information structure
     */
    explicit CameraDevice80363(DEVSELINFO *devSelInfo,
                               DEVINFORMATION *deviceInfo);

    /**
     * @brief Destructor with defensive ILMFrameRouter cleanup
     */
    virtual ~CameraDevice80363();

    /**
     * @brief Initialize camera stream with specified parameters
     *
     * Supports three streaming modes:
     * 1. ILM mode: depthFormat = 0x1A or 0x1B (single endpoint, alternating frames)
     * 2. Color-only: depthWidth = 0 (single endpoint base+1)
     * 3. Depth-only: colorWidth = 0 (dual endpoints, base+1 at depth res + base+2)
     * 4. Dual-stream: both > 0, non-ILM (dual endpoints with sleep(2))
     * DEPTH_IMG_NON_TRANSFER flag will not decode the depth view into color palette depth view.
     * DEPTH_IMG_COLORFUL_TRANSFER flag will decode depth view into color palette depth view for human reading.
     * @return APC_OK on success, error code otherwise
     */
    virtual int initStream(libeYs3D::video::COLOR_RAW_DATA_TYPE colorFormat,
                           int32_t colorWidth, int32_t colorHeight, int32_t actualFps,
                           libeYs3D::video::DEPTH_RAW_DATA_TYPE depthFormat,
                           int32_t depthWidth, int32_t depthHeight,
                           DEPTH_TRANSFER_CTRL depthDataTransferCtrl,
                           CONTROL_MODE ctrlMode,
                           int rectifyLogIndex,
                           libeYs3D::video::Producer::Callback colorImageCallback,
                           libeYs3D::video::Producer::Callback depthImageCallback,
                           libeYs3D::video::PCProducer::PCCallback pcFrameCallback,
                           libeYs3D::sensors::SensorDataProducer::AppCallback imuDataCallback = nullptr) override;

    /**
     * @brief Check if interleave mode is supported
     * @return true (80363 always supports interleave mode)
     */
    virtual bool isInterleaveModeSupported() override;

    /**
     * @brief Enable or disable interleave mode
     *
     * ORANGE chip note: Interleave mode is controlled by video mode enum (0x1A/0x1B),
     * NOT by this API. This API is used for IR switching control (on/off/on/off by frames).
     *
     * @param enable true to enable, false to disable
     * @return APC_OK on success, error code otherwise
     */
    virtual int enableInterleaveMode(bool enable) override;

    /**
     * @brief Check if Hardware Post-Processing (HWPP) is supported
     *
     * 80363 (eSP936/ORANGE chip) does not support Hardware Post-Processing.
     * This override prevents HWPP operations from being attempted on this device.
     *
     * @return false (HWPP not supported on 80363)
     */
    virtual bool isHWPPSupported() override { return false; }

    /**
     * @brief Convert eSP936-specific depth format to APCImageType
     *
     * Handles eSP936 (ORANGE chip) specific depth formats (0x18-0x1b)
     * that do not map to standard APC_DEPTH_DATA_xxx values.
     *
     * @param depthFormat Raw depth format value
     * @return APCImageType::Value (DEPTH_11BITS, DEPTH_14BITS, etc.)
     */
    virtual APCImageType::Value getDepthImageType(uint32_t depthFormat) const override;

    /**
     * @brief Get SDK-compatible depth data type for point cloud processing
     *
     * Maps eSP936-specific depth formats (0x18-0x1b) to APC_GetPointCloud API compatible video mode:
     * - 0x18/0x1a (11-bit formats) -> APC_DEPTH_DATA_11_BITS
     * - 0x19/0x1b (14-bit formats) -> APC_DEPTH_DATA_14_BITS
     *
     * @return SDK depth data type for point cloud APIs
     */
    virtual uint32_t getPointCloudDepthType() const override;

    /**
     * @brief Close additional devices beyond primary device (Mono Path)
     *
     * Called after primary device (Color Path 1) is closed.
     * Closes Mono Path if it was opened (dual-device mode).
     *
     * @return APC_OK on success, error code otherwise
     */
    int closeAdditionalDevices() override;

    /**
     * @brief Enable blocking mode for all opened devices
     *
     * Enables blocking for Color Path 1 (always) and Mono Path (if dual-device mode).
     */
    void enableBlockingForAllDevices() override;

    /**
     * @brief Check if using ILM shared free pool
     *
     * @return true if ILMFrameRouter is active (ILM mode with shared pool), false otherwise
     */
    bool isUsingILMSharedPool() const override {
        return mILMFrameRouter != nullptr;
    }

    /**
     * @brief Enable streaming (hides base class to restart ILMFrameRouter)
     *
     * ILM mode requires restarting the ILMFrameRouter reader thread AFTER enabling
     * producer callbacks. The router must start after producers are ready to receive
     * frames via mStageQueue.
     *
     * Order of operations:
     * 1. Call base class to enable producer callbacks
     * 2. Restart ILMFrameRouter reader thread (if ILM mode)
     *
     */
    void enableStream() override;

    /**
     * @brief Close streaming (override to stop ILMFrameRouter before producers)
     *
     * CRITICAL: Must stop ILMFrameRouter BEFORE stopping producers to avoid deadlock.
     *
     * This method stops ILMFrameRouter first, then calls base class closeStream().
     */
    int closeStream() override;

    friend class CameraDeviceFactory;

    // Endpoint index constants for 3-endpoint architecture
    static constexpr uint8_t INDEX_COLOR_PATH0 = 0;  // base+0 (unused)
    static constexpr uint8_t INDEX_COLOR_PATH1 = 1;  // base+1 (primary control)
    static constexpr uint8_t INDEX_MONO_PATH = 2;    // base+2 (depth/IR)

protected:
    /**
     * @brief Get DEVSELINFO pointer for specific action category (80363 multi-endpoint routing)
     *
     * Overrides base class to route SDK calls to correct USB endpoint:
     * - DEVICE_INFO -> base+0 (ColorPath0): Firmware, serial, bus info
     * - CALIBRATION -> base+1 (ColorPath1): ZD table, rectify, Y offset
     * - CAMERA_PROPERTY -> base+1 (ColorPath1): CT/PU properties, exposure, gain
     * - STREAMING -> base+1 (ColorPath1): OpenDevice, SetDepthDataType
     * - FRAME_COLOR -> base+1 (ColorPath1): Color frame reads
     * - IR_CONTROL -> base+2 (MonoPath): IR mode/value, interleave enable
     * - FRAME_DEPTH -> base+2 (MonoPath): Depth frame reads (non-ILM)
     * - HARDWARE_ACCESS -> base+1 (ColorPath1): Default for registers
     *
     * @param category Action category to route
     * @return Pointer to appropriate DEVSELINFO structure
     */
    virtual DEVSELINFO* getDeviceInfoByCategory(ActionCategory category) override;

    /**
     * @brief Get ZD table index based on resolution
     *
     * Maps color/depth resolution to calibration data index (0-4).
     * Returns the user-provided mRectifyLogIndex set during initStream().
     *
     * @return ZD table index (0-4)
     */
    virtual int getZDTableIndex() override;

    /**
     * @brief Update ZD table by calculating from point cloud information
     *
     * 80363 calculates ZD table from camera intrinsics/extrinsics instead of loading from file.
     * Uses disparity-to-world mapping from PointCloudInfo to generate depth lookup table.
     *
     * Algorithm:
     * 1. Clear existing ZD table
     * 2. For each disparity value: Z = focalLength / disparityToW[i]
     * 3. Store as big-endian 16-bit values using __bswap_16
     * 4. Calculate nZNear and nZFar from table
     *
     * @return APC_OK on success, APC_NO_CALIBRATION_LOG if no valid calibration data
     */
    virtual int updateZDTable() override;

    /**
     * @brief Initialize 3 endpoint indices based on USB enumeration
     *
     * ORANGE chip appears as 3 USB device indices:
     * - base+0: Color Path 0 (unused)
     * - base+1: Color Path 1 (primary control, color frames)
     * - base+2: Mono Path (depth/IR control)
     *
     * Called from constructor after base class initialization.
     * @return APC_OK on success
     */
    virtual int initDeviceSelInfo();

    /**
     * @brief Get DEVSELINFO for specific endpoint
     * @param pathIndex 0=color0, 1=color1, 2=mono
     * @return Pointer to DEVSELINFO or nullptr if invalid
     */
    DEVSELINFO* getDeviceSelInfo(uint8_t pathIndex);

    /**
     * @brief Read color frame from device
     *
     * In ILM mode: reads from Color Path 1 (base+1), filters by odd serial numbers
     * In non-ILM mode: reads from Color Path 1 (base+1)
     *
     * @param buffer Output buffer for frame data
     * @param bufferLength Size of output buffer
     * @param actualSize Actual size of frame data read
     * @param serial Frame serial number
     * @return APC_OK on success
     */
    virtual int readColorFrame(uint8_t *buffer, uint64_t bufferLength,
                               uint64_t *actualSize, uint32_t *serial) override;

    /**
     * @brief Read depth frame from device
     *
     * In ILM mode: reads from Color Path 1 (base+1), filters by even serial numbers
     * In non-ILM mode: reads from Mono Path (base+2)
     *
     * @param buffer Output buffer for frame data
     * @param bufferLength Size of output buffer
     * @param actualSize Actual size of frame data read
     * @param serial Frame serial number
     * @return APC_OK on success
     */
    virtual int readDepthFrame(uint8_t *buffer, uint64_t bufferLength,
                               uint64_t *actualSize, uint32_t *serial) override;

    /**
     * eSP936 not support it temporarily
     * @return
     */
    int adjustRegisters() override { return APC_OK; }

private:
    /**
     * @brief Interleave mode support flag
     * Always true for 80363
     */
    bool mSupportingInterleave;

    /**
     * @brief Base index from USB enumeration
     * Used to calculate all 3 endpoint indices
     */
    int mBaseIndex;

    /**
     * @brief Device selection info for Color Path 0 (base+0, unused)
     */
    DEVSELINFO mDevSelInfoColorPath0;

    /**
     * @brief Device selection info for Color Path 1 (base+1, primary endpoint)
     * Used for: color frames, depth type setting, ILM mode
     */
    DEVSELINFO mDevSelInfoColorPath1;

    /**
     * @brief Device selection info for Mono Path (base+2, depth/IR endpoint)
     * Used for: depth frames (non-ILM), IR control, interleave enable API
     */
    DEVSELINFO mDevSelInfoMonoPath;

    /**
     * @brief Track if currently in ILM mode
     * Set to true when depthFormat is 0x18 or 0x19
     */
    bool mIsILMMode;

    /**
     * @brief ILM frame router for shared free pool mode
     * Manages single shared pool (4 frames) and routes frames by serial number parity
     * Only active in ILM mode
     */
    std::unique_ptr<libeYs3D::video::ILMFrameRouter> mILMFrameRouter;

    /**
     * @brief Track if in depth-only mode
     * Set to true when colorWidth=0 but depthWidth>0
     */
    bool mIsDepthOnlyMode;

    /**
     * @brief Track if Color Path 1 device is opened
     * Used for idempotent closeStream() and dual-device management
     */
    bool mColorPath1Opened = false;

    /**
     * @brief Track if Mono Path device is opened
     * Used to determine dual-device mode and for idempotent closeStream()
     */
    bool mMonoPathOpened = false;

    /**
     * @brief Dummy callback for depth-only mode
     *
     * In depth-only mode, Color Path 1 must be opened and actively read (firmware requirement),
     * but frames should not be delivered to user. This callback drains Color Path 1 frames
     * and discards them silently.
     *
     * CRITICAL: Must be a class member to avoid dangling reference when lambda goes out of scope.
     */
    libeYs3D::video::Producer::Callback mDummyCallback;
};

} // namespace devices
} // namespace libeYs3D
