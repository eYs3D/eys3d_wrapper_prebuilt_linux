/*
 * Copyright (C) 2021 eYs3D Corporation
 * All rights reserved.
 * This project is licensed under the Apache License, Version 2.0.
 */

#pragma once

#include "CameraDevice.h"
#include "video/FrameProducer.h"
#include "video/PCProducer.h"
#include "sensors/SensorDataProducer.h"
#include "base/synchronization/Lock.h"
#include "base/synchronization/ConditionVariable.h"
#include "LatestFrameBuffer.h"
#include "utils.h"
#include "debug.h"
#include "macros.h"

#include <stdio.h>

#define DEBUGGING false

#define DEFAULT_TIMEOUT_MS   3200
#define DEFAULT_TIMEOUT_US   3200000

namespace libeYs3D    {
namespace devices    {

/**
 * @class Pipeline
 * @brief Pull-based frame retrieval with latest-wins semantics
 *
 * ## When to Use Pipeline
 *
 * 1. **UI/Preview Applications** - Display at screen refresh rate (30-60 FPS)
 *    while camera runs at higher rate. Always shows the most recent frame.
 *
 * 2. **Latest-Frame Processing** - Algorithms that need current state, not historical
 *    frames. If processing is slower than frame rate, intermediate frames are
 *    silently discarded.
 *
 * 3. **Monitoring/Dashboard Applications** - Sample latest data periodically
 *    without processing every frame.
 *
 * ## Usage Example (Multi-Threaded)
 *
 * @code
 * // Initialize camera and get pipeline
 * auto pipeline = cameraDevice->initStream(colorFmt, colorW, colorH, fps,
 *                                          depthFmt, depthW, depthH, ...);
 *
 * // Pre-allocate frames with proper buffer sizes to avoid resize during polling
 * // Frame(dataSize, initVal, zdDepthSize, initVal, rgbSize, initVal)
 * uint64_t colorPixels = colorW * colorH;
 * uint64_t depthPixels = depthW * depthH;
 * int colorBpp = 2;  // YUY2 = 2 bytes/pixel
 * int depthBpp = 2;  // Depth raw = 2 bytes/pixel
 *
 * std::atomic<bool> running{true};
 *
 * // Thread 1: Color frame consumer
 * std::thread colorThread([&]() {
 *     Frame colorFrame(colorPixels * colorBpp, 0,   // dataVec
 *                      colorPixels, 0,               // zdDepthVec (unused for color)
 *                      colorPixels * 3, 0);          // rgbVec
 *     while (running) {
 *         if (pipeline->waitForColorFrame(&colorFrame, 1000) == Pipeline::OK) {
 *             processColor(colorFrame);  // Process at own pace
 *         }
 *     }
 * });
 *
 * // Thread 2: Depth frame consumer
 * std::thread depthThread([&]() {
 *     Frame depthFrame(depthPixels * depthBpp, 0,   // dataVec
 *                      depthPixels, 0,               // zdDepthVec
 *                      depthPixels * 3, 0);          // rgbVec (heatmap)
 *     while (running) {
 *         if (pipeline->waitForDepthFrame(&depthFrame, 1000) == Pipeline::OK) {
 *             processDepth(depthFrame);  // Independent processing
 *         }
 *     }
 * });
 *
 * // ... run for duration ...
 * running = false;
 * colorThread.join();
 * depthThread.join();
 *
 * cameraDevice->closeStream();
 * @endcode
 *
 */
class Pipeline    {
public:
    enum RESULT    {
        SYNC_ERROR = -2,
        STOPPED = -1,
        OK = 0,
        TIMEOUT,
        QUEUE_EMPTY,
        QUEUE_FULL
    };

template <typename T, size_t CAPACITY>
class CircularQueue    {
public:
#ifdef _WIN32
    Pipeline::RESULT enQueue(const T *item, int32_t timeoutMs = DEFAULT_TIMEOUT_MS)    {

		if(mStopped)    return Pipeline::RESULT::STOPPED;

		mLock.lockWrite();

		Pipeline::RESULT ret = Pipeline::RESULT::OK;

        while(mCount == mCapacity && Pipeline::RESULT::OK == ret)    { // queue full, mRear == mFront

            if (mStopped) {
				ret = Pipeline::RESULT::STOPPED;
				break;
            }

			if (timeoutMs == 0) {
				mFront = (mFront + 1) % mCapacity;
				mRear = (mRear + 1) % mCapacity;
				mItems[mRear].clone(item);

				LOG_INFO("enQueue" , "%s: queue is full, mFront=%d, mRear = %d, mCount=%d", mName, mFront, mRear, mCount);
				ret = Pipeline::RESULT::QUEUE_FULL;
			} else if (timeoutMs < 0) { // wait forever
				 // instead of using wait() to check if the queue is stopped
				if(false == mReadyToProduce.timedWaitDebug(&mLock,
									  now_in_microsecond_unix_time() +
									  (DEFAULT_TIMEOUT_MS * 1000))) {
					LOG_INFO("enQueue" , "%s: sync error, mFront=%d, mRear = %d, mCount=%d", mName, mFront, mRear, mCount);
					ret = Pipeline::RESULT::SYNC_ERROR;
				}
			} else {
                if(false == mReadyToProduce.timedWaitDebug(&mLock,
                                      now_in_microsecond_unix_time() +
                                      (timeoutMs * 1000))) {
					LOG_INFO("enQueue" , "%s: sync error, mFront=%d, mRear = %d, mCount=%d", mName, mFront, mRear, mCount);
					ret = Pipeline::RESULT::SYNC_ERROR;
                }
			}

			if (DEBUGGING && Pipeline::RESULT::OK == ret)
				LOG_INFO("enQueue" , "%s: loop again, mFront=%d, mRear = %d, mCount=%d", mName, mFront, mRear, mCount);
        } // end of while(true)


		if (Pipeline::RESULT::OK == ret) {
		   mRear = (mRear + 1) % mCapacity;
		   mItems[mRear].clone(item);
		   mCount += 1;
		}

		if (DEBUGGING)
			LOG_INFO("enQueue" , "%s: mCount=%d", mName, mCount);

		mLock.unlockWrite();

		if (Pipeline::RESULT::STOPPED == ret)
			mReadyToProduce.broadcast();
		else
			mReadyToConsume.broadcast();

        return Pipeline::RESULT::OK;
    }
    
