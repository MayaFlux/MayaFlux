#pragma once
#include <vulkan/vulkan.hpp>

#include "MayaFlux/Core/GlobalGraphicsInfo.hpp"

namespace MayaFlux::Core {

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

} // namespace MF::VulkanEnumTranslator
