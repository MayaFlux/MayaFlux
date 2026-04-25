#pragma once

#include "Fabric.hpp"

namespace MayaFlux::Nexus {

/**
 * @class StateDecoder
 * @brief Patches Fabric state from a previously encoded EXR + JSON schema pair.
 *
 * v1 scope: reads {base_path}.exr (schema v2) and patches Emitters, Sensors,
 * and Agents by id. Dispatches per Kind; entities present in the schema but
 * absent from the Fabric are logged and counted in missing_count(). Optional
 * fields (color, size) are only patched when the schema records a non-null
 * value. Callable name mismatches between schema and live entity are warned
 * but do not abort the patch.
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
     * @brief Result of a reconstruct() call.
     */
    struct ReconstructionResult {
        int constructed { 0 };
        int patched { 0 };
        int skipped { 0 };
        std::vector<std::string> warnings;
    };

    /**
     * @brief Decode and apply to @p fabric.
     * @param fabric    Target fabric. Must already contain entities with
     *                  ids matching the schema.
     * @param base_path Path stem without extension, same value passed to
     *                  StateEncoder::encode.
     * @return True on success. Partial patches (some ids missing) still
     *         return true; failures are logged.
     */
    [[nodiscard]] bool decode(Fabric& fabric, const std::string& base_path);

    /**
     * @brief Patch existing entities and construct missing ones from schema.
     *
     * Entities whose id exists in the fabric are patched in place. Missing
     * entities are constructed, their callables resolved via the fabric's
     * function registry (no-op fallback with warning if absent), and wired.
     * Supported wiring kinds for v2: every, move_to, commit_driven.
     * Unsupported kinds (on, use, bind) fall back to commit_driven + warning.
     * Hard failure (bad schema, unreadable EXR, dimension mismatch) sets
     * last_error() and returns a zeroed result.
     *
     * @param fabric    Target fabric. May be empty, partial, or full.
     * @param base_path Path stem without extension.
     * @return Counts and per-entity warnings.
     */
    [[nodiscard]] ReconstructionResult reconstruct(Fabric& fabric, const std::string& base_path);

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
