#pragma once

#include "MayaFlux/Nexus/Tapestry.hpp"

#include "Schema.hpp"

namespace MayaFlux::Nexus {

/**
 * @class StateEncoder
 * @brief Serializes Fabric state to an EXR texture and JSON schema.
 *
 * Writes what the classes hold, in the order they are registered by id:
 * Emitters, Sensors, Agents (including Locus and Presence), their wiring and
 * sinks, and the Expanses of each Fabric (including Hull and Mantle). Output:
 *   {base}.exr   RGBA32F, one column per entity, rows described in Schema.hpp.
 *   {base}.json  Per-entity records and per-channel ranges for denormalization.
 *
 * An entity without a position is written with its position absent. Optional
 * fields (position, orientation, color, size) are omitted from the schema when
 * unset. A callable that exists but has no name emits a warning. Live objects
 * (buffers, windows, typed influences and perceptions, attachments, callables
 * themselves) are never encoded, only their names and plain configuration.
 */
class MAYAFLUX_API StateEncoder {
public:
    StateEncoder() = default;
    ~StateEncoder() = default;

    StateEncoder(const StateEncoder&) = delete;
    StateEncoder& operator=(const StateEncoder&) = delete;
    StateEncoder(StateEncoder&&) = default;
    StateEncoder& operator=(StateEncoder&&) = default;

    /**
     * @brief Encode the given Fabric to {base_path}.exr and {base_path}.json.
     * @param fabric    Source of entity and Expanse state.
     * @param base_path Path stem without extension.
     * @return True on success. On failure call last_error().
     */
    [[nodiscard]] bool encode(const Fabric& fabric, const std::string& base_path);

    /**
     * @brief Encode all Fabrics in @p tapestry to @p base_dir.
     *
     * Writes one EXR+JSON pair per Fabric under base_dir/{fabric_name_or_id},
     * then writes base_dir/tapestry.json as the envelope.
     *
     * @param tapestry  Source Tapestry.
     * @param base_dir  Directory path. Must exist.
     * @return True on success.
     */
    [[nodiscard]] bool encode(const Tapestry& tapestry, const std::string& base_dir, nlohmann::json user_state = {});

    /**
     * @brief Last error message, empty if no error.
     */
    [[nodiscard]] const std::string& last_error() const { return m_last_error; }

private:
    [[nodiscard]] bool encode_fabric(
        const Fabric& fabric,
        const std::string& base_path,
        const std::unordered_set<const Expanse*>& tapestry_owned);

    [[nodiscard]] static State::WiringRecord build_wiring(const Fabric& fabric, uint32_t id);

    template <typename Record>
    static void fill_expanse(Record& record, const Expanse& expanse);

    std::string m_last_error;
};

} // namespace MayaFlux::Nexus