    Pipeline::RESULT deQueue(T *item, int32_t timeoutMs = DEFAULT_TIMEOUT_MS)    {

        if(mStopped)    return Pipeline::RESULT::STOPPED;

		mLock.lockRead();

		Pipeline::RESULT ret = Pipeline::RESULT::OK;
		
        while(mCount == 0 && Pipeline::RESULT::OK == ret)    {

            if(mStopped) {
				ret = Pipeline::RESULT::STOPPED;
				break;
            }
            
            if(timeoutMs == 0)    {
				LOG_INFO("deQueue" , "%s: queue is empty, mFront=%d, mRear = %d, mCount=%d", mName, mFront, mRear, mCount);
                ret = Pipeline::RESULT::QUEUE_EMPTY;
            } else if(timeoutMs > 0)    {
                if(false == mReadyToConsume.timedWaitDebug(&mLock,
                                      now_in_microsecond_unix_time() +
                                      (timeoutMs * 1000))) {
					LOG_INFO("deQueue" , "%s: sync error, mFront=%d, mRear = %d, mCount=%d", mName, mFront, mRear, mCount);
                    ret = Pipeline::RESULT::SYNC_ERROR;
                }
            } else if (timeoutMs < 0) { // wait forever
                 // instead of using wait() to check if the queue is stopped
                if(false == mReadyToConsume.timedWaitDebug(&mLock,
                                      now_in_microsecond_unix_time() +
                                      (DEFAULT_TIMEOUT_MS * 1000))) {
					LOG_INFO("deQueue" , "%s: sync error, mFront=%d, mRear = %d, mCount=%d", mName, mFront, mRear, mCount);
                    ret = Pipeline::RESULT::SYNC_ERROR;
                }
            }

			if (DEBUGGING && Pipeline::RESULT::OK == ret)
				LOG_INFO("deQueue" , "%s: loop again, mFront=%d, mRear = %d, mCount=%d", mName, mFront, mRear, mCount);

        }

		if (Pipeline::RESULT::OK == ret) {
    		mFront = (mFront + 1) % mCapacity;
    		item->clone(&mItems[mFront]);
    		mCount -= 1;
		}

		if (DEBUGGING)
			LOG_INFO("deQueue" , "%s: mCount=%d", mName, mCount);

		mLock.unlockRead();

		if (Pipeline::RESULT::STOPPED == ret)
			mReadyToConsume.signal();
		else
			mReadyToProduce.signal();

        return ret;
    }
	
