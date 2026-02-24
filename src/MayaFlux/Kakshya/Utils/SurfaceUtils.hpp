#pragma once

#include "MayaFlux/Core/GlobalGraphicsInfo.hpp"

#include "MayaFlux/Kakshya/NDData/DataAccess.hpp"
#include "MayaFlux/Kakshya/NDData/NDData.hpp"

namespace MayaFlux::Core {
class Window;
}

namespace MayaFlux::Kakshya {

/**
 * @brief Query the actual vk::Format in use by the window's live swapchain,
 *        translated back to the MayaFlux surface format enum.
 *
 * This is the ground truth for readback allocation — it reflects what the
 * driver actually negotiated, which may differ from the value requested in
 * GlobalGraphicsConfig when the preferred format was unavailable.
 *
 * @param window Target window.
 * @return Actual surface format. Returns B8G8R8A8_SRGB if the window has
 *         no registered swapchain.
 */
MAYAFLUX_API Core::GraphicsSurfaceInfo::SurfaceFormat query_surface_format(
    const std::shared_ptr<Core::Window>& window);

/**
 * @brief Read a pixel rectangle from the last completed swapchain frame into
 *        a DataVariant whose element type matches the live swapchain format.
 *
 * Format → DataVariant mapping:
 *   B8G8R8A8_SRGB / R8G8B8A8_SRGB / B8G8R8A8_UNORM / R8G8B8A8_UNORM
 *     → std::vector<uint8_t>  (4 bytes/pixel)
 *   R16G16B16A16_SFLOAT
 *     → std::vector<uint16_t> (8 bytes/pixel, raw half-float bits)
 *   A2B10G10R10_UNORM
 *     → std::vector<uint32_t> (4 bytes/pixel, packed word)
 *   R32G32B32A32_SFLOAT
 *     → std::vector<float>    (16 bytes/pixel)
 *
 * "Last completed frame" semantics: the swapchain image whose in-flight
 * fence has already signaled. Safe to call without stalling the render
 * pipeline.
 *
 * Dimensions on the returned DataAccess (IMAGE_COLOR convention):
 *   [0] SPATIAL_Y  — pixel_height
 *   [1] SPATIAL_X  — pixel_width
 *   [2] CHANNEL    — channel_count derived from format traits
 *
 * @param window       Window whose surface is being read.
 * @param x_offset     Left edge of the pixel rectangle (inclusive).
 * @param y_offset     Top edge of the pixel rectangle (inclusive).
 * @param pixel_width  Width of the rectangle in pixels.
 * @param pixel_height Height of the rectangle in pixels.
 * @param out_variant  Receives the typed pixel data. Left unchanged on
 *                     failure.
 * @return DataAccess wrapping out_variant with IMAGE_COLOR modality.
 *         Returns a default-constructed DataAccess on failure.
 */
MAYAFLUX_API DataAccess readback_region(
    const std::shared_ptr<Core::Window>& window,
    uint32_t x_offset,
    uint32_t y_offset,
    uint32_t pixel_width,
    uint32_t pixel_height,
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
 *        last in-flight fence has not yet signalled.
 * @param window Target window.
 * @return true if readback_region() can safely be called now.
 */
MAYAFLUX_API bool is_readback_available(
    const std::shared_ptr<Core::Window>& window);

} // namespace MayaFlux::Kakshya
