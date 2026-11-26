#pragma once

#include "MeshAssemblyParts.hpp"

namespace MayaFlux::Kakshya {

/**
 * @struct MeshAssemblyRow
 * @brief A placement row with its index and what it shows, as one value.
 */
struct MeshAssemblyRow {
    uint64_t index {};
    MeshAssemblyPlacement placement;
    MeshAssemblyRowKind kind {};
};

/**
 * @class MeshAssemblyAccess
 * @brief Non-owning read view over a MeshAssemblyContainer: its hierarchy, its geometry and
 *        what it holds, asked for by row, by source and by name.
 *
 * Parallel to MeshAccess and CompositeAccess: it describes what the data is and moves nothing.
 * The container's own calls answer for one row or one source at a time; this view indexes the
 * parent links once, when it is made, so the questions a consumer asks of a whole level (the
 * children of a row, its subtree, its path, the rows of a source, the row called a name) are
 * answered without walking the table again. It is a snapshot: make a new one after rows are
 * added. The container must outlive it, and must not be changed while it is read.
 */
class MAYAFLUX_API MeshAssemblyAccess {
public:
    explicit MeshAssemblyAccess(const MeshAssemblyContainer& assembly);

    [[nodiscard]] const MeshAssemblyContainer& assembly() const noexcept { return *m_assembly; }

    [[nodiscard]] size_t row_count() const noexcept { return m_children.size(); }
    [[nodiscard]] size_t source_count() const noexcept { return m_assembly->get_data().sources.size(); }
    [[nodiscard]] size_t material_count() const noexcept { return m_assembly->get_data().materials.size(); }
    [[nodiscard]] size_t motion_count() const noexcept { return m_assembly->get_data().motions.size(); }

    // Rows

    /** @brief The row, or nullopt when it is out of range. */
    [[nodiscard]] std::optional<MeshAssemblyRow> row(size_t row) const;

    /** @brief Rows with no parent, in row order. */
    [[nodiscard]] const std::vector<uint64_t>& roots() const noexcept { return m_roots; }

    /** @brief The rows whose parent is @p row, in row order. Empty for a row out of range. */
    [[nodiscard]] std::span<const uint64_t> children(size_t row) const noexcept;

    /** @brief The row and everything below it, parents before children. Empty for a row out of range. */
    [[nodiscard]] std::vector<uint64_t> subtree(size_t row) const;

    /** @brief The parents of the row, nearest first, ending at its root. Empty for a root. */
    [[nodiscard]] std::vector<uint64_t> ancestors(size_t row) const;

    /** @brief How many parents the row has, 0 for a root. */
    [[nodiscard]] size_t depth(size_t row) const;

    /** @brief The names from the root to the row joined with '/', empty for a row out of range. */
    [[nodiscard]] std::string path(size_t row) const;

    /** @brief The first row with this name, in row order. */
    [[nodiscard]] std::optional<uint64_t> find(std::string_view name) const;

    /** @brief The row with this path, as path() writes it. */
    [[nodiscard]] std::optional<uint64_t> find_path(std::string_view path) const;

    /** @brief Every row with this name, in row order. */
    [[nodiscard]] std::vector<uint64_t> find_all(std::string_view name) const;

    /** @brief Every row that shows this source, in row order. */
    [[nodiscard]] std::vector<uint64_t> rows_of_source(size_t source) const;

    /** @brief Every row that shows this kind of thing, in row order. */
    [[nodiscard]] std::vector<uint64_t> rows_of_kind(MeshAssemblyRowKind kind) const;

    /** @brief The row's placement composed through its parents, or nullopt for a row out of range. */
    [[nodiscard]] std::optional<glm::mat4> world(size_t row) const;

    // Geometry and what is held

    [[nodiscard]] const MeshAssemblySource* source(size_t source) const noexcept;
    [[nodiscard]] const MeshAssemblyMaterial* material(size_t material) const noexcept;
    [[nodiscard]] const MeshAssemblyMotion* motion(size_t motion) const noexcept;

    /** @brief A view over the mesh of a source, or nullopt when the source is not a mesh or has no valid triangles. */
    [[nodiscard]] std::optional<MeshAccess> mesh(size_t source) const;

    /** @brief The points or curves of a source, or nullptr. */
    [[nodiscard]] const SpatialData* spatial(size_t source) const noexcept { return m_assembly->spatial(source); }

    /** @brief The lens of a camera source, or nullptr. */
    [[nodiscard]] const Kakshya::MeshAssemblyLens* lens(size_t source) const noexcept { return m_assembly->lens(source); }

    /** @brief The source a row shows, or nullopt for a joint. */
    [[nodiscard]] std::optional<uint64_t> source_of(size_t row) const;

    /** @brief The channel of a motion that moves a row, or nullptr. */
    [[nodiscard]] const MeshAssemblyChannel* channel(size_t motion, size_t row) const noexcept;

    /** @brief Whether the source has frames of its own, to be played by whoever plays them. */
    [[nodiscard]] bool animates(size_t source) const;

private:
    const MeshAssemblyContainer* m_assembly;
    std::vector<std::optional<uint64_t>> m_parent;
    std::vector<std::vector<uint64_t>> m_children;
    std::vector<uint64_t> m_roots;
    std::vector<std::string> m_names;
};

} // namespace MayaFlux::Kakshya