	void reset()    {
        libeYs3D::base::AutoWriteLock lock(mLock);

		LOG_INFO("reset" , "%s: mFront=%d, mRear=%d, mCount=%d", mName, mFront, mRear, mCount);
		
        mFront = 0;
        mRear = 0;
        mCount = 0;
    }
    
    void stop()    {
        
        if(mStopped)    return;
        
        mStopped = true;

		{
			libeYs3D::base::AutoWriteLock lock(mLock);

			LOG_INFO("stop" , "%s: mFront=%d, mRear=%d, mCount=%d", mName, mFront, mRear, mCount);
			
			mCount = 0;
		}

        mReadyToConsume.broadcast();
		mReadyToProduce.broadcast();
    }
#else  // !_WIN32
    Pipeline::RESULT enQueue(const T *item, int32_t timeoutMs = DEFAULT_TIMEOUT_MS)    {
        libeYs3D::base::AutoLock lock(mLock);
        
        while(true)    {
            if(mStopped)    return RESULT::STOPPED;

            if((mRear != mFront) || (mCount == 0))    {
                mCount += 1;
                mRear = (mRear + 1) % mCapacity;
                mItems[mRear].clone(item);
            
                if(mCount == 1)    mCond.signal();
                                
                break;
            } else    { // queue full, ((mRear == mFront) && (mCount == mCapacity))
                if(timeoutMs == 0)    {
                    mFront = (mFront + 1) % mCapacity;
                    mRear = (mRear + 1) % mCapacity;
                    mItems[mRear].clone(item);

                    break;
                } else if(timeoutMs > 0)    {
                    if(false == mCond.timedWaitDebug(&mLock,
                                          now_in_microsecond_high_res_time_REALTIME() +
                                          (timeoutMs * 1000)))
                        return RESULT::SYNC_ERROR;
                    else
                        if(mCount == mCapacity)    return Pipeline::RESULT::TIMEOUT;
                } else    { // timeoutMs < 0, wait forever
                    // instead of using mCond.wait(), checking if queue is stopped is required
                    if(false == mCond.timedWaitDebug(&mLock,
                                          now_in_microsecond_high_res_time_REALTIME() +
                                          DEFAULT_TIMEOUT_US))
                        return RESULT::SYNC_ERROR;
                }
            }
        } // end of while(true)
        
        return RESULT::OK;
    }
    
    Pipeline::RESULT deQueue(T *item, int32_t timeoutMs = DEFAULT_TIMEOUT_MS)    {
        libeYs3D::base::AutoLock lock(mLock);
        
        if(mStopped)    return Pipeline::RESULT::STOPPED;
        
        while(mCount == 0)    {
            if(mStopped)    return Pipeline::RESULT::STOPPED;
            
            if(timeoutMs == 0)    {
                return Pipeline::RESULT::QUEUE_EMPTY;
            } else if(timeoutMs > 0)    {
                if(false == mCond.timedWaitDebug(&mLock,
                                      now_in_microsecond_high_res_time_REALTIME() +
                                      (timeoutMs * 1000)))
                    return Pipeline::RESULT::SYNC_ERROR;
                    
                if(mCount == 0)    return Pipeline::RESULT::TIMEOUT;
            } else    { // wait forever
                // instead of using mCond.wait() to check if the queue is stopped
                if(false == mCond.timedWaitDebug(&mLock,
                                      now_in_microsecond_high_res_time_REALTIME() +
                                      (DEFAULT_TIMEOUT_MS * 1000)))
                    return Pipeline::RESULT::SYNC_ERROR;
            }
        }
        
        if(mCount == mCapacity)    mCond.signal();

        mCount -= 1;
        mFront = (mFront + 1) % mCapacity;
        item->clone(&mItems[mFront]);

        return Pipeline::RESULT::OK;
    }
    
    void reset()    {
        libeYs3D::base::AutoLock lock(mLock);
        
        mFront = 0;
        mRear = 0;
        mCount = 0;
    }
    
    void stop()    {
        libeYs3D::base::AutoLock lock(mLock);
        
        if(mStopped)    return;
        
        mStopped = true;
        mCond.broadcast();
    }
#endif
    
    CircularQueue(const char *name)    {
        snprintf(mName, sizeof(mName), "%s", name);
    }

