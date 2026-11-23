#pragma once

#include "MeshAssemblyContainer.hpp"

namespace MayaFlux::Kakshya {

/**
 * @enum MeshAssemblyRowKind
 * @brief What a placement row shows.
 */
enum class MeshAssemblyRowKind : uint8_t {
    Joint, ///< Nothing: the row only carries a placement for its children.
    Mesh, ///< A mesh.
    Points, ///< A set of points.
    Curves ///< Curves, as lines.
};

/** @brief What the row shows, Joint for a row with no source or a source out of range. */
[[nodiscard]] MAYAFLUX_API MeshAssemblyRowKind row_kind(const MeshAssemblyContainer& assembly, size_t row);

/**
 * @struct MeshAssemblyPartKey
 * @brief Which part a row belongs to, as a partition decides it.
 *
 * Rows of one kind with one key make one part. A part of meshes marked instanced is drawn once
 * and placed many times, so it is split further by source and material, and a part of anything
 * but meshes ignores the mark.
 */
struct MeshAssemblyPartKey {
    uint64_t key {};
    bool instanced {};

    auto operator<=>(const MeshAssemblyPartKey&) const = default;
};

/**
 * @struct MeshAssemblyPartContext
 * @brief What a partition may read about an assembly, worked out once for all rows.
 */
struct MeshAssemblyPartContext {
    const MeshAssemblyContainer& assembly;
    std::vector<MeshAssemblyGroup> groups; ///< assembly.groups().
    std::vector<std::optional<size_t>> group_of; ///< The group each row is in, if any, indexed by row.
};

/**
 * @brief Decides which part a row belongs to. Any callable will do; the parts of an
 *        assembly are whatever it says they are.
 */
using MeshAssemblyPartition = std::function<MeshAssemblyPartKey(const MeshAssemblyPartContext&, size_t row)>;

/**
 * @struct MeshAssemblyPart
 * @brief Rows of one kind that a partition put together, in row order.
 */
struct MeshAssemblyPart {
    MeshAssemblyRowKind kind {};
    MeshAssemblyPartKey key;
    std::vector<uint64_t> rows;
};

/**
 * @brief Sort the rows that show something into parts.
 * @param partition What decides a row's key. Empty keeps every row of a kind together.
 * @return The parts in the order their first rows appear. A joint is in none.
 */
[[nodiscard]] MAYAFLUX_API std::vector<MeshAssemblyPart> partition_assembly(
    const MeshAssemblyContainer& assembly, const MeshAssemblyPartition& partition = {});

/** @brief Partitions that are ready to use. They are plain callables, and a partition of your own is as good. */
namespace Partitions {

    /** @brief All rows of a kind in one part. */
    [[nodiscard]] inline MeshAssemblyPartition together()
    {
        return [](const MeshAssemblyPartContext&, size_t) { return MeshAssemblyPartKey {}; };
    }

    /** @brief One part for each material, and one for the rows with none. */
    [[nodiscard]] inline MeshAssemblyPartition by_material()
    {
        return [](const MeshAssemblyPartContext& context, size_t row) {
            const auto placement = context.assembly.placement(row);
            return MeshAssemblyPartKey { .key = placement && placement->material ? *placement->material + 1 : 0 };
        };
    }

    /** @brief One part for each source, so every piece of geometry has its own target. */
    [[nodiscard]] inline MeshAssemblyPartition by_source()
    {
        return [](const MeshAssemblyPartContext& context, size_t row) {
            const auto placement = context.assembly.placement(row);
            return MeshAssemblyPartKey { .key = placement && placement->source ? *placement->source + 1 : 0 };
        };
    }

    /**
     * @brief Rows whose geometry has frames apart from rows whose geometry never changes.
     *
     * A target that takes new geometry uploads all of it, so keeping the one thing that animates
     * out of the static level means the level is never uploaded again.
     */
    [[nodiscard]] inline MeshAssemblyPartition by_geometry_change()
    {
        return [](const MeshAssemblyPartContext& context, size_t row) {
            const auto placement = context.assembly.placement(row);
            if (!placement || !placement->source) {
                return MeshAssemblyPartKey {};
            }
            const bool changes = context.assembly.mesh_frames(*placement->source)
                || context.assembly.spatial_frames(*placement->source);
            return MeshAssemblyPartKey { .key = changes ? 1U : 0U };
        };
    }

    /**
     * @brief Meshes shown in at least @p min_rows places go to an instanced part each.
     * @param min_rows Rows of one source and material at which they are instanced.
     */
    [[nodiscard]] inline MeshAssemblyPartition instanced(size_t min_rows = 2)
    {
        return [min_rows](const MeshAssemblyPartContext& context, size_t row) {
            const auto& group = context.group_of[row];
            if (!group || context.groups[*group].rows.size() < min_rows) {
                return MeshAssemblyPartKey {};
            }
            return MeshAssemblyPartKey { .key = *group + 1, .instanced = true };
        };
    }

} // namespace Partitions

} // namespace MayaFlux::Kakshya
