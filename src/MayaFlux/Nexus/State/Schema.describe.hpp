#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"

#ifndef MAYAFLUX_REFLECT_SCAN

namespace MayaFlux::Reflect::Generated {

template <>
struct Description<::MayaFlux::Nexus::State::AudioSinkRecord> {
    static constexpr std::string_view name = "MayaFlux::Nexus::State::AudioSinkRecord";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Nexus::State::AudioSinkRecord::channel> { "channel" },
            Member<&::MayaFlux::Nexus::State::AudioSinkRecord::fn_name> { "fn_name" },
        };
    }
};

template <>
struct Description<::MayaFlux::Nexus::State::EntityRecord> {
    static constexpr std::string_view name = "MayaFlux::Nexus::State::EntityRecord";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Nexus::State::EntityRecord::id> { "id" },
            Member<&::MayaFlux::Nexus::State::EntityRecord::kind> { "kind" },
            Member<&::MayaFlux::Nexus::State::EntityRecord::subkind> { "subkind" },
            Member<&::MayaFlux::Nexus::State::EntityRecord::position> { "position" },
            Member<&::MayaFlux::Nexus::State::EntityRecord::orientation> { "orientation" },
            Member<&::MayaFlux::Nexus::State::EntityRecord::intensity> { "intensity" },
            Member<&::MayaFlux::Nexus::State::EntityRecord::radius> { "radius" },
            Member<&::MayaFlux::Nexus::State::EntityRecord::query_radius> { "query_radius" },
            Member<&::MayaFlux::Nexus::State::EntityRecord::color> { "color" },
            Member<&::MayaFlux::Nexus::State::EntityRecord::size> { "size" },
            Member<&::MayaFlux::Nexus::State::EntityRecord::influence_fn_name> { "influence_fn_name" },
            Member<&::MayaFlux::Nexus::State::EntityRecord::perception_fn_name> { "perception_fn_name" },
            Member<&::MayaFlux::Nexus::State::EntityRecord::radiate_fn_name> { "radiate_fn_name" },
            Member<&::MayaFlux::Nexus::State::EntityRecord::wiring> { "wiring" },
            Member<&::MayaFlux::Nexus::State::EntityRecord::audio_sinks> { "audio_sinks" },
            Member<&::MayaFlux::Nexus::State::EntityRecord::render_sinks> { "render_sinks" },
            Member<&::MayaFlux::Nexus::State::EntityRecord::locus_nav> { "locus_nav" },
            Member<&::MayaFlux::Nexus::State::EntityRecord::falloff_curve_name> { "falloff_curve_name" },
            Member<&::MayaFlux::Nexus::State::EntityRecord::falloff_radius> { "falloff_radius" },
        };
    }
};

template <>
struct Description<::MayaFlux::Nexus::State::ExpanseRecord> {
    static constexpr std::string_view name = "MayaFlux::Nexus::State::ExpanseRecord";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Nexus::State::ExpanseRecord::id> { "id" },
            Member<&::MayaFlux::Nexus::State::ExpanseRecord::subkind> { "subkind" },
            Member<&::MayaFlux::Nexus::State::ExpanseRecord::fn_name> { "fn_name" },
            Member<&::MayaFlux::Nexus::State::ExpanseRecord::on_enter_fn_name> { "on_enter_fn_name" },
            Member<&::MayaFlux::Nexus::State::ExpanseRecord::on_exit_fn_name> { "on_exit_fn_name" },
            Member<&::MayaFlux::Nexus::State::ExpanseRecord::bounds_min> { "bounds_min" },
            Member<&::MayaFlux::Nexus::State::ExpanseRecord::bounds_max> { "bounds_max" },
            Member<&::MayaFlux::Nexus::State::ExpanseRecord::collection> { "collection" },
            Member<&::MayaFlux::Nexus::State::ExpanseRecord::fit_name> { "fit_name" },
        };
    }
};

template <>
struct Description<::MayaFlux::Nexus::State::FabricRef> {
    static constexpr std::string_view name = "MayaFlux::Nexus::State::FabricRef";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Nexus::State::FabricRef::name> { "name" },
            Member<&::MayaFlux::Nexus::State::FabricRef::base_path> { "base_path" },
        };
    }
};

