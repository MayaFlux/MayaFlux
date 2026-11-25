#include "MeshNetwork.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Kakshya/NDData/MeshInsertion.hpp"
#include "MayaFlux/Kakshya/Semantic/MeshAssemblyContainer.hpp"
#include "MayaFlux/Nodes/Graphics/MeshWriterNode.hpp"
#include "MayaFlux/Kakshya/NDData/VertexPlacement.hpp"
#include "MayaFlux/Nodes/Network/Operators/MeshOperator.hpp"
#include "MayaFlux/Portal/Graphics/TextureLoom.hpp"

namespace MayaFlux::Nodes::Network {

namespace {

    /** @brief Give a slot the morph targets its row's source keeps, once per source. */
    void adopt_morphs(MeshSlot& slot, const Kakshya::MeshAssemblyContainer& assembly,
        const std::optional<uint64_t>& source,
        std::map<uint64_t, std::shared_ptr<const Kakshya::MeshMorphTargets>>& held)
    {
        if (!slot.node || !source) {
            return;
        }
        const auto [it, inserted] = held.try_emplace(*source);
        if (inserted) {
            if (const auto* morphs = assembly.get_data().sources[*source].facets.get<Kakshya::MeshMorphTargets>()) {
                it->second = std::make_shared<const Kakshya::MeshMorphTargets>(*morphs);
            }
        }
        slot.morphs = it->second;
    }

}

// =============================================================================
// Construction
// =============================================================================

MeshNetwork::MeshNetwork()
{
    set_output_mode(OutputMode::GRAPHICS_BIND);
    set_topology(Topology::CUSTOM);
}

MeshNetwork::~MeshNetwork()
{
    auto& loom = Portal::Graphics::get_texture_manager();
    for (const auto& [index, image] : m_held_textures) {
        loom.release_image(image);
    }
}

// =============================================================================
// Slot management
// =============================================================================

void MeshNetwork::set_slot_texture(uint32_t index, std::shared_ptr<Core::VKImage> image)
{
    auto& slot = get_slot(index);
    auto& loom = Portal::Graphics::get_texture_manager();

    loom.retain_image(image);
    if (const auto held = m_held_textures.find(index); held != m_held_textures.end()) {
        loom.release_image(held->second);
        held->second = image;
    } else if (image) {
        m_held_textures.emplace(index, image);
    }
    slot.diffuse_texture = std::move(image);
}

uint32_t MeshNetwork::add_slot(
    std::string name,
    std::shared_ptr<GpuSync::MeshWriterNode> node,
    std::optional<uint32_t> parent)
{
    if (parent.has_value() && parent.value() >= m_slots.size()) {
        error<std::out_of_range>(
            Journal::Component::Nodes, Journal::Context::Init,
            std::source_location::current(),
            "MeshNetwork::add_slot: parent index {} out of range (slot count: {})",
            parent.value(), m_slots.size());
    }

    const auto idx = static_cast<uint32_t>(m_slots.size());

    MeshSlot slot;
    slot.index = idx;
    slot.name = std::move(name);
    slot.node = std::move(node);
    slot.parent_index = parent;
    slot.dirty = true;

    if (parent.has_value()) {
        m_slots[parent.value()].child_indices.push_back(idx);
    }

    m_slots.push_back(std::move(slot));
    m_sort_dirty = true;

    MF_DEBUG(Journal::Component::Nodes, Journal::Context::Init,
        "MeshNetwork: added slot '{}' at index {} (parent: {})",
        m_slots[idx].name, idx,
        parent.has_value() ? std::to_string(parent.value()) : "none");

    return idx;
}

MeshSlot& MeshNetwork::get_slot(uint32_t index)
{
    return m_slots.at(index);
}

const MeshSlot& MeshNetwork::get_slot(uint32_t index) const
{
    return m_slots.at(index);
}

MeshSlot* MeshNetwork::find_slot(std::string_view name)
{
    auto it = std::ranges::find_if(m_slots,
        [name](const MeshSlot& s) { return s.name == name; });
    return it != m_slots.end() ? &*it : nullptr;
}

const MeshSlot* MeshNetwork::find_slot(std::string_view name) const
{
    auto it = std::ranges::find_if(m_slots,
        [name](const MeshSlot& s) { return s.name == name; });
    return it != m_slots.end() ? &*it : nullptr;
}

std::optional<uint32_t> MeshNetwork::find_slot_index(std::string_view name) const
{
    for (uint32_t i = 0; i < static_cast<uint32_t>(m_slots.size()); ++i) {
        if (m_slots[i].name == name)
            return i;
    }
    return std::nullopt;
}

// =============================================================================
// NodeNetwork interface
// =============================================================================

void MeshNetwork::process_batch(unsigned int num_samples)
{
    if (!is_enabled() || m_slots.empty())
        return;

    if (m_sort_dirty)
        rebuild_sort();

    const float dt = get_timestep();

    for (unsigned int frame = 0; frame < num_samples; ++frame) {
        propagate_world_transforms();

        if (m_operator) {
            if (auto* mesh_op = dynamic_cast<MeshOperator*>(m_operator.get()))
                mesh_op->set_slots(m_slots, m_sorted_indices);
            m_operator->process(dt);
        }

        if (m_operator_chain) {
            for (const auto& op : m_operator_chain->operators()) {
                if (auto* mesh_op = dynamic_cast<MeshOperator*>(op.get()))
                    mesh_op->set_slots(m_slots, m_sorted_indices);
                op->process(dt);
            }
        }
    }

    for (const auto& slot : m_slots) {
        if (slot.node)
            slot.node->compute_frame();
    }
}

void MeshNetwork::reset()
{
    for (auto& slot : m_slots) {
        slot.local_transform = glm::mat4(1.0F);
        slot.world_transform = glm::mat4(1.0F);
        slot.dirty = true;
    }
}

std::optional<double> MeshNetwork::get_node_output(size_t index) const
{
    if (index >= m_slots.size())
        return std::nullopt;
    return static_cast<double>(index);
}

std::unordered_map<std::string, std::string> MeshNetwork::get_metadata() const
{
    auto meta = NodeNetwork::get_metadata();
    meta["slot_count"] = std::to_string(m_slots.size());
    meta["operator"] = m_operator ? std::string(m_operator->get_type_name()) : "none";
    meta["chain_size"] = m_operator_chain
        ? std::to_string(m_operator_chain->size())
        : "0";
    return meta;
}

// =============================================================================
// Operator management
// =============================================================================

void MeshNetwork::set_operator(std::shared_ptr<NetworkOperator> op)
{
    if (!op) {
        MF_ERROR(Journal::Component::Nodes, Journal::Context::Init,
            "MeshNetwork::set_operator: null operator ignored");
        return;
    }
    m_operator = std::move(op);
}

void MeshNetwork::ensure_sorted()
{
    if (m_sort_dirty)
        rebuild_sort();
}

// =============================================================================
// Private helpers
// =============================================================================

void MeshNetwork::rebuild_sort()
{
    // Roots: slots with no parent.
    std::vector<uint32_t> queue;
    for (uint32_t i = 0; i < static_cast<uint32_t>(m_slots.size()); ++i) {
        if (!m_slots[i].parent_index.has_value())
            queue.push_back(i);
    }

    m_sorted_indices.clear();
    m_sorted_indices.reserve(m_slots.size());

    while (!queue.empty()) {
        uint32_t idx = queue.front();
        queue.erase(queue.begin());
        m_sorted_indices.push_back(idx);

        for (uint32_t child : m_slots[idx].child_indices)
            queue.push_back(child);
    }

    if (m_sorted_indices.size() != m_slots.size()) {
        MF_ERROR(Journal::Component::Nodes, Journal::Context::Init,
            "MeshNetwork: slot DAG contains a cycle — sort is incomplete");
    }

    m_sort_dirty = false;

    MF_DEBUG(Journal::Component::Nodes, Journal::Context::Init,
        "MeshNetwork: rebuilt slot sort order ({} slots)", m_sorted_indices.size());
}

void MeshNetwork::propagate_world_transforms()
{
    for (uint32_t idx : m_sorted_indices) {
        auto& slot = m_slots[idx];
        if (slot.parent_index.has_value()) {
            slot.world_transform
                = m_slots[slot.parent_index.value()].world_transform
                * slot.local_transform;
        } else {
            slot.world_transform = slot.local_transform;
        }
    }
}

std::optional<Kakshya::MeshData> MeshNetwork::get_mesh_data() const
{
    return merged_mesh_data(false);
}

std::optional<Kakshya::MeshData> MeshNetwork::get_world_mesh_data() const
{
    return merged_mesh_data(true);
}

glm::mat4 MeshNetwork::composed_transform(const MeshSlot& slot) const
{
    glm::mat4 world = slot.local_transform;
    std::optional<uint32_t> parent = slot.parent_index;
    for (size_t hops = 0; parent && hops < m_slots.size(); ++hops) {
        const MeshSlot& above = m_slots[*parent];
        world = above.local_transform * world;
        parent = above.parent_index;
    }
    return world;
}

std::optional<Kakshya::MeshData> MeshNetwork::merged_mesh_data(bool placed) const
{
    auto mesh_data = Kakshya::MeshData::empty();
    Kakshya::MeshInsertion ins(mesh_data.vertex_variant, mesh_data.index_variant);

    for (const auto& slot : m_slots) {
        if (!slot.node) {
            continue;
        }

        auto packed = slot.node->get_mesh_data();
        if (!packed) {
            continue;
        }

        const std::string name = slot.name.empty() ? std::to_string(slot.index) : slot.name;
        const glm::mat4 transform = placed ? composed_transform(slot) : glm::mat4(1.0F);

        const size_t part_count = packed->submeshes ? packed->submeshes->regions.size() : 0;
        std::vector<Kakshya::MeshData> parts;
        uint64_t covered_indices = 0;
        uint64_t covered_vertices = 0;
        for (size_t i = 0; i < part_count; ++i) {
            auto part = packed->extract_submesh(i);
            const auto* range = packed->submeshes->regions[i].get<Kakshya::MeshSubrange>();
            if (!part || !range) {
                parts.clear();
                break;
            }
            covered_indices += range->index_count;
            covered_vertices += range->vertex_count;
            parts.push_back(std::move(*part));
        }

        const bool keeps_parts = !parts.empty() && parts.size() == part_count
            && covered_indices == std::get<std::vector<uint32_t>>(packed->index_variant).size()
            && covered_vertices == packed->vertex_count();

        if (!keeps_parts) {
            if (placed) {
                Kakshya::place_vertices(
                    std::get<std::vector<uint8_t>>(packed->vertex_variant), packed->layout, transform);
            }
            ins.insert_submesh(
                std::get<std::vector<uint8_t>>(packed->vertex_variant),
                std::get<std::vector<uint32_t>>(packed->index_variant),
                name, {}, packed->layout);
            continue;
        }

        for (auto& part : parts) {
            auto& vertices = std::get<std::vector<uint8_t>>(part.vertex_variant);
            if (placed) {
                Kakshya::place_vertices(vertices, part.layout, transform);
            }

            Kakshya::Region carried = part.submeshes->regions.front();
            auto range = *carried.get<Kakshya::MeshSubrange>();
            if (range.name.empty()) {
                range.name = name;
            }
            carried.set(range);
            ins.insert_submesh(vertices, std::get<std::vector<uint32_t>>(part.index_variant), carried, part.layout);
        }
    }

    const auto access = ins.build();
    if (!access) {
        return std::nullopt;
    }
    mesh_data.layout = access->layout;
    mesh_data.submeshes = access->submeshes;

    return mesh_data;
}

std::shared_ptr<MeshNetwork> make_mesh_network(
    const std::shared_ptr<const Kakshya::MeshAssemblyContainer>& assembly)
{
    if (!assembly) {
        return nullptr;
    }

    const size_t rows = assembly->get_total_elements();
    if (rows == 0) {
        return nullptr;
    }

    const auto world = assembly->world_transforms();
    auto network = std::make_shared<MeshNetwork>();

    for (size_t row = 0; row < rows; ++row) {
        const auto placement = assembly->placement(row);
        if (!placement) {
            return nullptr;
        }

        std::shared_ptr<GpuSync::MeshWriterNode> node;
        if (placement->source) {
            if (const auto* mesh = assembly->mesh(*placement->source)) {
                node = std::make_shared<GpuSync::MeshWriterNode>(mesh->vertex_count());
                node->set_mesh(*mesh);
            }
        }

        std::optional<uint32_t> parent;
        if (placement->parent) {
            parent = static_cast<uint32_t>(*placement->parent);
        }

        const auto index = network->add_slot(placement->name, std::move(node), parent);
        auto& slot = network->get_slot(index);
        slot.local_transform = placement->local;
        slot.world_transform = world[row];
    }

    std::map<uint64_t, std::shared_ptr<const Kakshya::MeshMorphTargets>> morphs;
    for (size_t row = 0; row < rows; ++row) {
        const auto placement = assembly->placement(row);
        auto& slot = network->get_slot(static_cast<uint32_t>(row));
        adopt_morphs(slot, *assembly, placement->source, morphs);
        const auto* skeleton = placement->skeleton ? assembly->skeleton(*placement->skeleton) : nullptr;
        const auto skin = placement->source ? assembly->skin(*placement->source) : nullptr;
        if (slot.node && skin && skeleton && skeleton->size() == skin->joint_count()) {
            slot.skin = skin;
            slot.joints.assign(skeleton->begin(), skeleton->end());
        }
    }
    return network;
}

std::optional<MeshNetworkPart> make_mesh_network(
    const std::shared_ptr<const Kakshya::MeshAssemblyContainer>& assembly,
    std::span<const uint64_t> rows,
    MeshPartPlacement placement)
{
    if (!assembly || rows.empty()) {
        return std::nullopt;
    }

    const size_t total = assembly->get_total_elements();
    std::vector<uint64_t> selected(rows.begin(), rows.end());
    std::ranges::sort(selected);
    selected.erase(std::ranges::unique(selected).begin(), selected.end());
    if (selected.back() >= total) {
        return std::nullopt;
    }

    std::vector<bool> shown(total, false);
    for (const uint64_t row : selected) {
        shown[row] = true;
    }

    std::vector<bool> included = shown;
    if (placement == MeshPartPlacement::Chained) {
        std::vector<bool> walked(total, false);
        for (const uint64_t row : selected) {
            auto parent = assembly->placement(row)->parent;
            while (parent && !walked[*parent]) {
                walked[*parent] = true;
                included[*parent] = true;
                parent = assembly->placement(*parent)->parent;
            }
        }
    }

    const auto world = assembly->world_transforms();
    MeshNetworkPart part { .network = std::make_shared<MeshNetwork>(), .rows = {} };
    std::vector<std::optional<uint32_t>> slot_of(total);
    bool any_mesh = false;

    for (size_t row = 0; row < total; ++row) {
        if (!included[row]) {
            continue;
        }
        const auto held = assembly->placement(row);
        if (!held) {
            return std::nullopt;
        }

        std::shared_ptr<GpuSync::MeshWriterNode> node;
        if (shown[row] && held->source) {
            if (const auto* mesh = assembly->mesh(*held->source)) {
                node = std::make_shared<GpuSync::MeshWriterNode>(mesh->vertex_count());
                node->set_mesh(*mesh);
            }
        }

        const bool flat = placement == MeshPartPlacement::Flat;
        if (flat && !node) {
            continue;
        }
        any_mesh = any_mesh || node != nullptr;

        std::optional<uint32_t> parent;
        if (!flat && held->parent) {
            parent = slot_of[*held->parent];
        }

        const auto index = part.network->add_slot(held->name, std::move(node), parent);
        auto& slot = part.network->get_slot(index);
        slot.local_transform = flat ? world[row] : held->local;
        slot.world_transform = world[row];
        slot_of[row] = index;
        part.rows.push_back(row);
    }

    if (!any_mesh) {
        return std::nullopt;
    }

    std::map<uint64_t, std::shared_ptr<const Kakshya::MeshMorphTargets>> morphs;
    for (size_t index = 0; index < part.rows.size(); ++index) {
        const auto held = assembly->placement(part.rows[index]);
        auto& slot = part.network->get_slot(static_cast<uint32_t>(index));
        adopt_morphs(slot, *assembly, held->source, morphs);
        const auto* skeleton = held->skeleton ? assembly->skeleton(*held->skeleton) : nullptr;
        const auto skin = held->source ? assembly->skin(*held->source) : nullptr;
        if (!slot.node || !skin || !skeleton || skeleton->size() != skin->joint_count()) {
            continue;
        }

        slot.skin = skin;
        const bool chained = placement == MeshPartPlacement::Chained;
        if (chained && std::ranges::all_of(*skeleton, [&](uint64_t joint) { return slot_of[joint].has_value(); })) {
            for (const uint64_t joint : *skeleton) {
                slot.joints.push_back(*slot_of[joint]);
            }
        }
    }
    return part;
}

} // namespace MayaFlux::Nodes::Network
