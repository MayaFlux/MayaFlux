#include "RelationNetwork.hpp"

namespace MayaFlux::Nodes::Network {

RelationNetwork::RelationNetwork()
{
    set_output_mode(OutputMode::AUDIO_SINK);
    m_operator_chain = std::make_shared<OperatorChain>();
}

uint32_t RelationNetwork::add_slot(RelationSlot slot)
{
    constexpr uint32_t k_default_block_frames = 512;

    slot.index = static_cast<uint32_t>(m_slots.size());
    if (slot.node) {
        slot.node->m_fire_events_during_snapshot = true;
    }
    slot.reserve_block(k_default_block_frames);

    const uint32_t index = slot.index;
    m_slots.push_back(std::move(slot));

    return index;
}

std::vector<uint32_t> RelationNetwork::add_slots(std::initializer_list<RelationSlot> slots)
{
    std::vector<uint32_t> indices;
    indices.reserve(slots.size());
    for (const auto& slot : slots) {
        indices.push_back(add_slot(slot));
    }
    return indices;
}

RelationSlot& RelationNetwork::get_slot(uint32_t index)
{
    return m_slots[index];
}

const RelationSlot& RelationNetwork::get_slot(uint32_t index) const
{
    return m_slots[index];
}

std::optional<std::reference_wrapper<RelationSlot>> RelationNetwork::find_slot(std::string_view name)
{
    auto it = std::ranges::find_if(m_slots, [name](const RelationSlot& slot) { return slot.name == name; });
    if (it == m_slots.end()) {
        return std::nullopt;
    }
    return std::ref(*it);
}

std::optional<std::reference_wrapper<const RelationSlot>> RelationNetwork::find_slot(std::string_view name) const
{
    auto it = std::ranges::find_if(m_slots, [name](const RelationSlot& slot) { return slot.name == name; });
    if (it == m_slots.end()) {
        return std::nullopt;
    }
    return std::cref(*it);
}

std::optional<uint32_t> RelationNetwork::find_slot_index(std::string_view name) const
{
    for (uint32_t i = 0; i < static_cast<uint32_t>(m_slots.size()); ++i) {
        if (m_slots[i].name == name) {
            return i;
        }
    }
    return std::nullopt;
}

void RelationNetwork::process_batch(unsigned int num_samples)
{
    auto chain = get_operator_chain();

    for (const auto& op : chain->operators()) {
        if (auto* relation_op = dynamic_cast<RelationOperator*>(op.get())) {
            relation_op->set_slots(m_slots);
            relation_op->begin_block();
        }
    }

    chain->process(static_cast<float>(num_samples));

    while (m_audio_buffer_lock.test_and_set(std::memory_order_acquire))
        std::this_thread::yield();

    if (!m_publishers.empty() && m_publishers.front()->get_output_count() > 0) {
        auto block = m_publishers.front()->get_output_block(0);
        m_last_audio_buffer.assign(block.begin(), block.end());
        apply_output_scale();
    } else {
        m_last_audio_buffer.clear();
    }

    m_audio_buffer_lock.clear(std::memory_order_release);
}

std::optional<std::vector<double>> RelationNetwork::get_audio_buffer() const
{
    return NodeNetwork::get_audio_buffer();
}

std::optional<double> RelationNetwork::get_node_output(size_t index) const
{
    if (index >= m_slots.size()) {
        return std::nullopt;
    }
    return m_slots[index].level;
}

std::optional<std::span<const double>> RelationNetwork::get_node_audio_buffer(size_t index) const
{
    if (index >= m_slots.size()) {
        return std::nullopt;
    }
    return m_slots[index].block;
}

size_t RelationNetwork::get_output_count() const
{
    size_t total = 0;
    for (const auto& publisher : m_publishers) {
        total += publisher->get_output_count();
    }
    return total;
}

std::optional<std::span<const double>> RelationNetwork::get_output_block(size_t index) const
{
    for (const auto& publisher : m_publishers) {
        const size_t count = publisher->get_output_count();
        if (index < count) {
            return publisher->get_output_block(index);
        }
        index -= count;
    }
    return std::nullopt;
}

std::unordered_map<std::string, std::string> RelationNetwork::get_metadata() const
{
    auto meta = NodeNetwork::get_metadata();
    meta["slot_count"] = std::to_string(m_slots.size());
    meta["output_count"] = std::to_string(get_output_count());
    meta["operator_count"] = std::to_string(get_operator_chain()->size());
    return meta;
}

} // namespace MayaFlux::Nodes::Network
