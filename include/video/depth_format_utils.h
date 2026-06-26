/*
 * Centralized classification for eSP936 (ORANGE / GRAPE, module 80363) depth formats.
 *
 * Recognizes BOTH firmware generations:
 *     legacy : 0x18 0x19 0x1a 0x1b
 *     new(V2): 0x48 0x49 0x4a 0x4b
 *
 * The depth-format value is delivered verbatim from ModeConfig.db and is firmware-specific
 * (the user installs the DB matching the device firmware). These helpers are the single place
 * that knows which raw values mean what -- add any future generation HERE only, never by
 * re-introducing per-call-site switches.
 *
 * Both functions are a single switch (jump-table friendly): orangeDepthFormatToImageType() is
 * used on the per-pixel decode path via the per-frame cache in Frame, so keep it cheap.
 */
#pragma once

#include <cstdint>         // uint32_t
#include "video/video.h"   // DEPTH_RAW_DATA_TYPE values; also brings APCImageType (eSPDI_def.h)

namespace libeYs3D {
namespace video {

// Map an eSP936 depth format (either firmware generation) to its APCImageType.
// Returns IMAGE_UNKNOWN for any value that is not an ORANGE depth format.
inline APCImageType::Value orangeDepthFormatToImageType(uint32_t depthFormat) {
    switch (depthFormat) {
        case DEPTH_RAW_DATA_ORANGE_11_BITS:
        case DEPTH_RAW_DATA_ORANGE_11_BITS_ILM:
        case DEPTH_RAW_DATA_ORANGE_11_BITS_V2:
        case DEPTH_RAW_DATA_ORANGE_11_BITS_ILM_V2:
            return APCImageType::DEPTH_11BITS;
        case DEPTH_RAW_DATA_ORANGE_14_BITS:
        case DEPTH_RAW_DATA_ORANGE_14_BITS_ILM:
        case DEPTH_RAW_DATA_ORANGE_14_BITS_V2:
        case DEPTH_RAW_DATA_ORANGE_14_BITS_ILM_V2:
            return APCImageType::DEPTH_14BITS;
        default:
            return APCImageType::IMAGE_UNKNOWN;
    }
}

// True if the eSP936 depth format is an interleave-mode (ILM) format, either generation.
inline bool isOrangeDepthFormatILM(uint32_t depthFormat) {
    switch (depthFormat) {
        case DEPTH_RAW_DATA_ORANGE_11_BITS_ILM:
        case DEPTH_RAW_DATA_ORANGE_14_BITS_ILM:
        case DEPTH_RAW_DATA_ORANGE_11_BITS_ILM_V2:
        case DEPTH_RAW_DATA_ORANGE_14_BITS_ILM_V2:
            return true;
        default:
            return false;
    }
}

} // namespace video
} // namespace libeYs3D
