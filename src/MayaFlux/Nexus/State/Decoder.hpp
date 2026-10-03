#pragma once

#include "MayaFlux/Nexus/Tapestry.hpp"
#include "Schema.hpp"

namespace MayaFlux::Nexus {

/**
 * @class StateDecoder
 * @brief Patches or rebuilds Fabric state from a previously encoded EXR + JSON schema pair.
 *
 * Does what the classes allow and reports the rest. Emitters, Sensors, Agents
 * (including Locus and Presence) are patched by id or constructed; their
 * Expanses are restored. Optional fields (position, orientation, color, size)
 * are applied only when the schema records them, except that an absent position
 * clears the entity's position. Callable name mismatches between schema and
 * live entity are warned but do not abort the patch.
 *
 * Live objects are never invented. Sinks, typed influences and perceptions,
 * attachments, a Locus's view targets and every wiring source that needs a
 * window, a network source or a callable are reported as warnings, and a wiring
 * that cannot be restored falls back to commit_driven. A Hull needs a buffer
 * and is skipped; a Mantle comes back as its plain box. Files from schema
 * versions k_schema_min_version up to k_schema_version are read, with fields
 * added since taking their defaults.
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
        nlohmann::json user_state;
    };

    /**
     * @brief Decode and apply to @p fabric.
     * @param fabric    Target fabric. Must already contain entities with
     *                  ids matching the schema. Expanses are restored too; one
     *                  that matches an Expanse already on the Fabric is left
     *                  in place and only has its box updated.
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
     * function registry (left empty with a warning if absent), and wired.
     * Restorable wiring kinds: every (with its scheduler tokens), move_to and
     * commit_driven. Any other kind falls back to commit_driven with a warning.
     * Entity ids are assigned by the Fabric, so a reconstructed entity may get a
     * different id, which is warned.
     * Hard failure (bad schema, unreadable EXR, dimension mismatch) sets
     * last_error() and returns a zeroed result.
     *
     * @param fabric    Target fabric. May be empty, partial, or full.
     * @param base_path Path stem without extension.
     * @return Counts and per-entity warnings.
     */
    [[nodiscard]] ReconstructionResult reconstruct(Fabric& fabric, const std::string& base_path);

    /**
     * @brief Reconstruct all Fabrics in @p tapestry from @p base_dir.
     *
     * Reads tapestry.json, creates or finds Fabrics by name (a created Fabric gets
     * the cell size its file recorded), calls reconstruct(fabric, base_path) for
     * each, then restores Tapestry-level named Expanses and registers them on the
     * listed Fabrics. A name the Tapestry already holds is reused, not recreated.
     *
     * @param tapestry  Target Tapestry. May be empty or partially populated.
     * @param base_dir  Directory path matching the one passed to encode().
     * @return ReconstructionResult aggregated across all Fabrics.
     */
    [[nodiscard]] ReconstructionResult reconstruct(Tapestry& tapestry, const std::string& base_dir);

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
