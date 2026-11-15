#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"

#ifndef MAYAFLUX_REFLECT_SCAN

namespace MayaFlux::Reflect::Generated {

template <>
struct Description<::MayaFlux::Core::GlobalGraphicsConfig> {
    static constexpr std::string_view name = "MayaFlux::Core::GlobalGraphicsConfig";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Core::GlobalGraphicsConfig::key_repeat_config> { "key_repeat_config" },
            Member<&::MayaFlux::Core::GlobalGraphicsConfig::surface_info> { "surface_info" },
            Member<&::MayaFlux::Core::GlobalGraphicsConfig::backend_info> { "backend_info" },
            Member<&::MayaFlux::Core::GlobalGraphicsConfig::resource_limits> { "resource_limits" },
            Member<&::MayaFlux::Core::GlobalGraphicsConfig::target_frame_rate> { "target_frame_rate" },
            Member<&::MayaFlux::Core::GlobalGraphicsConfig::windowing_backend> { "windowing_backend" },
            Member<&::MayaFlux::Core::GlobalGraphicsConfig::requested_api> { "requested_api" },
            Member<&::MayaFlux::Core::GlobalGraphicsConfig::text_config> { "text_config" },
        };
    }
};

template <>
struct Description<::MayaFlux::Core::GraphicsBackendInfo> {
    static constexpr std::string_view name = "MayaFlux::Core::GraphicsBackendInfo";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Core::GraphicsBackendInfo::enable_validation> { "enable_validation" },
            Member<&::MayaFlux::Core::GraphicsBackendInfo::enable_debug_markers> { "enable_debug_markers" },
            Member<&::MayaFlux::Core::GraphicsBackendInfo::required_features> { "required_features" },
            Member<&::MayaFlux::Core::GraphicsBackendInfo::device_preference> { "device_preference" },
            Member<&::MayaFlux::Core::GraphicsBackendInfo::device_index> { "device_index" },
            Member<&::MayaFlux::Core::GraphicsBackendInfo::device_uuid> { "device_uuid" },
            Member<&::MayaFlux::Core::GraphicsBackendInfo::device_name> { "device_name" },
            Member<&::MayaFlux::Core::GraphicsBackendInfo::require_presentation> { "require_presentation" },
            Member<&::MayaFlux::Core::GraphicsBackendInfo::strict_device_selection> { "strict_device_selection" },
            Member<&::MayaFlux::Core::GraphicsBackendInfo::memory_strategy> { "memory_strategy" },
            Member<&::MayaFlux::Core::GraphicsBackendInfo::command_pooling> { "command_pooling" },
            Member<&::MayaFlux::Core::GraphicsBackendInfo::enable_compute_queue> { "enable_compute_queue" },
            Member<&::MayaFlux::Core::GraphicsBackendInfo::enable_transfer_queue> { "enable_transfer_queue" },
            Member<&::MayaFlux::Core::GraphicsBackendInfo::shader_compilation> { "shader_compilation" },
            Member<&::MayaFlux::Core::GraphicsBackendInfo::shader_cache_dir> { "shader_cache_dir" },
            Member<&::MayaFlux::Core::GraphicsBackendInfo::required_extensions> { "required_extensions" },
            Member<&::MayaFlux::Core::GraphicsBackendInfo::optional_extensions> { "optional_extensions" },
        };
    }
};

template <>
struct Description<::MayaFlux::Core::GraphicsBackendInfo::RequiredFeatures> {
    static constexpr std::string_view name = "MayaFlux::Core::GraphicsBackendInfo::RequiredFeatures";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Core::GraphicsBackendInfo::RequiredFeatures::compute_shaders> { "compute_shaders" },
            Member<&::MayaFlux::Core::GraphicsBackendInfo::RequiredFeatures::geometry_shaders> { "geometry_shaders" },
            Member<&::MayaFlux::Core::GraphicsBackendInfo::RequiredFeatures::tessellation_shaders> { "tessellation_shaders" },
            Member<&::MayaFlux::Core::GraphicsBackendInfo::RequiredFeatures::multi_viewport> { "multi_viewport" },
            Member<&::MayaFlux::Core::GraphicsBackendInfo::RequiredFeatures::sampler_anisotropy> { "sampler_anisotropy" },
            Member<&::MayaFlux::Core::GraphicsBackendInfo::RequiredFeatures::fill_mode_non_solid> { "fill_mode_non_solid" },
        };
    }
};

