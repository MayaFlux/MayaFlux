#pragma once

#include "MayaFlux/Nexus/Fabric.hpp"
#include "MayaFlux/Portal/Graphics/GraphicsUtils.hpp"
#include "MayaFlux/Transitive/Reflect/EnumReflect.hpp"

#include <nlohmann/json.hpp>

namespace MayaFlux::Nexus::State {

/**
 * @brief Current schema version written by StateEncoder and accepted by StateDecoder.
 *
 * History:
 *   1 - initial (positions only)
 *   2 - added color, size, wiring
 *   3 - added audio_sinks, render_sinks
 *   4 - added sink_type pixel row
 *   5 - added subkind, locus_nav fields, expanses array
 *   6 - entity position optional, entity orientation; locus_nav mouse_sensitivity,
 *       scroll_speed, aspect (up removed); wiring sources (key, mouse, network,
 *       window_event, factory, bind), scheduler tokens and callable names;
 *       render sink RenderConfig data; Expanse subkind (hull, mantle), collection
 *       and fit; Fabric cell_size
 */
inline constexpr uint32_t k_schema_version = 6;

/**
 * @brief Oldest schema version StateDecoder still reads. Every field added since
 *        is optional or defaulted, so an older file reads with those fields absent.
 */
inline constexpr uint32_t k_schema_min_version = 5;

/**
 * @brief RGBA32F EXR layout constants shared between encoder and decoder.
 *
 * Row 0: position.xyz, intensity
 * Row 1: color.rgb, size
 * Row 2: radius, query_radius, 0, 0
 * Row 3: id, entity_type_norm, trigger_kind, time_kind
 * Row 4: sink_type_bits, first_audio_channel, 0, 0
 *
 * The decoder reads rows 0 to 2. Rows 3 and 4 are descriptive only. The file's
 * channels are named A, B, G, R as EXR requires, so the four values of a pixel
 * listed above are channels A, B, G, R in that order.
 */
inline constexpr uint32_t k_exr_rows = 5;
inline constexpr uint32_t k_channels = 4;

// =============================================================================
// Range normalization records
// =============================================================================

struct Range {
    float min { 0.0F };
    float max { 1.0F };

    static constexpr auto describe()
    {
        return std::make_tuple(
            Reflect::member("min", &Range::min),
            Reflect::member("max", &Range::max));
    }
};

struct RangeSet {
    Range pos_x, pos_y, pos_z;
    Range intensity;
    Range color_r, color_g, color_b;
    Range size;
    Range radius;
    Range query_radius;

    static constexpr auto describe()
    {
        return std::make_tuple(
            Reflect::member("position.x", &RangeSet::pos_x),
            Reflect::member("position.y", &RangeSet::pos_y),
            Reflect::member("position.z", &RangeSet::pos_z),
            Reflect::member("intensity", &RangeSet::intensity),
            Reflect::member("color.r", &RangeSet::color_r),
            Reflect::member("color.g", &RangeSet::color_g),
            Reflect::member("color.b", &RangeSet::color_b),
            Reflect::member("size", &RangeSet::size),
            Reflect::member("radius", &RangeSet::radius),
            Reflect::member("query_radius", &RangeSet::query_radius));
    }
};

// =============================================================================
// Wiring records
// =============================================================================

/**
 * @brief Serializable wiring strategies.
 *
 * Written as the underlying integer, so new enumerators are only ever
 * appended. Key, Mouse, Network, WindowEvent, Factory and Bind describe a wiring
 * whose source is live (a window, a network source, a callable) and is not
 * encoded; the record keeps what is plain data. Unsupported marks a wiring the
 * encoder could not describe.
 */
enum class WiringKind : uint8_t {
    CommitDriven,
    Every,
    MoveTo,
    Scroll,
    Unsupported,
    Key,
    Mouse,
    Network,
    WindowEvent,
    Factory,
    Bind,
};

struct WiringStep {
    glm::vec3 position {};
    double delay_seconds { 0.0 };

    static constexpr auto describe()
    {
        return std::make_tuple(
            Reflect::member("position", &WiringStep::position),
            Reflect::member("delay", &WiringStep::delay_seconds));
    }
};

struct WiringRecord {
    WiringKind kind { WiringKind::CommitDriven };
    std::optional<double> interval;
    std::optional<double> duration;
    std::optional<size_t> times;
    std::optional<std::vector<WiringStep>> steps;
    std::optional<Vruta::ProcessingToken> interval_token;
    std::optional<Vruta::ProcessingToken> duration_token;
    std::optional<IO::Keys> key;
    std::optional<IO::MouseButtons> button;
    std::optional<bool> held;
    std::string position_fn_name;
    std::string factory_name;
    std::string attach_fn_name;
    std::string detach_fn_name;

