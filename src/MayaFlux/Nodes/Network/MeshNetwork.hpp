#pragma once

#include "MeshSlot.hpp"
#include "NodeNetwork.hpp"

namespace MayaFlux::Kakshya {
class MeshAssemblyContainer;
}

namespace MayaFlux::Nodes::Network {

/**
 * @class MeshNetwork
 * @brief NodeNetwork subclass whose nodes are named, hierarchically
 *        transformable mesh slots.
 *
 * Each slot holds a MeshWriterNode (geometry) and a local-space glm::mat4.
 * Slots form an optional DAG via parent indices; MeshNetwork resolves
 * processing order so parents are always updated before children.
 * World transforms propagate as: world = parent_world * local.
 *
 * The primary operator (set via create_operator<T> / set_operator) runs
 * first each cycle. The operator chain (get_operator_chain()) runs after.
 * Both receive the slot list via the operator interface.
 *
 * MeshNetwork initializes m_operator_chain on construction because it
 * requires multi-operator sequencing as a necessity, not an option.
 *
 * No topology enum from NodeNetwork is used: the DAG is explicit via
 * parent indices in each MeshSlot.
 *
 * Usage:
 * @code
 * auto net = std::make_shared<MeshNetwork>();
 *
 * auto torso_node = std::make_shared<GpuSync::MeshWriterNode>();
 * torso_node->set_mesh(torso_verts, torso_indices);
 * uint32_t torso = net->add_slot("torso", torso_node);
 *
 * auto arm_node = std::make_shared<GpuSync::MeshWriterNode>();
 * arm_node->set_mesh(arm_verts, arm_indices);
 * uint32_t arm_l = net->add_slot("arm_l", arm_node, torso);
 *
 * net->create_operator<MeshTransformOperator>();
 * net->get_operator_chain()->emplace<MeshFieldOperator>();
 * @endcode
 */
class MAYAFLUX_API MeshNetwork : public NodeNetwork {
public:
    MeshNetwork();
    ~MeshNetwork() override;

    // -------------------------------------------------------------------------
    // Slot management
    // -------------------------------------------------------------------------

    /**
     * @brief Add a slot to the network.
     * @param name    Logical name for lookup and logging.
     * @param node    MeshWriterNode carrying the slot's geometry, or null for a
     *                joint: a slot that only carries a transform for its children.
     * @param parent  Index of the parent slot, or nullopt for a root slot.
     * @return        Stable index of the new slot. Never changes after insertion.
     */
    uint32_t add_slot(
        std::string name,
        std::shared_ptr<GpuSync::MeshWriterNode> node,
        std::optional<uint32_t> parent = std::nullopt);

    /**
     * @brief Return the slot at the given index.
     * @param index Slot index as returned by add_slot().
     */
    [[nodiscard]] MeshSlot& get_slot(uint32_t index);
    [[nodiscard]] const MeshSlot& get_slot(uint32_t index) const;

    /**
     * @brief Set a slot's diffuse texture and hold it until replaced or the network ends.
     * @param index Slot index as returned by add_slot().
     * @param image Texture for the slot, or null to clear it.
     */
    void set_slot_texture(uint32_t index, std::shared_ptr<Core::VKImage> image);

    /**
     * @brief Find a slot by name.
     * @return Pointer to the slot, or nullptr if not found.
     */
    [[nodiscard]] MeshSlot* find_slot(std::string_view name);
    [[nodiscard]] const MeshSlot* find_slot(std::string_view name) const;

    /**
     * @brief Find the index of a slot by name.
     * @return Index, or nullopt if not found.
     */
    [[nodiscard]] std::optional<uint32_t> find_slot_index(std::string_view name) const;

    /**
     * @brief Number of slots in the network.
     */
    [[nodiscard]] size_t slot_count() const { return m_slots.size(); }

    /**
     * @brief Read-only access to the full slot list.
     */
    [[nodiscard]] const std::vector<MeshSlot>& slots() const { return m_slots; }
    [[nodiscard]] std::vector<MeshSlot>& slots() { return m_slots; }

    /**
     * @brief Every slot's geometry merged into one MeshData, one submesh per slot.
     *
     * Slots with no node or no geometry are skipped. Each submesh is named for
     * its slot, or by its index when the slot is unnamed, and its indices are
     * offset into the shared vertex array. Vertices are in each slot's local
     * space: no slot or world transform is applied.
     *
     * @return The merged mesh, or nullopt when no slot has geometry.
     */
    [[nodiscard]] std::optional<Kakshya::MeshData> get_mesh_data() const;