    ~CircularQueue()    { stop(); }
    
private:
    char mName[128];
    T mItems[CAPACITY];
#ifdef _WIN32
	libeYs3D::base::ReadWriteLock mLock;
	libeYs3D::base::ConditionVariable mReadyToConsume;
	libeYs3D::base::ConditionVariable mReadyToProduce;
#else
    libeYs3D::base::Lock mLock;
    libeYs3D::base::ConditionVariable mCond;
#endif
    size_t mFront = 0;
    size_t mRear = 0;
    size_t mCount = 0;
    size_t mCapacity = CAPACITY;
    
    bool mStopped = false;
};

public:
    /**
     * @brief Non-blocking poll for the latest frame (Color/Depth/PC)
     *
     * Retrieves the most recent frame if new data is available since the last poll.
     * Returns immediately without waiting.
     *
     * @param[out] frame  Output buffer to receive cloned frame data
     * @return
     *     OK:          New frame cloned successfully
     *     QUEUE_EMPTY: No new frame available (already read the latest)
     *     STOPPED:     Pipeline has been stopped
     *
     * @note For Color/Depth/PC frames, uses LatestFrameBuffer (always returns latest).
     *       Frames produced while user is processing are silently replaced.
     */
    RESULT pollColorFrame(libeYs3D::video::Frame *frame);
    RESULT pollDepthFrame(libeYs3D::video::Frame *frame);
    RESULT pollPCFrame(libeYs3D::video::PCFrame *pcFrame);

    /**
     * @brief Non-blocking poll for the latest IMU data
     *
     * Retrieves IMU sensor data from the circular queue if available.
     *
     * @param[out] imuData  Output buffer to receive cloned IMU data
     * @return
     *     OK:          IMU data retrieved successfully
     *     QUEUE_EMPTY: No IMU data available in queue
     *     STOPPED:     Pipeline has been stopped
     *
     * @note IMU data uses CircularQueue (FIFO) unlike frame buffers.
     */
    RESULT pollIMUData(libeYs3D::sensors::SensorData *imuData);

    /**
     * @brief Blocking wait for the latest frame (Color/Depth/PC)
     *
     * Waits until a new frame is available, then clones it to the output buffer.
     * A "new" frame means one produced after the last successful wait/poll.
     *
     * @param[out] frame     Output buffer to receive cloned frame data
     * @param[in]  timeoutMs Maximum time to wait in milliseconds
     *     - timeoutMs > 0:  Wait up to timeoutMs for new frame
     *     - timeoutMs == 0: Equivalent to poll (non-blocking)
     *     - timeoutMs < 0:  Wait indefinitely (with internal 3.2s safety checks)
     * @return
     *     OK:         New frame cloned successfully
     *     TIMEOUT:    No new frame within timeoutMs
     *     STOPPED:    Pipeline has been stopped
     *     SYNC_ERROR: Internal synchronization error
     *
     * @note For Color/Depth/PC frames, uses LatestFrameBuffer. Only the most
     *       recent frame is kept; intermediate frames are discarded.
     */
    RESULT waitForColorFrame(libeYs3D::video::Frame *frame,
                             int32_t timeoutMs = DEFAULT_TIMEOUT_MS);
    RESULT waitForDepthFrame(libeYs3D::video::Frame *frame,
                             int32_t timeoutMs = DEFAULT_TIMEOUT_MS);
    RESULT waitForPCFrame(libeYs3D::video::PCFrame *pcFrame,
                          int32_t timeoutMs = DEFAULT_TIMEOUT_MS);

    /**
     * @brief Blocking wait for IMU data
     *
     * Waits until IMU sensor data is available in the queue.
     *
     * @param[out] imuData   Output buffer to receive cloned IMU data
     * @param[in]  timeoutMs Maximum time to wait in milliseconds
     *     - timeoutMs > 0:  Wait up to timeoutMs for data
     *     - timeoutMs == 0: Equivalent to poll (non-blocking)
     *     - timeoutMs < 0:  Wait indefinitely
     * @return
     *     OK:         IMU data retrieved successfully
     *     TIMEOUT:    No data within timeoutMs
     *     STOPPED:    Pipeline has been stopped
     *     SYNC_ERROR: Internal synchronization error
     *
     * @note IMU data uses CircularQueue (FIFO) with capacity of 8 samples.
     */
    RESULT waitForIMUData(libeYs3D::sensors::SensorData *imuData,
                          int32_t timeoutMs = DEFAULT_TIMEOUT_MS);
    
    void reset();
    //void start();

    virtual ~Pipeline();

private:
    explicit Pipeline(CameraDevice *cameraDevice);