template <>
struct Description<::MayaFlux::Nexus::State::FabricSchema> {
    static constexpr std::string_view name = "MayaFlux::Nexus::State::FabricSchema";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Nexus::State::FabricSchema::version> { "version" },
            Member<&::MayaFlux::Nexus::State::FabricSchema::fabric_name> { "fabric_name" },
            Member<&::MayaFlux::Nexus::State::FabricSchema::entities> { "entities" },
            Member<&::MayaFlux::Nexus::State::FabricSchema::expanses> { "expanses" },
            Member<&::MayaFlux::Nexus::State::FabricSchema::ranges> { "ranges" },
            Member<&::MayaFlux::Nexus::State::FabricSchema::cell_size> { "cell_size" },
        };
    }
};

template <>
struct Description<::MayaFlux::Nexus::State::LocusNavRecord> {
    static constexpr std::string_view name = "MayaFlux::Nexus::State::LocusNavRecord";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Nexus::State::LocusNavRecord::eye> { "eye" },
            Member<&::MayaFlux::Nexus::State::LocusNavRecord::target> { "target" },
            Member<&::MayaFlux::Nexus::State::LocusNavRecord::fov> { "fov" },
            Member<&::MayaFlux::Nexus::State::LocusNavRecord::near_plane> { "near_plane" },
            Member<&::MayaFlux::Nexus::State::LocusNavRecord::far_plane> { "far_plane" },
            Member<&::MayaFlux::Nexus::State::LocusNavRecord::speed> { "speed" },
            Member<&::MayaFlux::Nexus::State::LocusNavRecord::mouse_sensitivity> { "mouse_sensitivity" },
            Member<&::MayaFlux::Nexus::State::LocusNavRecord::scroll_speed> { "scroll_speed" },
            Member<&::MayaFlux::Nexus::State::LocusNavRecord::aspect> { "aspect" },
        };
    }
};

template <>
struct Description<::MayaFlux::Nexus::State::Range> {
    static constexpr std::string_view name = "MayaFlux::Nexus::State::Range";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Nexus::State::Range::min> { "min" },
            Member<&::MayaFlux::Nexus::State::Range::max> { "max" },
        };
    }
};

template <>
struct Description<::MayaFlux::Nexus::State::RangeSet> {
    static constexpr std::string_view name = "MayaFlux::Nexus::State::RangeSet";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Nexus::State::RangeSet::pos_x> { "pos_x" },
            Member<&::MayaFlux::Nexus::State::RangeSet::pos_y> { "pos_y" },
            Member<&::MayaFlux::Nexus::State::RangeSet::pos_z> { "pos_z" },
            Member<&::MayaFlux::Nexus::State::RangeSet::intensity> { "intensity" },
            Member<&::MayaFlux::Nexus::State::RangeSet::color_r> { "color_r" },
            Member<&::MayaFlux::Nexus::State::RangeSet::color_g> { "color_g" },
            Member<&::MayaFlux::Nexus::State::RangeSet::color_b> { "color_b" },
            Member<&::MayaFlux::Nexus::State::RangeSet::size> { "size" },
            Member<&::MayaFlux::Nexus::State::RangeSet::radius> { "radius" },
            Member<&::MayaFlux::Nexus::State::RangeSet::query_radius> { "query_radius" },
        };
    }
};

template <>
struct Description<::MayaFlux::Nexus::State::RenderSinkRecord> {
    static constexpr std::string_view name = "MayaFlux::Nexus::State::RenderSinkRecord";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Nexus::State::RenderSinkRecord::fn_name> { "fn_name" },
            Member<&::MayaFlux::Nexus::State::RenderSinkRecord::vertex_shader> { "vertex_shader" },
            Member<&::MayaFlux::Nexus::State::RenderSinkRecord::fragment_shader> { "fragment_shader" },
            Member<&::MayaFlux::Nexus::State::RenderSinkRecord::geometry_shader> { "geometry_shader" },
            Member<&::MayaFlux::Nexus::State::RenderSinkRecord::default_texture_binding> { "default_texture_binding" },
            Member<&::MayaFlux::Nexus::State::RenderSinkRecord::topology> { "topology" },
            Member<&::MayaFlux::Nexus::State::RenderSinkRecord::polygon_mode> { "polygon_mode" },
            Member<&::MayaFlux::Nexus::State::RenderSinkRecord::cull_mode> { "cull_mode" },
            Member<&::MayaFlux::Nexus::State::RenderSinkRecord::triangulate> { "triangulate" },
            Member<&::MayaFlux::Nexus::State::RenderSinkRecord::draw_priority> { "draw_priority" },
            Member<&::MayaFlux::Nexus::State::RenderSinkRecord::extra_string_params> { "extra_string_params" },
        };
    }
};

