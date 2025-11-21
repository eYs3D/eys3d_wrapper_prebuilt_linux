/*
 * Copyright (C) 2021 eYs3D Corporation
 * All rights reserved.
 * This project is licensed under the Apache License, Version 2.0.
 */

#pragma once

#include <memory>
#include <thread>
#include <atomic>
#include <chrono>
#include "eSPDI.h"
#include "video/Frame.h"
#include "base/synchronization/MessageChannel.h"

namespace libeYs3D {
namespace video {

// Forward declarations
class ColorFrameProducer;
class DepthFrameProducer;

/**
 * ILMFrameRouter handles frame reading and routing for ORANGE chip 80363 ILM mode.
 *
 * Architecture:
 *   1. Maintains single shared frame pool (4 frames)
 *   2. Reader thread calls APC_GetImage() from Color Path 1
 *   3. Routes frames by serial number parity to producer mDataQueue
 *   4. Uses BLOCKING send() - propagates backpressure naturally
 *   5. Logs all frame drops with detailed reasons
 *   6. Provides comprehensive statistics
 *
 * Frame Routing:
 *   - Odd serial numbers (1, 3, 5, ...) → ColorFrameProducer
 *   - Even serial numbers (2, 4, 6, ...) → DepthFrameProducer
 *
 * Error Handling:
 *   - Transient errors: Log + recycle frame + retry after 10ms
 *   - Fatal errors: Log + stop thread after MAX_CONSECUTIVE_FAILURES
 *   - Channel stopped: Log + clean shutdown (not an error)
 *
 * Statistics:
 *   - Per-frame drops logged immediately (WARN/ERROR level)
 *   - Periodic summaries every 100 frames (INFO level)
 *   - Final summary on stop (INFO level)
 */
class ILMFrameRouter {
public:
    /**
     * Construct ILMFrameRouter.
     *
     * IMPORTANT: Caller MUST validate colorWidth == depthHeight BEFORE calling.
     *
     * @param colorProducer Pointer to ColorFrameProducer
     * @param depthProducer Pointer to DepthFrameProducer
     * @param devSelInfo Device selection info for Color Path 1
     * @param width Frame width (must match for color and depth)
     * @param height Frame height (must match for color and depth)
     * @param nDevType Device type (e.g., GRAPE/ORANGE)
     * @param nZDTableSize Size of ZD lookup table
     * @param nZDTable Pointer to ZD lookup table array
     */
    ILMFrameRouter(ColorFrameProducer* colorProducer,
                   DepthFrameProducer* depthProducer,
                   DEVSELINFO* devSelInfo,
                   int width,
                   int height,
                   uint16_t nDevType,
                   int32_t nZDTableSize,
                   const uint8_t* nZDTable);

    ~ILMFrameRouter();

    // Non-copyable, non-movable
    ILMFrameRouter(const ILMFrameRouter&) = delete;
    ILMFrameRouter& operator=(const ILMFrameRouter&) = delete;

    /**
     * Start the reader thread.
     * @return true if started successfully
     */
    bool start();

    /**
     * Stop the reader thread (blocking until thread exits).
     * Safe to call multiple times.
     */
    void stop();

    /**
     * Request stop of reader thread (non-blocking).
     */
    void requestStop() {
        mStopRequested = true;
    }

    /**
     * Get pointer to shared free pool for producer injection.
     * @return Pointer to MessageChannel with 4-frame pool
     */
    base::MessageChannel<Frame, 4>* getSharedFreeQueue() {
        return &mSharedFreeQueue;
    }

private:
    // Shared free pool
    static constexpr int kSharedPoolSize = 4;
    base::MessageChannel<Frame, kSharedPoolSize> mSharedFreeQueue;

    // Core methods
    void readerThreadWorker();
    void routeFrame(Frame&& frame, uint32_t serial);
    void initializeSharedPool();
    // Producer pointers (not owned)
    ColorFrameProducer* mColorProducer;
    DepthFrameProducer* mDepthProducer;

    // USB endpoint info
    DEVSELINFO* mDevSelInfo;

    // Frame dimensions
    int mWidth;
    int mHeight;

    // ZD table for depth frame initialization
    uint16_t mDevType;
    int32_t mZDTableSize;
    std::vector<uint8_t> mZDTable;

    // Reader thread control
    std::unique_ptr<std::thread> mReaderThread;
    std::atomic<bool> mStopRequested{false};

    // Error tracking
    std::atomic<int> mConsecutiveRoutingFailures{0};
    static constexpr int MAX_CONSECUTIVE_FAILURES = 100;

    // Statistics
    static constexpr uint64_t STATS_LOG_INTERVAL = 100;
};

} // namespace video
} // namespace libeYs3D
