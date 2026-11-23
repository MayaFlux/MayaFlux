#pragma once

#include "MeshAssemblyContainer.hpp"

namespace MayaFlux::Kakshya {

/**
 * @struct MeshAssemblyGraft
 * @brief Where everything of a grafted assembly landed, by its index in that assembly.
 *
 * rows[i] is the row of the grafted assembly's row i in the target, and likewise for the
 * others. joint is the row that holds the graft together, when it was asked for.
 */
struct MeshAssemblyGraft {
    std::vector<uint64_t> rows;
    std::vector<uint64_t> sources;
    std::vector<uint64_t> materials;
    std::vector<uint64_t> motions;
    std::optional<uint64_t> joint;
};

/**
 * @class MeshAssemblyInsertion
 * @brief Write counterpart to MeshAssemblyAccess: builds an assembly in the container it
 *        holds, a source, a place or a whole other assembly at a time.
 *
 * Parallel to MeshInsertion and CompositeInsertion. The container's own add calls take one
 * thing and say nothing of a level; this takes the shapes a level is made in: a mesh and the
 * places it is shown in, a named group of rows, and another assembly (a prop, a character, a
 * file) grafted under a row, with its sources, materials, motions and frame sources renumbered
 * to fit. It writes through the container's add calls, so every parent still precedes its child
 * and every index stays in range, and a call that is refused changes nothing.
 *
 * The container must outlive this object and must not be read through a MeshAssemblyAccess made
 * before the calls.
 *
 * @code
 * MeshAssemblyContainer level;
 * MeshAssemblyInsertion put(level);
 * const auto wheel = put.add_mesh(std::move(wheel_mesh), "wheel");
 * const auto car = put.add_node("car").value();
 * put.instance(wheel, std::array { front_left, front_right, rear_left, rear_right }, car, "wheel");
 * put.graft(*driver, car, seat_placement, "driver");
 * @endcode
 */
class MAYAFLUX_API MeshAssemblyInsertion {
public:
    explicit MeshAssemblyInsertion(MeshAssemblyContainer& target);

    [[nodiscard]] MeshAssemblyContainer& target() noexcept { return *m_target; }

    /**
     * @brief Add a mesh as a source.
     * @param time When the file said its shots are, if it did.
     * @return The source's index.
     */
    size_t add_mesh(MeshData geometry, std::string name = {},
        std::optional<Kinesis::TimeSampling> time = std::nullopt,
        TopologyVariance variance = TopologyVariance::Constant);

    /** @brief Add points or curves as a source. */
    size_t add_points(SpatialData geometry, std::string name = {},
        std::optional<Kinesis::TimeSampling> time = std::nullopt,
        TopologyVariance variance = TopologyVariance::Constant);

    /**
     * @brief Add a joint: a row that shows nothing and places its children.
     * @return The row, or nullopt when the parent is not an earlier row.
     */
    [[nodiscard]] std::optional<uint64_t> add_node(std::string name,
        std::optional<uint64_t> parent = std::nullopt, const glm::mat4& local = glm::mat4(1.0F));

    /**
     * @brief Add a row that shows a source.
     * @return The row, or nullopt when the parent, source or material is out of range.
     */
    [[nodiscard]] std::optional<uint64_t> add_instance(std::string name, uint64_t source,
        std::optional<uint64_t> parent = std::nullopt, const glm::mat4& local = glm::mat4(1.0F),
        std::optional<uint64_t> material = std::nullopt);

    /**
     * @brief Show one source in many places.
     * @param placements The local placement of each row.
     * @param name_stem Each row is named from it and its number.
     * @return The rows, in the order of the placements, or nothing when the parent, source or
     * material is out of range, in which case no row is added.
     */
    [[nodiscard]] std::vector<uint64_t> instance(uint64_t source, std::span<const glm::mat4> placements,
        std::optional<uint64_t> parent = std::nullopt, std::string_view name_stem = {},
        std::optional<uint64_t> material = std::nullopt);

    /**
     * @brief Put another assembly into this one.
     * @param other The assembly to copy from, which is not changed.
     * @param parent The row its roots go under, or none to leave them roots.
     * @param local Placement of the graft relative to the parent.
     * @param name When not empty, a joint of this name holds the graft, placed at @p local, and the
     *        roots of @p other go under it; when empty, @p local is composed into each root.
     * @return Where everything landed, or nullopt when the parent is out of range.
     *
     * Sources and materials are copied whole, with the frame sources of the sources that have
     * them, and the motions follow their rows.
     */
    [[nodiscard]] std::optional<MeshAssemblyGraft> graft(const MeshAssemblyContainer& other,
        std::optional<uint64_t> parent = std::nullopt, const glm::mat4& local = glm::mat4(1.0F),
        std::string_view name = {});

private:
    MeshAssemblyContainer* m_target;
};

} // namespace MayaFlux::Kakshya