template <>
struct Description<::MayaFlux::Nexus::State::TapestryExpanseRecord> {
    static constexpr std::string_view name = "MayaFlux::Nexus::State::TapestryExpanseRecord";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Nexus::State::TapestryExpanseRecord::name> { "name" },
            Member<&::MayaFlux::Nexus::State::TapestryExpanseRecord::subkind> { "subkind" },
            Member<&::MayaFlux::Nexus::State::TapestryExpanseRecord::fn_name> { "fn_name" },
            Member<&::MayaFlux::Nexus::State::TapestryExpanseRecord::on_enter_fn_name> { "on_enter_fn_name" },
            Member<&::MayaFlux::Nexus::State::TapestryExpanseRecord::on_exit_fn_name> { "on_exit_fn_name" },
            Member<&::MayaFlux::Nexus::State::TapestryExpanseRecord::bounds_min> { "bounds_min" },
            Member<&::MayaFlux::Nexus::State::TapestryExpanseRecord::bounds_max> { "bounds_max" },
            Member<&::MayaFlux::Nexus::State::TapestryExpanseRecord::collection> { "collection" },
            Member<&::MayaFlux::Nexus::State::TapestryExpanseRecord::fit_name> { "fit_name" },
            Member<&::MayaFlux::Nexus::State::TapestryExpanseRecord::fabric_names> { "fabric_names" },
        };
    }
};

template <>
struct Description<::MayaFlux::Nexus::State::TapestrySchema> {
    static constexpr std::string_view name = "MayaFlux::Nexus::State::TapestrySchema";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Nexus::State::TapestrySchema::version> { "version" },
            Member<&::MayaFlux::Nexus::State::TapestrySchema::fabrics> { "fabrics" },
            Member<&::MayaFlux::Nexus::State::TapestrySchema::expanses> { "expanses" },
            Member<&::MayaFlux::Nexus::State::TapestrySchema::user_state> { "user_state" },
        };
    }
};

template <>
struct Description<::MayaFlux::Nexus::State::WiringRecord> {
    static constexpr std::string_view name = "MayaFlux::Nexus::State::WiringRecord";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Nexus::State::WiringRecord::kind> { "kind" },
            Member<&::MayaFlux::Nexus::State::WiringRecord::interval> { "interval" },
            Member<&::MayaFlux::Nexus::State::WiringRecord::duration> { "duration" },
            Member<&::MayaFlux::Nexus::State::WiringRecord::times> { "times" },
            Member<&::MayaFlux::Nexus::State::WiringRecord::steps> { "steps" },
            Member<&::MayaFlux::Nexus::State::WiringRecord::interval_token> { "interval_token" },
            Member<&::MayaFlux::Nexus::State::WiringRecord::duration_token> { "duration_token" },
            Member<&::MayaFlux::Nexus::State::WiringRecord::key> { "key" },
            Member<&::MayaFlux::Nexus::State::WiringRecord::button> { "button" },
            Member<&::MayaFlux::Nexus::State::WiringRecord::held> { "held" },
            Member<&::MayaFlux::Nexus::State::WiringRecord::position_fn_name> { "position_fn_name" },
            Member<&::MayaFlux::Nexus::State::WiringRecord::factory_name> { "factory_name" },
            Member<&::MayaFlux::Nexus::State::WiringRecord::attach_fn_name> { "attach_fn_name" },
            Member<&::MayaFlux::Nexus::State::WiringRecord::detach_fn_name> { "detach_fn_name" },
        };
    }
};

template <>
struct Description<::MayaFlux::Nexus::State::WiringStep> {
    static constexpr std::string_view name = "MayaFlux::Nexus::State::WiringStep";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Nexus::State::WiringStep::position> { "position" },
            Member<&::MayaFlux::Nexus::State::WiringStep::delay_seconds> { "delay_seconds" },
        };
    }
};

}

#endif
