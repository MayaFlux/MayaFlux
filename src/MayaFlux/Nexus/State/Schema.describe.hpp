#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"

#ifndef MAYAFLUX_REFLECT_SCAN

namespace MayaFlux::Reflect::Generated {

template <>
struct Description<::MayaFlux::Nexus::State::AttachRecord> {
    static constexpr std::string_view name = "MayaFlux::Nexus::State::AttachRecord";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Nexus::State::AttachRecord::buffer> { "buffer" },
            Member<&::MayaFlux::Nexus::State::AttachRecord::index> { "index" },
            Member<&::MayaFlux::Nexus::State::AttachRecord::offset> { "offset" },
            Member<&::MayaFlux::Nexus::State::AttachRecord::on_move> { "on_move" },
        };
    }
};

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
struct Description<::MayaFlux::Nexus::State::CallRecord> {
    static constexpr std::string_view name = "MayaFlux::Nexus::State::CallRecord";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Nexus::State::CallRecord::name> { "name" },
            Member<&::MayaFlux::Nexus::State::CallRecord::arguments> { "arguments" },
        };
    }
};

template <>
struct Description<::MayaFlux::Nexus::State::ComponentRecord> {
    static constexpr std::string_view name = "MayaFlux::Nexus::State::ComponentRecord";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Nexus::State::ComponentRecord::name> { "name" },
            Member<&::MayaFlux::Nexus::State::ComponentRecord::component> { "component" },
            Member<&::MayaFlux::Nexus::State::ComponentRecord::range> { "range" },
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
            Member<&::MayaFlux::Nexus::State::EntityRecord::controls> { "controls" },
            Member<&::MayaFlux::Nexus::State::EntityRecord::influence> { "influence" },
            Member<&::MayaFlux::Nexus::State::EntityRecord::perception> { "perception" },
            Member<&::MayaFlux::Nexus::State::EntityRecord::radiate> { "radiate" },
            Member<&::MayaFlux::Nexus::State::EntityRecord::wiring> { "wiring" },
            Member<&::MayaFlux::Nexus::State::EntityRecord::attach> { "attach" },
            Member<&::MayaFlux::Nexus::State::EntityRecord::view_targets> { "view_targets" },
            Member<&::MayaFlux::Nexus::State::EntityRecord::audio_sinks> { "audio_sinks" },
            Member<&::MayaFlux::Nexus::State::EntityRecord::render_sinks> { "render_sinks" },
            Member<&::MayaFlux::Nexus::State::EntityRecord::locus_nav> { "locus_nav" },
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
            Member<&::MayaFlux::Nexus::State::ExpanseRecord::contains> { "contains" },
            Member<&::MayaFlux::Nexus::State::ExpanseRecord::on_enter> { "on_enter" },
            Member<&::MayaFlux::Nexus::State::ExpanseRecord::on_exit> { "on_exit" },
            Member<&::MayaFlux::Nexus::State::ExpanseRecord::buffer> { "buffer" },
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
            Member<&::MayaFlux::Nexus::State::FabricSchema::components> { "components" },
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
struct Description<::MayaFlux::Nexus::State::RenderSinkRecord> {
    static constexpr std::string_view name = "MayaFlux::Nexus::State::RenderSinkRecord";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Nexus::State::RenderSinkRecord::fn_name> { "fn_name" },
            Member<&::MayaFlux::Nexus::State::RenderSinkRecord::window> { "window" },
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
            Member<&::MayaFlux::Nexus::State::TapestryExpanseRecord::contains> { "contains" },
            Member<&::MayaFlux::Nexus::State::TapestryExpanseRecord::on_enter> { "on_enter" },
            Member<&::MayaFlux::Nexus::State::TapestryExpanseRecord::on_exit> { "on_exit" },
            Member<&::MayaFlux::Nexus::State::TapestryExpanseRecord::buffer> { "buffer" },
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
struct Description<::MayaFlux::Nexus::State::TriggerRecord> {
    static constexpr std::string_view name = "MayaFlux::Nexus::State::TriggerRecord";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Nexus::State::TriggerRecord::kind> { "kind" },
            Member<&::MayaFlux::Nexus::State::TriggerRecord::source> { "source" },
            Member<&::MayaFlux::Nexus::State::TriggerRecord::key> { "key" },
            Member<&::MayaFlux::Nexus::State::TriggerRecord::modifiers> { "modifiers" },
            Member<&::MayaFlux::Nexus::State::TriggerRecord::button> { "button" },
            Member<&::MayaFlux::Nexus::State::TriggerRecord::event_type> { "event_type" },
            Member<&::MayaFlux::Nexus::State::TriggerRecord::held> { "held" },
            Member<&::MayaFlux::Nexus::State::TriggerRecord::callback> { "callback" },
        };
    }
};

template <>
struct Description<::MayaFlux::Nexus::State::WiringRecord> {
    static constexpr std::string_view name = "MayaFlux::Nexus::State::WiringRecord";

    static constexpr auto members()
    {
        return std::tuple {
            Member<&::MayaFlux::Nexus::State::WiringRecord::interval> { "interval" },
            Member<&::MayaFlux::Nexus::State::WiringRecord::duration> { "duration" },
            Member<&::MayaFlux::Nexus::State::WiringRecord::times> { "times" },
            Member<&::MayaFlux::Nexus::State::WiringRecord::steps> { "steps" },
            Member<&::MayaFlux::Nexus::State::WiringRecord::interval_token> { "interval_token" },
            Member<&::MayaFlux::Nexus::State::WiringRecord::duration_token> { "duration_token" },
            Member<&::MayaFlux::Nexus::State::WiringRecord::triggers> { "triggers" },
            Member<&::MayaFlux::Nexus::State::WiringRecord::factory_kind> { "factory_kind" },
            Member<&::MayaFlux::Nexus::State::WiringRecord::factory> { "factory" },
            Member<&::MayaFlux::Nexus::State::WiringRecord::place> { "place" },
            Member<&::MayaFlux::Nexus::State::WiringRecord::bound> { "bound" },
            Member<&::MayaFlux::Nexus::State::WiringRecord::bound_to_entity> { "bound_to_entity" },
            Member<&::MayaFlux::Nexus::State::WiringRecord::bind_attach> { "bind_attach" },
            Member<&::MayaFlux::Nexus::State::WiringRecord::bind_detach> { "bind_detach" },
            Member<&::MayaFlux::Nexus::State::WiringRecord::on_complete> { "on_complete" },
            Member<&::MayaFlux::Nexus::State::WiringRecord::on_detach> { "on_detach" },
            Member<&::MayaFlux::Nexus::State::WiringRecord::lost> { "lost" },
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
