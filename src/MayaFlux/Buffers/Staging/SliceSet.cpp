#include "SliceSet.hpp"

#include "StagingUtils.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Buffers {

SliceSet::SliceSet(uint32_t stride_bytes, Portal::Graphics::PrimitiveTopology topology)
    : m_stride(stride_bytes)
    , m_topology(topology)
{
    if (m_stride == 0) {
        error<std::invalid_argument>(
            Journal::Component::Buffers,
            Journal::Context::Init,
            std::source_location::current(),
            "SliceSet: stride must be nonzero");
    }
}

SliceSet::SliceId SliceSet::add(std::span<const uint8_t> bytes)
{
    if (bytes.size() % m_stride != 0) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferManagement,
            "SliceSet::add: {} bytes is not a whole number of {} byte vertices",
            bytes.size(), m_stride);
        return 0;
    }

    const SliceId id = m_next_id++;
    const size_t offset = m_mirror.size();

    m_mirror.insert(m_mirror.end(), bytes.begin(), bytes.end());
    m_slices.emplace(id, Slice { .byte_offset = offset, .byte_size = bytes.size() });
    m_memory_order.push_back(id);
    m_draw_order.push_back(id);

    mark_dirty(offset, m_mirror.size());
    m_runs_changed = true;
    return id;
}

void SliceSet::update(SliceId id, std::span<const uint8_t> bytes)
{
    auto it = m_slices.find(id);
    if (it == m_slices.end() || bytes.size() % m_stride != 0) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferManagement,
            "SliceSet::update: unknown slice {} or {} bytes is not a whole number of {} byte vertices",
            id, bytes.size(), m_stride);
        return;
    }

    auto& slice = it->second;

    if (bytes.size() == slice.byte_size) {
        if (!bytes.empty()) {
            std::memcpy(m_mirror.data() + slice.byte_offset, bytes.data(), bytes.size());
            mark_dirty(slice.byte_offset, slice.byte_offset + bytes.size());
        }
        return;
    }

    const auto first = m_mirror.begin() + static_cast<std::ptrdiff_t>(slice.byte_offset);
    m_mirror.erase(first, first + static_cast<std::ptrdiff_t>(slice.byte_size));
    m_mirror.insert(m_mirror.begin() + static_cast<std::ptrdiff_t>(slice.byte_offset),
        bytes.begin(), bytes.end());
    slice.byte_size = bytes.size();

    const auto position = std::ranges::find(m_memory_order, id);
    repack_from(static_cast<size_t>(position - m_memory_order.begin()));
    mark_dirty(slice.byte_offset, m_mirror.size());
    m_runs_changed = true;
}

void SliceSet::remove(SliceId id)
{
    auto it = m_slices.find(id);
    if (it == m_slices.end()) {
        return;
    }

    const Slice slice = it->second;
    const auto first = m_mirror.begin() + static_cast<std::ptrdiff_t>(slice.byte_offset);
    m_mirror.erase(first, first + static_cast<std::ptrdiff_t>(slice.byte_size));

    const auto position = std::ranges::find(m_memory_order, id);
    const auto index = static_cast<size_t>(position - m_memory_order.begin());
    m_memory_order.erase(position);
    std::erase(m_draw_order, id);
    m_slices.erase(it);

    repack_from(index);
    mark_dirty(slice.byte_offset, m_mirror.size());
    m_runs_changed = true;
}

void SliceSet::set_visible(SliceId id, bool visible)
{
    auto it = m_slices.find(id);
    if (it == m_slices.end() || it->second.visible == visible) {
        return;
    }

    it->second.visible = visible;
    m_runs_changed = true;
}

void SliceSet::bring_to_front(SliceId id)
{
    if (!m_slices.contains(id) || m_draw_order.back() == id) {
        return;
    }

    std::erase(m_draw_order, id);
    m_draw_order.push_back(id);
    m_runs_changed = true;
}

void SliceSet::send_to_back(SliceId id)
{
    if (!m_slices.contains(id) || m_draw_order.front() == id) {
        return;
    }

    std::erase(m_draw_order, id);
    m_draw_order.insert(m_draw_order.begin(), id);
    m_runs_changed = true;
}

uint32_t SliceSet::vertex_count() const noexcept
{
    return static_cast<uint32_t>(m_mirror.size() / m_stride);
}

std::optional<std::vector<Portal::Graphics::DrawRun>> SliceSet::take_runs()
{
    if (!m_runs_changed) {
        return std::nullopt;
    }
    m_runs_changed = false;

    std::vector<Portal::Graphics::DrawRun> runs;
    for (const SliceId id : m_draw_order) {
        const auto& slice = m_slices.at(id);
        if (!slice.visible || slice.byte_size == 0) {
            continue;
        }

        const auto offset = static_cast<uint32_t>(slice.byte_offset / m_stride);
        const auto count = static_cast<uint32_t>(slice.byte_size / m_stride);

        if (!runs.empty() && runs.back().vertex_offset + runs.back().vertex_count == offset) {
            runs.back().vertex_count += count;
            continue;
        }

        runs.push_back({ .topology = m_topology, .vertex_offset = offset, .vertex_count = count });
    }

    return runs;
}

void SliceSet::flush(const std::shared_ptr<VKBuffer>& target, std::shared_ptr<VKBuffer>& staging)
{
    if (!target) {
        error<std::invalid_argument>(
            Journal::Component::Buffers,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "SliceSet::flush: target buffer is null");
    }

    const size_t capacity_before = target->get_size_bytes();
    ensure_gpu_capacity(target, staging, m_mirror.size());
    if (target->get_size_bytes() != capacity_before) {
        m_all_dirty = true;
    }

    if (m_all_dirty) {
        m_dirty.clear();
        m_dirty.emplace_back(0, m_mirror.size());
        m_all_dirty = false;
    }

    for (auto& [start, end] : m_dirty) {
        end = std::min(end, m_mirror.size());
    }

    upload_intervals(m_mirror.data(), m_dirty, target, staging);
    m_dirty.clear();
}

void SliceSet::repack_from(size_t memory_index)
{
    size_t offset = 0;
    if (memory_index > 0) {
        const auto& previous = m_slices.at(m_memory_order[memory_index - 1]);
        offset = previous.byte_offset + previous.byte_size;
    }

    for (size_t i = memory_index; i < m_memory_order.size(); ++i) {
        auto& slice = m_slices.at(m_memory_order[i]);
        slice.byte_offset = offset;
        offset += slice.byte_size;
    }
}

void SliceSet::mark_dirty(size_t start, size_t end)
{
    if (start < end) {
        m_dirty.emplace_back(start, end);
    }
}

} // namespace MayaFlux::Buffers