    /**
     * Initialize LatestFrameBuffer slots with proper buffer sizes
     *
     * MUST be called after initStream() sets up the frame dimensions
     * but BEFORE streaming starts to prevent buffer corruption.
     *
     * @param colorWidth Color frame width (0 if color disabled)
     * @param colorHeight Color frame height (0 if color disabled)
     * @param colorBytesPerPixel Bytes per pixel for color format
     * @param depthWidth Depth frame width (0 if depth disabled)
     * @param depthHeight Depth frame height (0 if depth disabled)
     * @param depthBytesPerPixel Bytes per pixel for depth format
     */
    void initializeBuffers(int32_t colorWidth, int32_t colorHeight, int32_t colorBytesPerPixel,
                          int32_t depthWidth, int32_t depthHeight, int32_t depthBytesPerPixel);

    /**
     * @brief Producer API: Insert frame into buffer (Color/Depth/PC)
     *
     * Swaps the frame's buffers into LatestFrameBuffer. After this call,
     * the input frame's buffers are swapped out (contain old/empty data).
     *
     * "Latest wins" policy: Always succeeds immediately. If user hasn't
     * consumed the previous frame, it is silently replaced.
     *
     * @param[in,out] frame  Frame to insert (buffers swapped after call)
     * @param[in] timeoutMs  Unused for LatestFrameBuffer (always immediate)
     * @return
     *     OK:      Frame inserted successfully
     *     STOPPED: Pipeline has been stopped
     *
     * @note Called internally by FrameProducer callbacks. Not for user code.
     * @note timeoutMs parameter is kept for API compatibility but ignored.
     */
    RESULT insertColorFrame(libeYs3D::video::Frame *frame,
                            int32_t timeoutMs = DEFAULT_TIMEOUT_MS);
    RESULT insertDepthFrame(libeYs3D::video::Frame *frame,
                            int32_t timeoutMs = DEFAULT_TIMEOUT_MS);
    RESULT insertPCFrame(libeYs3D::video::PCFrame *pcFrame,
                         int32_t timeoutMs = DEFAULT_TIMEOUT_MS);

    /**
     * @brief Producer API: Insert IMU data into circular queue
     *
     * Enqueues IMU sensor data. Uses CircularQueue with FIFO semantics.
     *
     * @param[in] imuData   IMU data to enqueue (cloned into queue)
     * @param[in] timeoutMs Maximum time to wait if queue is full
     *     - timeoutMs == 0: If full, drop oldest and insert
     *     - timeoutMs > 0:  Wait up to timeoutMs for space
     *     - timeoutMs < 0:  Wait indefinitely
     * @return
     *     OK:         Data enqueued successfully
     *     TIMEOUT:    Queue full and timeout exceeded
     *     STOPPED:    Pipeline has been stopped
     *     SYNC_ERROR: Internal synchronization error
     *
     * @note Called internally by IMU producer callback. Not for user code.
     */
    RESULT insertIMUData(const libeYs3D::sensors::SensorData *imuData,
                         int32_t timeoutMs = DEFAULT_TIMEOUT_MS);

    void stop();

    bool colorImageCallback(const libeYs3D::video::Frame* frame);
    bool depthImageCallback(const libeYs3D::video::Frame* frame);
    bool pcFrameCallback(const libeYs3D::video::PCFrame *pcFrame);
    bool imuDataCallback(const libeYs3D::sensors::SensorData *sensorData);

private:
    CameraDevice *mCameraDevice;
    
    bool mStopped = false;
    
    libeYs3D::video::Producer::Callback mColorImageCallback;
    libeYs3D::video::Producer::Callback mDepthImageCallback;
    libeYs3D::video::PCProducer::PCCallback mPCFrameCallback;
    libeYs3D::sensors::SensorDataProducer::AppCallback mIMUDataCallback;
    
    // LatestFrameBuffer for Color/Depth/PC frames (zero-copy swap at enqueue)
    LatestFrameBuffer<libeYs3D::video::Frame> mColorFrameBuffer;
    LatestFrameBuffer<libeYs3D::video::Frame> mDepthFrameBuffer;
    LatestFrameBuffer<libeYs3D::video::PCFrame> mPCFrameBuffer;

    // Keep CircularQueue for IMU data (small data, clone is fine)
    static constexpr int kMaxIMUDataCount = 8;
    CircularQueue<libeYs3D::sensors::SensorData, kMaxIMUDataCount> mIMUDataQueue;

public:
    friend class CameraDevice;
};

} // end of namespace devices
} // end of namespace libeYs3D
