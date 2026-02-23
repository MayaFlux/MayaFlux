#pragma once

#include "MayaFlux/Kakshya/NDData/DataAccess.hpp"
#include "MayaFlux/Kakshya/NDData/NDData.hpp"

namespace MayaFlux::Core {
class Window;
}

namespace MayaFlux::Kakshya {

/**
 * @brief Read a pixel rectangle from the last completed swapchain frame
 *        of the given window into a DataVariant (std::vector<uint8_t>).
 *
 * "Last completed frame" semantics: the swapchain image whose in-flight
 * fence has already signaled. Safe to read without stalling the render
 * pipeline. Modality of the returned DataAccess is IMAGE_COLOR.
 *
 * Dimensions on the returned DataAccess:
 *   [0] SPATIAL_X  — pixel_width
 *   [1] SPATIAL_Y  — pixel_height
 *   [2] CHANNEL    — channel_count
 *
 * @param window        Window whose surface is being read.
 * @param x_offset      Left edge of the pixel rectangle (inclusive).
 * @param y_offset      Top edge of the pixel rectangle (inclusive).
 * @param pixel_width   Width of the rectangle in pixels.
 * @param pixel_height  Height of the rectangle in pixels.
 * @param channel_count Channels per pixel (4 for RGBA8, 3 for RGB8).
 * @param out_variant   Receives std::vector<uint8_t> of size
 *                      pixel_width * pixel_height * channel_count.
 *                      Left unchanged on failure.
 * @return DataAccess wrapping out_variant with IMAGE_COLOR modality.
 *         Returns a default-constructed DataAccess on failure.
 */
MAYAFLUX_API DataAccess readback_region(
    const std::shared_ptr<Core::Window>& window,
    uint32_t x_offset,
    uint32_t y_offset,
    uint32_t pixel_width,
    uint32_t pixel_height,
    uint32_t channel_count,
    DataVariant& out_variant);

/**
 * @brief Query the current pixel dimensions of the window's swapchain.
 * @param window Target window.
 * @return {width, height} in pixels. Returns {0, 0} if the window is not
 *         registered with the graphics backend.
 */
MAYAFLUX_API std::pair<uint32_t, uint32_t> query_surface_extent(
    const std::shared_ptr<Core::Window>& window);

/**
 * @brief Check whether a completed frame is currently available for readback.
 *        Returns false if the window has no registered swapchain or if the
 *        last in-flight fence has not yet signaled.
 * @param window Target window.
 * @return true if readback_region() can safely be called now.
 */
MAYAFLUX_API bool is_readback_available(
    const std::shared_ptr<Core::Window>& window);

} // namespace MayaFlux::Kakshya
