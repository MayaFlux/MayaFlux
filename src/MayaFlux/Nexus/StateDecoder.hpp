#pragma once

#include "Fabric.hpp"

namespace MayaFlux::Nexus {

/**
 * @class StateDecoder
 * @brief Patches Fabric state from a previously encoded EXR + JSON schema pair.
 *
 * v0 scope: reads {base_path}.exr and {base_path}.json, denormalizes
 * per-channel ranges from the schema, and patches existing Emitters in the
 * target Fabric by id. Ids present in the schema but absent from the Fabric
 * are logged and skipped. Non-Emitter entities are not touched.
 *
 * Patch semantics: position and intensity are overwritten from the decoded
 * data. The Emitter's influence function, sinks, color, size, and radius
 * are preserved.
 */
class MAYAFLUX_API StateDecoder {
public:
    StateDecoder() = default;
    ~StateDecoder() = default;

    StateDecoder(const StateDecoder&) = delete;
    StateDecoder& operator=(const StateDecoder&) = delete;
    StateDecoder(StateDecoder&&) = default;
    StateDecoder& operator=(StateDecoder&&) = default;

    /**
     * @brief Decode and apply to @p fabric.
     * @param fabric    Target fabric. Must already contain Emitters with
     *                  matching ids.
     * @param base_path Path stem without extension, same value passed to
     *                  StateEncoder::encode.
     * @return True on success. Partial patches (some ids missing) still
     *         return true; failures are logged.
     */
    [[nodiscard]] bool decode(Fabric& fabric, const std::string& base_path);

    /**
     * @brief Last error message, empty if no error.
     */
    [[nodiscard]] const std::string& last_error() const { return m_last_error; }

    /**
     * @brief Number of Emitters actually patched on the last decode call.
     */
    [[nodiscard]] size_t patched_count() const { return m_patched_count; }

    /**
     * @brief Number of ids present in the schema but missing from the fabric.
     */
    [[nodiscard]] size_t missing_count() const { return m_missing_count; }

private:
    std::string m_last_error;
    size_t m_patched_count { 0 };
    size_t m_missing_count { 0 };
};

} // namespace MayaFlux::Nexus
