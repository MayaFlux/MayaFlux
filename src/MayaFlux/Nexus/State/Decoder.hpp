#pragma once

#include "MayaFlux/Nexus/Tapestry.hpp"
#include "Bindings.hpp"
#include "Schema.hpp"

namespace MayaFlux::Nexus {

/**
 * @class StateDecoder
 * @brief Patches or rebuilds Fabric state from a previously encoded JSON schema.
 *
 * Does what the classes allow and reports the rest. Emitters, Sensors, Agents
 * (including Locus and Presence) are patched by id or constructed; their
 * Expanses are restored. Each control of the entity takes the value the schema
 * saved for it, and one the schema does not list is emptied, so an absent
 * position clears the entity's position. A control the schema lists and the
 * entity does not hold is warned and skipped. Callable name mismatches between
 * schema and live entity are warned but do not abort the patch. Values are read
 * from the JSON; the EXR is not read.
 *
 * Live objects are never invented. A function is made from the Bindings'
 * FunctionTable with the arguments saved for it, or taken from the Fabric's
 * registry by name when it has none. Buffers, windows and network sources are
 * looked up by the names saved for them, which restores triggers, sinks,
 * attachments, view targets, Hull and Mantle. What is not bound is reported, and
 * a wiring with nothing left to schedule it falls back to commit_driven. Only
 * files of schema version k_schema_version are read.
 */
class MAYAFLUX_API StateDecoder {
public:
    StateDecoder() = default;

    /** @param bindings Names for the live objects the state refers to. */
    explicit StateDecoder(const std::shared_ptr<const State::Bindings>& bindings)
        : m_bindings(bindings)
    {
    }

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
        State::BufferNames buffers; ///< Loaded from the state's assets, for names the Bindings do not hold.
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
     * Entities whose id exists in the fabric have their controls patched in
     * place. Missing entities are constructed with their functions, sinks,
     * attachment and view targets, and wired as they were.
     * Entity ids are assigned by the Fabric, so a reconstructed entity may get a
     * different id, which is warned.
     * Hard failure (a missing or bad schema) sets
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
    /** reconstruct() with buffers already loaded for names the Bindings do not hold. */
    [[nodiscard]] ReconstructionResult reconstruct_fabric(
        Fabric& fabric, const std::string& base_path, const State::BufferNames& loaded);

    /** Whether a live Expanse is of a record's kind and, for a Hull or Mantle, wraps its buffer. */
    [[nodiscard]] bool wraps(const Expanse& live, const std::string& subkind,
        const std::shared_ptr<Buffers::VKBuffer>& buffer) const;

    std::string m_last_error;
    size_t m_patched_count { 0 };
    size_t m_missing_count { 0 };
    std::shared_ptr<const State::Bindings> m_bindings;
};

} // namespace MayaFlux::Nexus