    static constexpr auto describe()
    {
        return std::make_tuple(
            Reflect::member("kind", &WiringRecord::kind),
            Reflect::opt_member("interval", &WiringRecord::interval),
            Reflect::opt_member("duration", &WiringRecord::duration),
            Reflect::opt_member("times", &WiringRecord::times),
            Reflect::opt_member("steps", &WiringRecord::steps),
            Reflect::opt_member("interval_token", &WiringRecord::interval_token),
            Reflect::opt_member("duration_token", &WiringRecord::duration_token),
            Reflect::opt_member("key", &WiringRecord::key),
            Reflect::opt_member("button", &WiringRecord::button),
            Reflect::opt_member("held", &WiringRecord::held),
            Reflect::member("position_fn_name", &WiringRecord::position_fn_name),
            Reflect::member("factory_name", &WiringRecord::factory_name),
            Reflect::member("attach_fn_name", &WiringRecord::attach_fn_name),
            Reflect::member("detach_fn_name", &WiringRecord::detach_fn_name));
    }
};

// =============================================================================
// Sink records
// =============================================================================

struct AudioSinkRecord {
    uint32_t channel {};
    std::string fn_name;

    static constexpr auto describe()
    {
        return std::make_tuple(
            Reflect::member("channel", &AudioSinkRecord::channel),
            Reflect::member("fn_name", &AudioSinkRecord::fn_name));
    }
};

/**
 * @brief Plain-data part of the sink buffer's RenderConfig. The target window,
 *        additional textures and scissor are live or derived and are not encoded.
 */
struct RenderSinkRecord {
    std::string fn_name;
    std::string vertex_shader;
    std::string fragment_shader;
    std::string geometry_shader;
    std::string default_texture_binding;
    Portal::Graphics::PrimitiveTopology topology { Portal::Graphics::PrimitiveTopology::POINT_LIST };
    Portal::Graphics::PolygonMode polygon_mode { Portal::Graphics::PolygonMode::FILL };
    Portal::Graphics::CullMode cull_mode { Portal::Graphics::CullMode::NONE };
    bool triangulate { false };
    int64_t draw_priority {};
    std::unordered_map<std::string, std::string> extra_string_params;

    static constexpr auto describe()
    {
        return std::make_tuple(
            Reflect::member("fn_name", &RenderSinkRecord::fn_name),
            Reflect::member("vertex_shader", &RenderSinkRecord::vertex_shader),
            Reflect::member("fragment_shader", &RenderSinkRecord::fragment_shader),
            Reflect::member("geometry_shader", &RenderSinkRecord::geometry_shader),
            Reflect::member("default_texture_binding", &RenderSinkRecord::default_texture_binding),
            Reflect::member("topology", &RenderSinkRecord::topology),
            Reflect::member("polygon_mode", &RenderSinkRecord::polygon_mode),
            Reflect::member("cull_mode", &RenderSinkRecord::cull_mode),
            Reflect::member("triangulate", &RenderSinkRecord::triangulate),
            Reflect::member("draw_priority", &RenderSinkRecord::draw_priority),
            Reflect::member("extra_string_params", &RenderSinkRecord::extra_string_params));
    }
};

// =============================================================================
// Locus navigation fields
// =============================================================================

/**
 * @brief Serializable subset of Kinesis::NavigationConfig sufficient to seed
 *        a Locus on reconstruct. Dynamic nav state (velocity, yaw accumulation)
 *        is transient and is not encoded.
 *
 * @note view_targets are live RenderProcessor pointers and cannot be serialized.
 *       StateDecoder warns on reconstruct; the caller must reconnect them.
 */
struct LocusNavRecord {
    glm::vec3 eye {};
    glm::vec3 target {};
    float fov { 60.0F };
    float near_plane { 0.01F };
    float far_plane { 1000.0F };
    float speed { 1.0F };
    float mouse_sensitivity { 0.002F };
    float scroll_speed { 0.5F };
    float aspect { 1.0F };

