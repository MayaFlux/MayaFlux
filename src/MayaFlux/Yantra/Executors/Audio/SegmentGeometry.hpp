#pragma once

namespace MayaFlux::Yantra {

/**
 * @struct SegmentGeometry
 * @brief Where the windows of a GPU pass sit in the value array.
 *
 * Window s covers length elements starting at offset + s * stride. A stride
 * equal to length tiles the array; a smaller stride overlaps windows.
 */
struct SegmentGeometry {
    uint32_t length { 0 };
    uint32_t stride { 0 };
    uint32_t offset { 0 };
    uint32_t count { 0 };
};

} // namespace MayaFlux::Yantra
