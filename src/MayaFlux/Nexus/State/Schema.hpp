#pragma once

#include "MayaFlux/Nexus/Fabric.hpp"
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
 */
inline constexpr uint32_t k_schema_version = 5;

/**
 * @brief RGBA32F EXR layout constants shared between encoder and decoder.
 *
 * Row 0: position.xyz, intensity
 * Row 1: color.rgb, size
 * Row 2: radius, query_radius, entity_type_norm, 0
 * Row 3: trigger_kind, time_kind, sink_type_bits, first_audio_channel
 * Row 4: locus_nav.fov, locus_nav.near, locus_nav.far, locus_nav.speed
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
            IO::member("min", &Range::min),
            IO::member("max", &Range::max));
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
            IO::member("position.x", &RangeSet::pos_x),
            IO::member("position.y", &RangeSet::pos_y),
            IO::member("position.z", &RangeSet::pos_z),
            IO::member("intensity", &RangeSet::intensity),
            IO::member("color.r", &RangeSet::color_r),
            IO::member("color.g", &RangeSet::color_g),
            IO::member("color.b", &RangeSet::color_b),
            IO::member("size", &RangeSet::size),
            IO::member("radius", &RangeSet::radius),
            IO::member("query_radius", &RangeSet::query_radius));
    }
};

// =============================================================================
// Wiring records
// =============================================================================

/**
 * @brief Serializable wiring strategies.
 *
 * Enumerator names are lowercased by Reflect:: for JSON: commit_driven,
 * every, move_to, unsupported. The decoder treats Unsupported as a
 * commit_driven fallback with a warning.
 */
enum class WiringKind : uint8_t {
    CommitDriven,
    Every,
    MoveTo,
    Unsupported,
};

struct WiringStep {
    glm::vec3 position {};
    double delay_seconds { 0.0 };

    static constexpr auto describe()
    {
        return std::make_tuple(
            IO::member("position", &WiringStep::position),
            IO::member("delay", &WiringStep::delay_seconds));
    }
};

struct WiringRecord {
    WiringKind kind { WiringKind::CommitDriven };
    std::optional<double> interval;
    std::optional<double> duration;
    std::optional<size_t> times;
    std::optional<std::vector<WiringStep>> steps;

    static constexpr auto describe()
    {
        return std::make_tuple(
            IO::member("kind", &WiringRecord::kind),
            IO::opt_member("interval", &WiringRecord::interval),
            IO::opt_member("duration", &WiringRecord::duration),
            IO::opt_member("times", &WiringRecord::times),
            IO::opt_member("steps", &WiringRecord::steps));
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
            IO::member("channel", &AudioSinkRecord::channel),
            IO::member("fn_name", &AudioSinkRecord::fn_name));
    }
};

struct RenderSinkRecord {
    std::string fn_name;

