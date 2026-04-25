#pragma once

#include "Fabric.hpp"

namespace MayaFlux::Nexus {

/**
 * @class StateEncoder
 * @brief Serializes Fabric state to an EXR texture and JSON schema.
 *
 * v1 scope: encodes all three Kinds (Emitter, Sensor, Agent) that have a
 * position set. Output:
 *   {base}.exr   RGBA32F, width=N entities, height=3 rows:
 *                  row 0: position.xyz, intensity
 *                  row 1: color.rgb, size
 *                  row 2: radius, query_radius, 0, 0
 *   {base}.json  Schema v2 with per-entity records (id, kind, fields) and
 *                per-channel ranges for denormalization.
 *
 * Entities without a position are skipped. Unnamed callables emit a warning
 * but are still encoded. Optional fields (color, size) are written as null
 * in the schema; the EXR channels are zeroed but the decoder ignores them.
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
     * @param fabric    Source of entity state. Only Emitters with a position
     *                  are encoded in v0.
     * @param base_path Path stem without extension.
     * @return True on success. On failure call last_error().
     */
    [[nodiscard]] bool encode(const Fabric& fabric, const std::string& base_path);

    /**
     * @brief Last error message, empty if no error.
     */
    [[nodiscard]] const std::string& last_error() const { return m_last_error; }

private:
    std::string m_last_error;
};

} // namespace MayaFlux::Nexus
