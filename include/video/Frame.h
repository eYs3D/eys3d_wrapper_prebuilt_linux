/*
 * Copyright (C) 2021 eYs3D Corporation
 * All rights reserved.
 * This project is licensed under the Apache License, Version 2.0.
 */

#pragma once

#include "devices/AlignedAllocator.h"
#include "devices/model/DepthAccuracyOptions.h"
#include "sensors/SensorData.h"
#include "base/synchronization/Lock.h"
#include "GeneralFrame.h"
#include "video/video.h"

#include <cstdint>                // for uint8_t, uint32_t, uint64_t
#include <vector>                 // for vector
#include <string>

namespace libeYs3D {
namespace video    {

//forward declaration
class FrameProducer; 

// A small structure to encapsulate frame data.
class Frame : public GeneralFrame  {
public:
    int64_t tsUs = 0ll;    // timestamp(microsec)
    uint32_t serialNumber; // serial number of this frame
    int32_t width;
    int32_t height;
    uint64_t actualDataBufferSize;  // the actual buffer size getting from device
    uint64_t dataBufferSize;        // the data length of dataVec
    uint16_t nDevType;
    int32_t nZDTableSize;// = APC_ZD_TABLE_FILE_SIZE_11_BITS;
    std::vector<uint8_t> nZDTable;
    uint16_t getDepth( int x, int y) const;
    uint16_t getZValue(uint16_t depth) const;
    uint64_t processedBufferSize;   // e.g. 720p Decimation filter resized dataVec to 360p, but capacity is still the same.

    std::vector<uint8_t, libeYs3D::devices::AlignedAllocator<uint8_t>> dataVec;

    uint64_t actualZDDepthBufferSize;   // the actual buffer size when converting from raw data to ZD table depth
    uint64_t zdDepthBufferSize;         // the image buffer size of zdDepthVec

    std::vector<uint16_t, libeYs3D::devices::AlignedAllocator<uint16_t>> zdDepthVec;

    uint64_t actualRGBBufferSize;   // the actual buffer size when converting the image
    uint64_t rgbBufferSize;         // the image buffer size of imageVec

    std::vector<uint8_t, libeYs3D::devices::AlignedAllocator<uint8_t>> rgbVec;

    /*
     * for color frame:
     *     libeYs3D::video::COLOR_RAW_DATA_TYPE
     * for depth frame:
     *     ibeYs3D::video::DEPTH_RAW_DATA_TYPE
     */
    uint32_t dataFormat;
    
    /*
     * the format of RGB transcoding format 
     */
    uint32_t rgbFormat;

    /* for performance benchmark purpose in micro seconds */
    int64_t rgbTranscodingTimeUs;
    int64_t filteringTimeUs;
    
    libeYs3D::sensors::SensorDataSet sensorDataSet;
    
    /* for ROI support */
    int32_t roiDepth = 0;
    int32_t roiZValue = 0;
    
    union extra    {
        struct libeYs3D::devices::DepthAccuracyInfo depthAccuracyInfo;
        
        extra() { memset(this, 0, sizeof(extra)); }
        ~extra() = default;
    } extra;

    // Allocates the space with capacity |cap| and sets each element to |val|
    Frame(uint64_t dataBufferSize = 0, uint8_t initDataVal = 0,
          uint64_t zdDepthBufferSize = 0, uint16_t initZDDepthVal = 0,
          uint64_t rgbBufferSize = 0, uint8_t initRGBVal = 0);
    

    int toString(char *buffer, int bufferLength) const;
    int toStringSimple(char *buffer, int bufferLength) const;
    int toStringFull(char *buffer, int bufferLength) const;
#ifndef WIN32
    int toStringSHA256(char *buffer, int bufferLength) const;
#endif
    int toString(std::string &string) const;
    int saveToFile(const char *dirPath) const;
    
    void clone(const Frame *frame);

    // High-performance buffer swap (O(1) instead of O(n) memcpy)
    // Only swaps vector buffers, preserves metadata integrity
    void swapBuffersOnly(Frame& other) {
        dataVec.swap(other.dataVec);
        zdDepthVec.swap(other.zdDepthVec);
        rgbVec.swap(other.rgbVec);
        // No metadata swap - preserves frame identity
    }
    
    // Fast metadata copy for callback frame setup
    void copyMetadata(const Frame* source) {
        width = source->width;
        height = source->height;
        dataFormat = source->dataFormat;
        serialNumber = source->serialNumber;
        tsUs = source->tsUs;
        actualDataBufferSize = source->actualDataBufferSize;
        actualRGBBufferSize = source->actualRGBBufferSize;
        actualZDDepthBufferSize = source->actualZDDepthBufferSize;
        rgbFormat = source->rgbFormat;
        processedBufferSize = source->processedBufferSize;
        interleaveMode = source->interleaveMode;
    }

    // Move constructor
    Frame(Frame&& f) = default;
    // Move assignment
    Frame& operator=(Frame&& f) = default;
    
    // Do not define default assignment operator to prevent from misusing Frame assignment
    Frame& operator=(Frame& f) = delete;
    
    // hidden attributes for internal only
    bool toCallback = false;
    bool toPCCallback = false;
    bool interleaveMode = false;
};

} // namespace video
} // namespace libeYs3D