    static constexpr auto describe()
    {
        return std::make_tuple(
            Reflect::member("eye", &LocusNavRecord::eye),
            Reflect::member("target", &LocusNavRecord::target),
            Reflect::member("fov", &LocusNavRecord::fov),
            Reflect::member("near", &LocusNavRecord::near_plane),
            Reflect::member("far", &LocusNavRecord::far_plane),
            Reflect::member("speed", &LocusNavRecord::speed),
            Reflect::member("mouse_sensitivity", &LocusNavRecord::mouse_sensitivity),
            Reflect::member("scroll_speed", &LocusNavRecord::scroll_speed),
            Reflect::member("aspect", &LocusNavRecord::aspect));
    }
};

// =============================================================================
// Entity record
// =============================================================================

/**
 * @brief Per-entity JSON record.
 *
 * `kind` is one of "emitter", "sensor", "agent".
 * `subkind` is empty for plain Agent, "locus" for Locus, "presence" for Presence.
 * `locus_nav` is present only when subkind == "locus".
 * `position` is absent for an entity that has none; its EXR column is zero.
 * `orientation` is x, y, z, w and is absent for an entity that has none.
 */
struct EntityRecord {
    uint32_t id {};
    std::string kind;
    std::string subkind;
    std::optional<glm::vec3> position;
    std::optional<glm::vec4> orientation;
    float intensity { 0.0F };
    float radius { 0.0F };
    float query_radius { 0.0F };
    std::optional<glm::vec3> color;
    std::optional<float> size;
    std::string influence_fn_name;
    std::string perception_fn_name;
    std::string radiate_fn_name;
    WiringRecord wiring;
    std::vector<AudioSinkRecord> audio_sinks;
    std::vector<RenderSinkRecord> render_sinks;
    std::optional<LocusNavRecord> locus_nav;
    std::string falloff_curve_name;
    std::optional<float> falloff_radius;

    static constexpr auto describe()
    {
        return std::make_tuple(
            Reflect::member("id", &EntityRecord::id),
            Reflect::member("kind", &EntityRecord::kind),
            Reflect::member("subkind", &EntityRecord::subkind),
            Reflect::opt_member("position", &EntityRecord::position),
            Reflect::opt_member("orientation", &EntityRecord::orientation),
            Reflect::member("intensity", &EntityRecord::intensity),
            Reflect::member("radius", &EntityRecord::radius),
            Reflect::member("query_radius", &EntityRecord::query_radius),
            Reflect::opt_member("color", &EntityRecord::color),
            Reflect::opt_member("size", &EntityRecord::size),
            Reflect::member("influence_fn_name", &EntityRecord::influence_fn_name),
            Reflect::member("perception_fn_name", &EntityRecord::perception_fn_name),
            Reflect::member("radiate_fn_name", &EntityRecord::radiate_fn_name),
            Reflect::member("wiring", &EntityRecord::wiring),
            Reflect::member("audio_sinks", &EntityRecord::audio_sinks),
            Reflect::member("render_sinks", &EntityRecord::render_sinks),
            Reflect::member("falloff_curve", &EntityRecord::falloff_curve_name),
            Reflect::opt_member("locus_nav", &EntityRecord::locus_nav),
            Reflect::opt_member("falloff_radius", &EntityRecord::falloff_radius));
    }
};

// =============================================================================
// Expanse record
// =============================================================================

/**
 * @brief Per-expanse JSON record.
 *
 * The containment predicate and crossing callbacks are closures, so only their
 * names are serializable and they are resolved from the Fabric's function
 * registry on reconstruct. The box is plain data and is stored as is.
 *
 * `subkind` is empty for a plain Expanse, "hull" for a Hull and "mantle" for a
 * Mantle. A Hull keeps its collection index and a Mantle its fit mode, as the
 * lowercase FitMode name. The buffer both wrap is live and is not encoded.
 * `id` is the id the Expanse had when saved and is informational.
 */
struct ExpanseRecord {
    uint32_t id {};
    std::string subkind;
    std::string fn_name;
    std::string on_enter_fn_name;
    std::string on_exit_fn_name;
    std::optional<glm::vec3> bounds_min;
    std::optional<glm::vec3> bounds_max;
    std::optional<uint32_t> collection;
    std::string fit_name;

    static constexpr auto describe()
    {
        return std::make_tuple(
            Reflect::member("id", &ExpanseRecord::id),
            Reflect::member("subkind", &ExpanseRecord::subkind),
            Reflect::member("fn_name", &ExpanseRecord::fn_name),
            Reflect::member("on_enter_fn_name", &ExpanseRecord::on_enter_fn_name),
            Reflect::member("on_exit_fn_name", &ExpanseRecord::on_exit_fn_name),
            Reflect::opt_member("bounds_min", &ExpanseRecord::bounds_min),
            Reflect::opt_member("bounds_max", &ExpanseRecord::bounds_max),
            Reflect::opt_member("collection", &ExpanseRecord::collection),
            Reflect::member("fit", &ExpanseRecord::fit_name));
    }
};

// =============================================================================
// Fabric schema
// =============================================================================

struct FabricSchema {
    uint32_t version { k_schema_version };
    std::string fabric_name;
    std::vector<EntityRecord> entities;
    std::vector<ExpanseRecord> expanses;
    RangeSet ranges;
    std::optional<float> cell_size;