    /**
     * @brief Every slot's geometry merged into one MeshData, placed where the slots are.
     *
     * As get_mesh_data, but each slot's world transform is applied to its positions,
     * normals and tangents. The transform is composed from the local transforms up the
     * parent chain, so it is current whether or not the network has processed a block.
     *
     * @return The merged mesh, or nullopt when no slot has geometry.
     */
    [[nodiscard]] std::optional<Kakshya::MeshData> get_world_mesh_data() const;

    // -------------------------------------------------------------------------
    // NodeNetwork interface
    // -------------------------------------------------------------------------

    void process_batch(unsigned int num_samples) override;
    void reset() override;

    [[nodiscard]] size_t get_node_count() const override { return m_slots.size(); }
    [[nodiscard]] std::optional<double> get_node_output(size_t index) const override;
    [[nodiscard]] std::unordered_map<std::string, std::string> get_metadata() const override;

    // -------------------------------------------------------------------------
    // Operator management (mirrors ParticleNetwork pattern)
    // -------------------------------------------------------------------------

    /**
     * @brief Replace the primary operator.
     * @param op New operator. Must be non-null.
     */
    void set_operator(std::shared_ptr<NetworkOperator> op);

    /**
     * @brief Construct and set the primary operator in one call.
     * @tparam OpType Concrete operator type.
     * @return Shared pointer to the new operator.
     */
    template <typename OpType, typename... Args>
    std::shared_ptr<OpType> create_operator(Args&&... args)
    {
        auto op = std::make_shared<OpType>(std::forward<Args>(args)...);
        set_operator(op);
        return op;
    }

    NetworkOperator* get_operator() override
    {
        return m_operator.get();
    }

    const NetworkOperator* get_operator() const override
    {
        return m_operator.get();
    }

    bool has_operator() const override
    {
        return m_operator != nullptr;
    }

    /**
     * @brief Get the processing order of slots as indices into m_slots.
     *        Parents always appear before children.
     */
    [[nodiscard]] const std::vector<uint32_t>& sorted_indices() const { return m_sorted_indices; }

    /**
     * @brief Ensure the slot processing order is up to date. Called internally
     *        before processing, but can be called manually after making changes
     *        to the slot DAG.
     */
    void ensure_sorted();

private:
    std::vector<MeshSlot> m_slots;
    std::map<uint32_t, std::shared_ptr<Core::VKImage>> m_held_textures;

    /// @brief Processing order: indices into m_slots, parents before children.
    std::vector<uint32_t> m_sorted_indices;

    /// @brief Set when add_slot() changes the DAG and a re-sort is needed.
    bool m_sort_dirty { true };

    std::shared_ptr<NetworkOperator> m_operator;

    void rebuild_sort();
    void propagate_world_transforms();

    [[nodiscard]] glm::mat4 composed_transform(const MeshSlot& slot) const;
    [[nodiscard]] std::optional<Kakshya::MeshData> merged_mesh_data(bool placed) const;
};

/**
 * @brief Build a MeshNetwork from a MeshAssembly: one slot for every row, in row order.
 * @param assembly The assembly to read. Null gives nullptr.
 * @return A network where slot i is row i. A slot has the row's name and parent, its local
 *         transform is the row's local placement and its world transform the row's world
 *         placement. A row that shows a mesh gets a node of its own holding a copy of that
 *         mesh with its submeshes and polygon faces, so a mesh shared by several rows is
 *         copied into each. A joint, and a row whose source is points or curves, gets no
 *         node and only carries its transform. Nullptr for an assembly with no rows.
 *
 * Because slot i is row i, an assembly's motion plays on the network by writing one local
 * matrix per slot (Kriya::slot_sink). The material is not applied: a MeshAssemblyMaterial
 * holds a path, and loading and binding the texture is the caller's, with
 * MeshNetwork::set_slot_texture.
 */
[[nodiscard]] MAYAFLUX_API std::shared_ptr<MeshNetwork> make_mesh_network(
    const std::shared_ptr<const Kakshya::MeshAssemblyContainer>& assembly);

} // namespace MayaFlux::Nodes::Network
