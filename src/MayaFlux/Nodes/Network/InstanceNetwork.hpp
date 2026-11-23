#pragma once

#include "GeometrySlot.hpp"
#include "NodeNetwork.hpp"

namespace MayaFlux::Kakshya {
class MeshAssemblyContainer;
struct MeshAssemblyGroup;
}

namespace MayaFlux::Nodes::Network {

/**
 * @class InstanceNetwork
 * @brief NodeNetwork whose slots are flat, peer GeometrySlots for instanced rendering.
 *
 * The geometry drawn is the first slot's node, instanced once per slot, so a slot
 * is a transform and the slots usually share one node. No DAG, no parent
 * indices. process_batch() runs the operator chain then calls compute_frame() on
 * each slot's node. The buffer layer reads the slot list each cycle and uploads
 * per-instance transforms as an SSBO for a single instanced draw call. Per-vertex
 * data (color, normals, UVs) is owned by the first slot's node and is unchanged by
 * the instancing path.
 *
 * Usage:
 * @code
 * auto net = std::make_shared<InstanceNetwork>();
 * auto tmpl = std::make_shared<PathGeneratorNode>(...);
 *
 * for (uint32_t i = 0; i < 200; ++i) {
 *     uint32_t idx = net->add_slot("inst_" + std::to_string(i), tmpl);
 *     net->get_slot(idx).transform = glm::translate(glm::mat4{1}, positions[i]);
 * }
 *
 * auto op = net->create_operator<InstanceFieldOperator>();
 * op->bind_position(0, Kinesis::VectorField { [](glm::vec3 p) {
 *     return p + glm::vec3(0.01F, 0, 0);
 * }});
 * @endcode
 */
class MAYAFLUX_API InstanceNetwork : public NodeNetwork {
public:
    InstanceNetwork();
    ~InstanceNetwork() override = default;

    uint32_t add_slot(std::string name, std::shared_ptr<GpuSync::GeometryWriterNode> node);

    [[nodiscard]] GeometrySlot& get_slot(uint32_t index);
    [[nodiscard]] const GeometrySlot& get_slot(uint32_t index) const;
    [[nodiscard]] GeometrySlot* find_slot(std::string_view name);
    [[nodiscard]] const GeometrySlot* find_slot(std::string_view name) const;

    [[nodiscard]] size_t slot_count() const { return m_slots.size(); }
    [[nodiscard]] const std::vector<GeometrySlot>& slots() const { return m_slots; }
    [[nodiscard]] std::vector<GeometrySlot>& slots() { return m_slots; }

    void process_batch(unsigned int num_samples) override;
    void reset() override;

    [[nodiscard]] size_t get_node_count() const override { return m_slots.size(); }
    [[nodiscard]] std::optional<double> get_node_output(size_t index) const override;
    [[nodiscard]] std::unordered_map<std::string, std::string> get_metadata() const override;

    void set_operator(std::shared_ptr<NetworkOperator> op);

    template <typename OpType, typename... Args>
    std::shared_ptr<OpType> create_operator(Args&&... args)
    {
        auto op = std::make_shared<OpType>(std::forward<Args>(args)...);
        set_operator(op);
        return op;
    }

    NetworkOperator* get_operator() override { return m_operator.get(); }
    const NetworkOperator* get_operator() const override { return m_operator.get(); }
    bool has_operator() const override { return m_operator != nullptr; }

private:
    std::vector<GeometrySlot> m_slots;
    std::shared_ptr<NetworkOperator> m_operator;
};

/**
 * @brief Build an InstanceNetwork from one group of a MeshAssembly: every row that shows
 *        the same mesh with the same material.
 * @param assembly The assembly the group came from. Null gives nullptr.
 * @param group A group from assembly.groups().
 * @return A network with one slot per row, named from the row, all sharing one template
 *         node made from the group's mesh, each slot's transform the row's world
 *         placement. Nullptr when the group's source is not a mesh (points and curves
 *         sources are not instanced) or a row is out of range.
 *
 * The material is not applied: a MeshAssemblyMaterial holds a path, and loading and
 * binding the texture is the caller's, with InstanceNetworkBuffer::bind_instance_textures.
 * Every slot keeps the default payload.
 */
[[nodiscard]] MAYAFLUX_API std::shared_ptr<InstanceNetwork> make_instance_network(
    const std::shared_ptr<const Kakshya::MeshAssemblyContainer>& assembly,
    const Kakshya::MeshAssemblyGroup& group);

} // namespace MayaFlux::Nodes::Network