    static constexpr auto describe()
    {
        return std::make_tuple(
            Reflect::member("version", &FabricSchema::version),
            Reflect::member("fabric_name", &FabricSchema::fabric_name),
            Reflect::opt_member("cell_size", &FabricSchema::cell_size),
            Reflect::member("entities", &FabricSchema::entities),
            Reflect::member("expanses", &FabricSchema::expanses),
            Reflect::member("ranges", &FabricSchema::ranges));
    }
};

// =============================================================================
// Tapestry schema
// =============================================================================

/**
 * @brief Entry in the Tapestry envelope pointing to one Fabric's EXR+JSON pair.
 */
struct FabricRef {
    std::string name;
    std::string base_path;

    static constexpr auto describe()
    {
        return std::make_tuple(
            Reflect::member("name", &FabricRef::name),
            Reflect::member("base_path", &FabricRef::base_path));
    }
};

/**
 * @brief Tapestry-level named Expanse record. Lives in tapestry.json rather
 *        than any individual fabric file because Tapestry-owned Expanses may
 *        be registered on multiple Fabrics.
 *
 * `fabric_names` lists the Fabrics that had this Expanse registered at encode
 * time. On reconstruct the decoder resolves Fabrics by name and calls
 * Fabric::add_expanse for each. `subkind`, `collection` and `fit` are as in
 * ExpanseRecord.
 */
struct TapestryExpanseRecord {
    std::string name;
    std::string subkind;
    std::string fn_name;
    std::string on_enter_fn_name;
    std::string on_exit_fn_name;
    std::optional<glm::vec3> bounds_min;
    std::optional<glm::vec3> bounds_max;
    std::optional<uint32_t> collection;
    std::string fit_name;
    std::vector<std::string> fabric_names;

    static constexpr auto describe()
    {
        return std::make_tuple(
            Reflect::member("name", &TapestryExpanseRecord::name),
            Reflect::member("subkind", &TapestryExpanseRecord::subkind),
            Reflect::member("fn_name", &TapestryExpanseRecord::fn_name),
            Reflect::member("on_enter_fn_name", &TapestryExpanseRecord::on_enter_fn_name),
            Reflect::member("on_exit_fn_name", &TapestryExpanseRecord::on_exit_fn_name),
            Reflect::opt_member("bounds_min", &TapestryExpanseRecord::bounds_min),
            Reflect::opt_member("bounds_max", &TapestryExpanseRecord::bounds_max),
            Reflect::opt_member("collection", &TapestryExpanseRecord::collection),
            Reflect::member("fit", &TapestryExpanseRecord::fit_name),
            Reflect::member("fabric_names", &TapestryExpanseRecord::fabric_names));
    }
};

struct TapestrySchema {
    uint32_t version { k_schema_version };
    std::vector<FabricRef> fabrics;
    std::vector<TapestryExpanseRecord> expanses;
    nlohmann::json user_state;

    static constexpr auto describe()
    {
        return std::make_tuple(
            Reflect::member("version", &TapestrySchema::version),
            Reflect::member("fabrics", &TapestrySchema::fabrics),
            Reflect::member("expanses", &TapestrySchema::expanses),
            Reflect::member("user_state", &TapestrySchema::user_state));
    }
};

// =============================================================================
// Kind helpers shared between encoder and decoder
// =============================================================================

/**
 * @brief Map Fabric::Kind to its lowercase JSON string token via magic_enum.
 */
inline std::string kind_to_string(Fabric::Kind k)
{
    return Reflect::enum_to_lowercase_string(k);
}

/**
 * @brief Return true if @p s maps to a known Fabric::Kind token.
 *
 * Call this as a gate before parse_kind. Unrecognised strings should be
 * warned and skipped at the call site; parse_kind assumes the string is valid.
 */
inline bool kind_known(std::string_view s)
{
    return Reflect::string_to_enum_case_insensitive<Fabric::Kind>(s).has_value();
}

/**
 * @brief Parse a JSON kind token to Fabric::Kind (case-insensitive).
 *
 * Precondition: kind_known(s) is true. Behaviour is undefined for
 * unrecognised strings; guard with kind_known before calling.
 */
inline Fabric::Kind parse_kind(std::string_view s)
{
    return *Reflect::string_to_enum_case_insensitive<Fabric::Kind>(s);
}

} // namespace MayaFlux::Nexus::State