    static constexpr auto describe()
    {
        return std::make_tuple(
            IO::member("fn_name", &RenderSinkRecord::fn_name));
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
    glm::vec3 up { 0.0F, 1.0F, 0.0F };
    float fov { 60.0F };
    float near_plane { 0.01F };
    float far_plane { 1000.0F };
    float speed { 1.0F };

    static constexpr auto describe()
    {
        return std::make_tuple(
            IO::member("eye", &LocusNavRecord::eye),
            IO::member("target", &LocusNavRecord::target),
            IO::member("up", &LocusNavRecord::up),
            IO::member("fov", &LocusNavRecord::fov),
            IO::member("near", &LocusNavRecord::near_plane),
            IO::member("far", &LocusNavRecord::far_plane),
            IO::member("speed", &LocusNavRecord::speed));
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
 */
struct EntityRecord {
    uint32_t id {};
    std::string kind;
    std::string subkind;
    glm::vec3 position {};
    float intensity { 0.0F };
    float radius { 0.0F };
    float query_radius { 0.0F };
    std::optional<glm::vec3> color;
    std::optional<float> size;
    std::string influence_fn_name;
    std::string perception_fn_name;
    WiringRecord wiring;
    std::vector<AudioSinkRecord> audio_sinks;
    std::vector<RenderSinkRecord> render_sinks;
    std::optional<LocusNavRecord> locus_nav;

    static constexpr auto describe()
    {
        return std::make_tuple(
            IO::member("id", &EntityRecord::id),
            IO::member("kind", &EntityRecord::kind),
            IO::member("subkind", &EntityRecord::subkind),
            IO::member("position", &EntityRecord::position),
            IO::member("intensity", &EntityRecord::intensity),
            IO::member("radius", &EntityRecord::radius),
            IO::member("query_radius", &EntityRecord::query_radius),
            IO::opt_member("color", &EntityRecord::color),
            IO::opt_member("size", &EntityRecord::size),
            IO::member("influence_fn_name", &EntityRecord::influence_fn_name),
            IO::member("perception_fn_name", &EntityRecord::perception_fn_name),
            IO::member("wiring", &EntityRecord::wiring),
            IO::member("audio_sinks", &EntityRecord::audio_sinks),
            IO::member("render_sinks", &EntityRecord::render_sinks),
            IO::opt_member("locus_nav", &EntityRecord::locus_nav));
    }
};

// =============================================================================
// Expanse record
// =============================================================================

/**
 * @brief Per-expanse JSON record.
 *
 * Only the function names are serializable; the containment predicate and
 * crossing callbacks are closures that must be resolved from the Fabric's
 * function registry on reconstruct. If fn_name is empty the expanse cannot
 * be reconstructed and the decoder skips it with a warning.
 */
struct ExpanseRecord {
    uint32_t id {};
    std::string fn_name;
    std::string on_enter_fn_name;
    std::string on_exit_fn_name;

    static constexpr auto describe()
    {
        return std::make_tuple(
            IO::member("id", &ExpanseRecord::id),
            IO::member("fn_name", &ExpanseRecord::fn_name),
            IO::member("on_enter_fn_name", &ExpanseRecord::on_enter_fn_name),
            IO::member("on_exit_fn_name", &ExpanseRecord::on_exit_fn_name));
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

    static constexpr auto describe()
    {
        return std::make_tuple(
            IO::member("version", &FabricSchema::version),
            IO::member("fabric_name", &FabricSchema::fabric_name),
            IO::member("entities", &FabricSchema::entities),
            IO::member("expanses", &FabricSchema::expanses),
            IO::member("ranges", &FabricSchema::ranges));
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
            IO::member("name", &FabricRef::name),
            IO::member("base_path", &FabricRef::base_path));
    }
};

/**
 * @brief Tapestry-level named Expanse record. Lives in tapestry.json rather
 *        than any individual fabric file because Tapestry-owned Expanses may
 *        be registered on multiple Fabrics.
 *
 * `fabric_ids` lists the runtime Fabric ids that had this Expanse registered
 * at encode time. On reconstruct the decoder resolves Fabrics by name and
 * calls Fabric::add_expanse for each.
 */
struct TapestryExpanseRecord {
    std::string name;
    std::string fn_name;
    std::string on_enter_fn_name;
    std::string on_exit_fn_name;
    std::vector<std::string> fabric_names;

    static constexpr auto describe()
    {
        return std::make_tuple(
            IO::member("name", &TapestryExpanseRecord::name),
            IO::member("fn_name", &TapestryExpanseRecord::fn_name),
            IO::member("on_enter_fn_name", &TapestryExpanseRecord::on_enter_fn_name),
            IO::member("on_exit_fn_name", &TapestryExpanseRecord::on_exit_fn_name),
            IO::member("fabric_names", &TapestryExpanseRecord::fabric_names));
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
            IO::member("version", &TapestrySchema::version),
            IO::member("fabrics", &TapestrySchema::fabrics),
            IO::member("expanses", &TapestrySchema::expanses),
            IO::member("user_state", &TapestrySchema::user_state));
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