template <>
struct Description<::MayaFlux::Core::GraphicsResourceLimits> {
    static constexpr std::string_view name = "MayaFlux::Core::GraphicsResourceLimits";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Core::GraphicsResourceLimits::max_windows> { "max_windows" },
            Member<&::MayaFlux::Core::GraphicsResourceLimits::max_staging_buffer_mb> { "max_staging_buffer_mb" },
            Member<&::MayaFlux::Core::GraphicsResourceLimits::max_compute_buffer_mb> { "max_compute_buffer_mb" },
            Member<&::MayaFlux::Core::GraphicsResourceLimits::max_texture_cache_mb> { "max_texture_cache_mb" },
            Member<&::MayaFlux::Core::GraphicsResourceLimits::max_descriptor_sets> { "max_descriptor_sets" },
            Member<&::MayaFlux::Core::GraphicsResourceLimits::max_pipelines> { "max_pipelines" },
        };
    }
};

template <>
struct Description<::MayaFlux::Core::GraphicsSurfaceInfo> {
    static constexpr std::string_view name = "MayaFlux::Core::GraphicsSurfaceInfo";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Core::GraphicsSurfaceInfo::format> { "format" },
            Member<&::MayaFlux::Core::GraphicsSurfaceInfo::color_space> { "color_space" },
            Member<&::MayaFlux::Core::GraphicsSurfaceInfo::present_mode> { "present_mode" },
            Member<&::MayaFlux::Core::GraphicsSurfaceInfo::image_count> { "image_count" },
            Member<&::MayaFlux::Core::GraphicsSurfaceInfo::enable_regions> { "enable_regions" },
            Member<&::MayaFlux::Core::GraphicsSurfaceInfo::max_regions_per_window> { "max_regions_per_window" },
            Member<&::MayaFlux::Core::GraphicsSurfaceInfo::enable_hdr> { "enable_hdr" },
            Member<&::MayaFlux::Core::GraphicsSurfaceInfo::measure_frame_time> { "measure_frame_time" },
            Member<&::MayaFlux::Core::GraphicsSurfaceInfo::backend_options> { "backend_options" },
        };
    }
};

template <>
struct Description<::MayaFlux::Core::KeyRepeatConfig> {
    static constexpr std::string_view name = "MayaFlux::Core::KeyRepeatConfig";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Core::KeyRepeatConfig::initial_delay_ms> { "initial_delay_ms" },
            Member<&::MayaFlux::Core::KeyRepeatConfig::interval_ms> { "interval_ms" },
            Member<&::MayaFlux::Core::KeyRepeatConfig::text_initial_delay_ms> { "text_initial_delay_ms" },
            Member<&::MayaFlux::Core::KeyRepeatConfig::text_interval_ms> { "text_interval_ms" },
            Member<&::MayaFlux::Core::KeyRepeatConfig::allow_compositor_override> { "allow_compositor_override" },
        };
    }
};

template <>
struct Description<::MayaFlux::Core::TextConfig> {
    static constexpr std::string_view name = "MayaFlux::Core::TextConfig";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Core::TextConfig::family> { "family" },
            Member<&::MayaFlux::Core::TextConfig::style> { "style" },
            Member<&::MayaFlux::Core::TextConfig::pixel_size> { "pixel_size" },
            Member<&::MayaFlux::Core::TextConfig::atlas_size> { "atlas_size" },
        };
    }
};

}

#endif
