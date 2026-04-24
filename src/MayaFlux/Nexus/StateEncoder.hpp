#pragma once

#include "Fabric.hpp"

namespace MayaFlux::Nexus {

/**
 * @class StateEncoder
 * @brief Serializes Fabric state to an EXR texture and JSON schema.
 *
 * v0 scope: encodes Emitter position (xyz) and intensity as RGBA32F pixels,
 * one pixel per Emitter. Ranges are computed from the data and written into
 * the schema for self-describing decode.
 *
 * Non-Emitter entities and entities with no position are skipped in v0.
 * Output consists of two files sharing a base path:
 *   {base}.exr   RGBA32F texture, 1 row, N columns where N = emitter count
 *   {base}.json  Schema with fabric identity, id list, channel map, ranges
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
