#pragma once

#include "MayaFlux/Nexus/Tapestry.hpp"

#include "Bindings.hpp"
#include "Schema.hpp"

namespace MayaFlux::IO {
class StagedFileSet;
}

namespace MayaFlux::Nexus {

/**
 * @class StateEncoder
 * @brief Serializes Fabric state to an EXR texture and JSON schema.
 *
 * Writes what the classes hold, in the order they are registered by id:
 * Emitters, Sensors, Agents (including Locus and Presence), their wiring, sinks,
 * attachment and view targets, and the Expanses of each Fabric (including Hull
 * and Mantle). Output:
 *   {base}.exr   RGBA32F, one column per entity, a value per numeric component of
 *                the controls the entities hold, laid out as FabricSchema::components says.
 *   {base}.json  Per-entity records, which hold each control by name, and the
 *                component ranges the EXR was normalized by.
 *
 * An entity's state is its controls: every slot of its store that holds a value
 * is written, and an empty one is left out. A function is written by name with
 * the arguments of its call. Buffers, windows and network sources are written by
 * the names the Bindings give them, and every bound buffer is recorded as its
 * declared file or written as a snapshot into a new assets directory beside the
 * state. Bound courses have their playback written. What cannot be written, such as an anonymous
 * function or an unbound window, is warned and listed in the record.
 */
class MAYAFLUX_API StateEncoder {
public:
    StateEncoder() = default;

    /** @param bindings Names for the live objects the state refers to. */
    explicit StateEncoder(const std::shared_ptr<const State::Bindings>& bindings)
        : m_bindings(bindings)
    {
    }

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
     * then base_dir/tapestry.json as the envelope. Every file is staged first and
     * promoted together with tapestry.json last, so a failed encode leaves an
     * earlier snapshot in base_dir untouched. A fabric name must be a plain file
     * name. Files of fabrics dropped since the last snapshot are not removed.
     *
     * @param tapestry  Source Tapestry.
     * @param base_dir  Directory path, resolved as IO::resolve_write_path does.
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
        const std::unordered_set<const Expanse*>& tapestry_owned,
        IO::StagedFileSet* staged = nullptr);

    /** Record the playback of every bound course. */
    [[nodiscard]] std::vector<State::CourseRecord> save_courses() const;

    /** Record every bound buffer, writing snapshots into a new directory under @p directory. */
    [[nodiscard]] std::vector<State::AssetRecord> save_assets(const std::string& directory) const;

    [[nodiscard]] State::WiringRecord build_wiring(const Fabric& fabric, uint32_t id) const;

    template <typename Record>
    void fill_expanse(Record& record, const Expanse& expanse) const;

    [[nodiscard]] std::string buffer_name(const std::shared_ptr<Buffers::VKBuffer>& buffer) const;
    [[nodiscard]] std::string window_name(const std::shared_ptr<Core::Window>& window) const;
    [[nodiscard]] std::string source_name(const std::shared_ptr<Vruta::NetworkSource>& source) const;

    template <typename Entity>
    void fill_attachment(State::EntityRecord& record, const Entity& entity) const;

    std::shared_ptr<const State::Bindings> m_bindings;

    std::string m_last_error;
};

} // namespace MayaFlux::Nexus
