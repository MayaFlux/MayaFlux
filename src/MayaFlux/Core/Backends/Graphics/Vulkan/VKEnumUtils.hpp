#pragma once
#include <vulkan/vulkan.hpp>

#include "MayaFlux/Core/GlobalGraphicsInfo.hpp"

namespace MayaFlux::Core {

/**
 * @struct SurfaceFormatTraits
 * @brief DataVariant dispatch descriptor for a swapchain surface format.
 *
 * Byte count is intentionally absent — callers derive it from
 * vk_format_bytes_per_pixel(to_vk_format(fmt)) to avoid duplication.
 * Only the fields that cannot be trivially derived from the vk::Format
 * alone are stored here: the element-type discriminants needed to choose
 * the correct DataVariant alternative.
 */
struct SurfaceFormatTraits {
    uint32_t channel_count; ///< Number of colour channels.
    uint32_t bits_per_channel; ///< Bits per individual channel.
    bool is_float; ///< True for SFLOAT formats.
    bool is_packed; ///< True for packed formats (e.g. A2B10G10R10).
};

inline vk::Format to_vk_format(GraphicsSurfaceInfo::SurfaceFormat fmt)
{
    using SF = GraphicsSurfaceInfo::SurfaceFormat;
    switch (fmt) {
    case SF::B8G8R8A8_SRGB:
        return vk::Format::eB8G8R8A8Srgb;
    case SF::R8G8B8A8_SRGB:
        return vk::Format::eR8G8B8A8Srgb;
    case SF::B8G8R8A8_UNORM:
        return vk::Format::eB8G8R8A8Unorm;
    case SF::R8G8B8A8_UNORM:
        return vk::Format::eR8G8B8A8Unorm;
    case SF::R16G16B16A16_SFLOAT:
        return vk::Format::eR16G16B16A16Sfloat;
    case SF::A2B10G10R10_UNORM:
        return vk::Format::eA2B10G10R10UnormPack32;
    case SF::R32G32B32A32_SFLOAT:
        return vk::Format::eR32G32B32A32Sfloat;
    default:
        return vk::Format::eB8G8R8A8Srgb;
    }
}

inline vk::ColorSpaceKHR to_vk_color_space(GraphicsSurfaceInfo::ColorSpace space)
{
    using CS = GraphicsSurfaceInfo::ColorSpace;
    switch (space) {
    case CS::SRGB_NONLINEAR:
        return vk::ColorSpaceKHR::eSrgbNonlinear;
    case CS::EXTENDED_SRGB:
        return vk::ColorSpaceKHR::eExtendedSrgbLinearEXT;
    case CS::HDR10_ST2084:
        return vk::ColorSpaceKHR::eHdr10St2084EXT;
    case CS::DISPLAY_P3:
        return vk::ColorSpaceKHR::eDisplayP3NonlinearEXT;
    default:
        return vk::ColorSpaceKHR::eSrgbNonlinear;
    }
}

inline vk::PresentModeKHR to_vk_present_mode(GraphicsSurfaceInfo::PresentMode mode)
{
    using PM = GraphicsSurfaceInfo::PresentMode;
    switch (mode) {
    case PM::IMMEDIATE:
        return vk::PresentModeKHR::eImmediate;
    case PM::MAILBOX:
        return vk::PresentModeKHR::eMailbox;
    case PM::FIFO:
        return vk::PresentModeKHR::eFifo;
    case PM::FIFO_RELAXED:
        return vk::PresentModeKHR::eFifoRelaxed;
    default:
        return vk::PresentModeKHR::eFifo;
    }
}

/**
 * @brief Byte width of a single pixel for a given Vulkan format.
 *
 * Covers all formats currently tracked in GraphicsSurfaceInfo plus the full
 * set used by VKImage (depth, stencil, compute). Returns 4 with a warning for
 * unknown formats, matching the previous VKImage fallback behaviour.
 *
 * @param fmt Vulkan format.
 * @return Bytes per pixel, or 4 on unknown formats.
 */
inline uint32_t vk_format_bytes_per_pixel(vk::Format fmt)
{
    switch (fmt) {
    case vk::Format::eR8Unorm:
    case vk::Format::eR8Snorm:
    case vk::Format::eR8Uint:
    case vk::Format::eR8Sint:
        return 1U;

    case vk::Format::eR8G8Unorm:
    case vk::Format::eR8G8Snorm:
    case vk::Format::eR8G8Uint:
    case vk::Format::eR8G8Sint:
    case vk::Format::eR16Unorm:
    case vk::Format::eR16Snorm:
    case vk::Format::eR16Uint:
    case vk::Format::eR16Sint:
    case vk::Format::eR16Sfloat:
        return 2U;

    case vk::Format::eR8G8B8Unorm:
    case vk::Format::eR8G8B8Snorm:
    case vk::Format::eR8G8B8Uint:
    case vk::Format::eR8G8B8Sint:
    case vk::Format::eB8G8R8Unorm:
        return 3U;

    case vk::Format::eR8G8B8A8Unorm:
    case vk::Format::eR8G8B8A8Snorm:
    case vk::Format::eR8G8B8A8Uint:
    case vk::Format::eR8G8B8A8Sint:
    case vk::Format::eR8G8B8A8Srgb:
    case vk::Format::eB8G8R8A8Unorm:
    case vk::Format::eB8G8R8A8Srgb:
    case vk::Format::eR16G16Unorm:
    case vk::Format::eR16G16Snorm:
    case vk::Format::eR16G16Uint:
    case vk::Format::eR16G16Sint:
    case vk::Format::eR16G16Sfloat:
    case vk::Format::eR32Uint:
    case vk::Format::eR32Sint:
    case vk::Format::eR32Sfloat:
    case vk::Format::eD24UnormS8Uint:
    case vk::Format::eD32Sfloat:
    case vk::Format::eA2B10G10R10UnormPack32:
        return 4U;

    case vk::Format::eR16G16B16A16Unorm:
    case vk::Format::eR16G16B16A16Snorm:
    case vk::Format::eR16G16B16A16Uint:
    case vk::Format::eR16G16B16A16Sint:
    case vk::Format::eR16G16B16A16Sfloat:
    case vk::Format::eR32G32Uint:
    case vk::Format::eR32G32Sint:
    case vk::Format::eR32G32Sfloat:
        return 8U;

    case vk::Format::eR32G32B32Uint:
    case vk::Format::eR32G32B32Sint:
    case vk::Format::eR32G32B32Sfloat:
        return 12U;

    case vk::Format::eR32G32B32A32Uint:
    case vk::Format::eR32G32B32A32Sint:
    case vk::Format::eR32G32B32A32Sfloat:
        return 16U;

    default:
        return 4U;
    }
}

/**
 * @brief Reverse-map a vk::Format to the nearest GraphicsSurfaceInfo::SurfaceFormat.
 *        Returns B8G8R8A8_SRGB as a safe fallback for unmapped formats.
 * @param fmt Vulkan format obtained from the live swapchain.
 * @return Closest MayaFlux surface format enum value.
 */
inline GraphicsSurfaceInfo::SurfaceFormat from_vk_format(vk::Format fmt)
{
    using SF = GraphicsSurfaceInfo::SurfaceFormat;
    switch (fmt) {
    case vk::Format::eB8G8R8A8Srgb:
        return SF::B8G8R8A8_SRGB;
    case vk::Format::eR8G8B8A8Srgb:
        return SF::R8G8B8A8_SRGB;
    case vk::Format::eB8G8R8A8Unorm:
        return SF::B8G8R8A8_UNORM;
    case vk::Format::eR8G8B8A8Unorm:
        return SF::R8G8B8A8_UNORM;
    case vk::Format::eR16G16B16A16Sfloat:
        return SF::R16G16B16A16_SFLOAT;
    case vk::Format::eA2B10G10R10UnormPack32:
        return SF::A2B10G10R10_UNORM;
    case vk::Format::eR32G32B32A32Sfloat:
        return SF::R32G32B32A32_SFLOAT;
    default:
        return SF::B8G8R8A8_SRGB;
    }
}

/**
 * @brief Query DataVariant-dispatch traits for a surface format.
 *        For the byte count, call vk_format_bytes_per_pixel(to_vk_format(fmt)).
 * @param fmt The MayaFlux surface format enum value.
 * @return SurfaceFormatTraits for that format.
 */
inline SurfaceFormatTraits get_surface_format_traits(
    GraphicsSurfaceInfo::SurfaceFormat fmt)
{
    using SF = GraphicsSurfaceInfo::SurfaceFormat;
    switch (fmt) {
    case SF::B8G8R8A8_SRGB:
    case SF::R8G8B8A8_SRGB:
    case SF::B8G8R8A8_UNORM:
    case SF::R8G8B8A8_UNORM:
        return {
            .channel_count = 4U,
            .bits_per_channel = 8U,
            .is_float = false,
            .is_packed = false
        };

    case SF::R16G16B16A16_SFLOAT:
        return {
            .channel_count = 4U,
            .bits_per_channel = 16U,
            .is_float = true,
            .is_packed = false
        };

    case SF::A2B10G10R10_UNORM:
        return {
            .channel_count = 4U,
            .bits_per_channel = 10U,
            .is_float = false,
            .is_packed = true
        };

    case SF::R32G32B32A32_SFLOAT:
        return {
            .channel_count = 4U,
            .bits_per_channel = 32U,
            .is_float = true,
            .is_packed = false
        };

    default:
        return {
            .channel_count = 4U,
            .bits_per_channel = 8U,
            .is_float = false,
            .is_packed = false
        };
    }
}

} // namespace MF::VulkanEnumTranslator
