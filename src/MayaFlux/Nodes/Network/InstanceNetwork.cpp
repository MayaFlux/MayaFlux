#include "InstanceNetwork.hpp"

#include "Operators/InstanceOperator.hpp"

#include "MayaFlux/Kakshya/Semantic/MeshAssemblyContainer.hpp"
#include "MayaFlux/Kakshya/NDData/SpatialAccess.hpp"
#include "MayaFlux/Kakshya/NDData/VertexFormats.hpp"
#include "MayaFlux/Kinesis/Polyhedral/SkinPalette.hpp"
#include "MayaFlux/Nodes/Graphics/MeshWriterNode.hpp"
#include "MayaFlux/Nodes/Graphics/PointCollectionNode.hpp"

#include "MayaFlux/Transitive/Reflect/TypeInfo.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Nodes::Network {

InstanceNetwork::InstanceNetwork()
{
    set_output_mode(OutputMode::GRAPHICS_BIND);
}

uint32_t InstanceNetwork::add_slot(
    std::string name, std::shared_ptr<GpuSync::GeometryWriterNode> node)
{
    if (!node) {
        error<std::invalid_argument>(
            Journal::Component::Nodes, Journal::Context::Init,
            std::source_location::current(),
            "InstanceNetwork::add_slot: null GeometryWriterNode for slot '{}'", name);
    }

    const auto idx = static_cast<uint32_t>(m_slots.size());
    if (name.empty()) {
        name = "slot_" + std::to_string(idx) + "_" + Reflect::short_dynamic_type_name(*node);
    }
    m_slots.push_back({ .index = idx, .name = std::move(name), .node = std::move(node) });
    return idx;
}

GeometrySlot& InstanceNetwork::get_slot(uint32_t index) { return m_slots.at(index); }
const GeometrySlot& InstanceNetwork::get_slot(uint32_t index) const { return m_slots.at(index); }

GeometrySlot* InstanceNetwork::find_slot(std::string_view name)
{
    auto it = std::ranges::find_if(
        m_slots, [name](const GeometrySlot& s) { return s.name == name; });
    return it != m_slots.end() ? &*it : nullptr;
}

const GeometrySlot* InstanceNetwork::find_slot(std::string_view name) const
{
    auto it = std::ranges::find_if(
        m_slots, [name](const GeometrySlot& s) { return s.name == name; });
    return it != m_slots.end() ? &*it : nullptr;
}

void InstanceNetwork::set_operator(std::shared_ptr<NetworkOperator> op)
{
    m_operator = std::move(op);
}

void InstanceNetwork::process_batch(unsigned int num_samples)
{
    if (!is_enabled() || m_slots.empty())
        return;

    for (unsigned int frame = 0; frame < num_samples; ++frame) {
        if (m_operator) {
            if (auto* inst_op = dynamic_cast<InstanceOperator*>(m_operator.get()))
                inst_op->set_slots(m_slots);
            m_operator->process(0.0F);
        }

        if (m_operator_chain) {
            for (const auto& op : m_operator_chain->operators()) {
                if (auto* inst_op = dynamic_cast<InstanceOperator*>(op.get()))
                    inst_op->set_slots(m_slots);
                op->process(0.0F);
            }
        }
    }

    for (const auto& slot : m_slots) {
        if (slot.node)
            slot.node->compute_frame();
    }
}

void InstanceNetwork::reset()
{
    for (auto& slot : m_slots) {
        slot.transform = glm::mat4(1.0F);
        slot.dirty = true;
    }
}

std::optional<double> InstanceNetwork::get_node_output(size_t index) const
{
    if (index >= m_slots.size())
        return std::nullopt;
    return static_cast<double>(index);
}

std::unordered_map<std::string, std::string> InstanceNetwork::get_metadata() const
{
    auto meta = NodeNetwork::get_metadata();
    meta["slot_count"] = std::to_string(m_slots.size());
    meta["operator"] = m_operator ? std::string(m_operator->get_type_name()) : "none";
    meta["chain_size"] = m_operator_chain
        ? std::to_string(m_operator_chain->size())
        : "0";
    return meta;
}

std::shared_ptr<InstanceNetwork> make_instance_network(
    const std::shared_ptr<const Kakshya::MeshAssemblyContainer>& assembly,
    const Kakshya::MeshAssemblyGroup& group)
{
    if (!assembly) {
        return nullptr;
    }

    const auto* mesh = assembly->mesh(group.source);
    const auto* points = assembly->spatial(group.source);
    if (!mesh && (!points || points->topology != Portal::Graphics::PrimitiveTopology::POINT_LIST)) {
        return nullptr;
    }

    const auto world = assembly->world_transforms();
    if (std::ranges::any_of(group.rows, [&world](uint64_t row) { return row >= world.size(); })) {
        return nullptr;
    }

    std::shared_ptr<GpuSync::GeometryWriterNode> node;
    if (mesh) {
        auto mesh_node = std::make_shared<GpuSync::MeshWriterNode>();
        mesh_node->set_mesh(*mesh);
        node = std::move(mesh_node);
    } else {
        const auto access = Kakshya::as_spatial_vertex_access(*points);
        if (!access || access->byte_count % sizeof(Kakshya::PointVertex) != 0) {
            return nullptr;
        }
        std::vector<Kakshya::PointVertex> vertices(access->byte_count / sizeof(Kakshya::PointVertex));
        std::memcpy(vertices.data(), access->data_ptr, access->byte_count);
        node = std::make_shared<GpuSync::PointCollectionNode>(std::move(vertices));
    }

    const auto skin = mesh ? assembly->skin(group.source) : nullptr;
    std::shared_ptr<const Kakshya::MeshMorphTargets> morphs;
    if (const auto* held = assembly->get_data().sources[group.source].facets.get<Kakshya::MeshMorphTargets>(); held && mesh) {
        morphs = std::make_shared<const Kakshya::MeshMorphTargets>(*held);
    }

    auto network = std::make_shared<InstanceNetwork>();
    for (const uint64_t row : group.rows) {
        const auto placement = assembly->placement(row);
        const auto index = network->add_slot(placement ? placement->name : std::string {}, node);
        auto& slot = network->get_slot(index);
        slot.transform = world[row];
        slot.skin = skin;
        slot.morphs = morphs;

        const auto* skeleton = placement && placement->skeleton ? assembly->skeleton(*placement->skeleton) : nullptr;
        if (skin && skeleton) {
            std::vector<glm::mat4> joints;
            joints.reserve(skeleton->size());
            for (const uint64_t joint : *skeleton) {
                joints.push_back(world[joint]);
            }
            slot.palette = Kinesis::skin_palette(world[row], joints, skin->inverse_bind);
        }
        if (morphs) {
            for (const auto& [name, shape] : *morphs) {
                slot.morph_weights.push_back(shape.empty() ? 0.0F : shape.front().weight);
            }
        }
    }
    return network;
}

} // namespace MayaFlux::Nodes::Network
