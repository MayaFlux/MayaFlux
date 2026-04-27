#pragma once

#include "MeshSlot.hpp"
#include "NodeNetwork.hpp"

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
    ~MeshNetwork() override = default;

    // -------------------------------------------------------------------------
    // Slot management
    // -------------------------------------------------------------------------

    /**
     * @brief Add a slot to the network.
     * @param name    Logical name for lookup and logging.
     * @param node    MeshWriterNode carrying the slot's geometry.
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

    /// @brief Processing order: indices into m_slots, parents before children.
    std::vector<uint32_t> m_sorted_indices;

    /// @brief Set when add_slot() changes the DAG and a re-sort is needed.
    bool m_sort_dirty { true };

    std::shared_ptr<NetworkOperator> m_operator;

    void rebuild_sort();
    void propagate_world_transforms();
};

} // namespace MayaFlux::Nodes::Network
