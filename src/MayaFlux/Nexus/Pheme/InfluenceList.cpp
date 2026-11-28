#include "InfluenceList.hpp"

#include "MayaFlux/Buffers/Shaders/RenderProcessor.hpp"
#include "MayaFlux/Buffers/VKBuffer.hpp"
#include "MayaFlux/Kinesis/Radiometry/Prefilter.hpp"
#include "MayaFlux/Kinesis/Radiometry/TransformedCosine.hpp"
#include "MayaFlux/Registry/BackendRegistry.hpp"
#include "MayaFlux/Registry/Service/BufferService.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Nexus {

namespace {

    constexpr size_t k_header_bytes = 4 * sizeof(uint32_t);

    constexpr size_t surround_texel_count()
    {
        size_t texels = 0;
        for (uint32_t level = 0; level < Kinesis::k_surround_levels; ++level) {
            texels += static_cast<size_t>(Kinesis::k_surround_width >> level) * (Kinesis::k_surround_height >> level);
        }
        return texels;
    }

    constexpr size_t k_table_entries = static_cast<size_t>(Kinesis::k_albedo_table_size) * Kinesis::k_albedo_table_size;
    constexpr size_t k_lobe_entries = static_cast<size_t>(Kinesis::k_lobe_table_size) * Kinesis::k_lobe_table_size;
    constexpr size_t k_table_offset = k_header_bytes + 9 * sizeof(glm::vec4);
    constexpr size_t k_lobe_offset = k_table_offset + k_table_entries * sizeof(glm::vec4);
    constexpr size_t k_surround_vectors = 9 + k_table_entries + k_lobe_entries + surround_texel_count();
    constexpr size_t k_surround_bytes = k_header_bytes + k_surround_vectors * sizeof(glm::vec4);

    std::shared_ptr<Buffers::VKBuffer> zeroed_storage(size_t bytes)
    {
        auto buffer = std::make_shared<Buffers::VKBuffer>(
            bytes, Buffers::VKBuffer::Usage::HOST_STORAGE, Kakshya::DataModality::UNKNOWN);

        const auto service = Registry::BackendRegistry::instance().get_service<Registry::Service::BufferService>();
        if (!service) {
            error<std::runtime_error>(Journal::Component::Nexus, Journal::Context::Init, std::source_location::current(),
                "InfluenceList needs a buffer service");
        }
        service->initialize_buffer(buffer);

        std::memset(buffer->get_mapped_ptr(), 0, bytes);
        buffer->mark_dirty_range(0, bytes);
        return buffer;
    }

}

struct InfluenceList::Places {
    std::shared_ptr<Buffers::VKBuffer> buffer;
    std::shared_ptr<Buffers::VKBuffer> surround;
    std::unique_ptr<std::atomic<bool>[]> taken;
    std::atomic<uint32_t> used { 0 };
    uint32_t capacity {};

    void write(size_t offset, const void* data, size_t bytes) const
    {
        auto* mapped = static_cast<uint8_t*>(buffer->get_mapped_ptr());
        std::memcpy(mapped + offset, data, bytes);
        buffer->mark_dirty_range(offset, bytes);
    }

    void put(uint32_t place, const InfluenceBlock& block) const
    {
        write(k_header_bytes + place * sizeof(InfluenceBlock), &block, sizeof(InfluenceBlock));
    }
};

InfluenceList::InfluenceList(uint32_t capacity)
    : m_places(std::make_shared<Places>())
{
    m_places->capacity = std::max(capacity, 1U);
    m_places->taken = std::make_unique<std::atomic<bool>[]>(m_places->capacity);

    m_places->buffer = zeroed_storage(k_header_bytes + m_places->capacity * sizeof(InfluenceBlock));
    m_places->surround = zeroed_storage(k_surround_bytes);
}

void InfluenceList::bind(const std::shared_ptr<Buffers::RenderProcessor>& renderer) const
{
    if (renderer) {
        renderer->bind_buffer("influences", m_places->buffer);
        renderer->bind_buffer("surround", m_places->surround);
    }
}

void InfluenceList::set_surround(const Kinesis::SurroundRadiance& surround, float scale) const
{
    bool sized = surround.albedo.size() == k_table_entries;
    for (uint32_t level = 0; level < Kinesis::k_surround_levels; ++level) {
        sized = sized && surround.levels[level].size() == static_cast<size_t>(Kinesis::k_surround_width >> level) * (Kinesis::k_surround_height >> level);
    }
    if (!sized) {
        MF_WARN(Journal::Component::Nexus, Journal::Context::Runtime,
            "InfluenceList: surroundings of the wrong size are ignored");
        return;
    }

    std::vector<glm::vec4> head;
    head.reserve(9 + k_table_entries);
    for (const auto& coefficient : surround.harmonics) {
        head.emplace_back(coefficient * scale, 0.0F);
    }
    for (const auto& entry : surround.albedo) {
        head.emplace_back(entry.x, entry.y, 0.0F, 0.0F);
    }
    std::vector<glm::vec4> texels;
    texels.reserve(surround_texel_count());
    for (const auto& level : surround.levels) {
        for (const auto& texel : level) {
            texels.emplace_back(texel * scale, 0.0F);
        }
    }

    auto* mapped = static_cast<uint8_t*>(m_places->surround->get_mapped_ptr());
    const uint32_t present = 1;
    std::memcpy(mapped + k_header_bytes, head.data(), head.size() * sizeof(glm::vec4));
    std::memcpy(mapped + k_lobe_offset + k_lobe_entries * sizeof(glm::vec4), texels.data(), texels.size() * sizeof(glm::vec4));
    std::memcpy(mapped, &present, sizeof(present));
    m_places->surround->mark_dirty_range(0, k_surround_bytes);
}

void InfluenceList::set_lobes(std::span<const glm::vec4> lobes) const
{
    if (lobes.size() != k_lobe_entries) {
        MF_WARN(Journal::Component::Nexus, Journal::Context::Runtime,
            "InfluenceList: a lobe table of {} entries is ignored, {} are needed", lobes.size(), k_lobe_entries);
        return;
    }

    std::vector<glm::vec4> table;
    table.reserve(k_table_entries);
    for (const auto& entry : Kinesis::specular_albedo_table(Kinesis::k_albedo_table_size)) {
        table.emplace_back(entry.x, entry.y, 0.0F, 0.0F);
    }

    auto* mapped = static_cast<uint8_t*>(m_places->surround->get_mapped_ptr());
    const uint32_t present = 1;
    std::memcpy(mapped + k_table_offset, table.data(), table.size() * sizeof(glm::vec4));
    std::memcpy(mapped + k_lobe_offset, lobes.data(), lobes.size_bytes());
    std::memcpy(mapped + sizeof(uint32_t), &present, sizeof(present));
    m_places->surround->mark_dirty_range(0, k_surround_bytes);
}

void InfluenceList::clear_lobes() const
{
    const uint32_t present = 0;
    std::memcpy(static_cast<uint8_t*>(m_places->surround->get_mapped_ptr()) + sizeof(uint32_t), &present, sizeof(present));
    m_places->surround->mark_dirty_range(sizeof(uint32_t), sizeof(present));
}

void InfluenceList::clear_surround() const
{
    const uint32_t present = 0;
    std::memcpy(m_places->surround->get_mapped_ptr(), &present, sizeof(present));
    m_places->surround->mark_dirty_range(0, sizeof(present));
}

Influence<InfluenceBlock>::Target InfluenceList::target() const
{
    uint32_t place = 0;
    for (; place < m_places->capacity; ++place) {
        bool expected = false;
        if (m_places->taken[place].compare_exchange_strong(expected, true)) {
            break;
        }
    }
    if (place == m_places->capacity) {
        MF_WARN(Journal::Component::Nexus, Journal::Context::Runtime,
            "InfluenceList: all {} places are taken", m_places->capacity);
        return [](const InfluenceBlock&) { };
    }

    uint32_t used = m_places->used.load();
    while (used < place + 1 && !m_places->used.compare_exchange_weak(used, place + 1)) { }
    const uint32_t count = m_places->used.load();
    m_places->write(0, &count, sizeof(count));

    const std::shared_ptr<void> hold(nullptr, [places = m_places, place](void*) {
        InfluenceBlock empty;
        empty.intensity = 0.0F;
        empty.radius = 0.0F;
        places->put(place, empty);
        places->taken[place].store(false);
    });

    return [places = m_places, place, hold](const InfluenceBlock& block) {
        places->put(place, block);
    };
}

uint32_t InfluenceList::capacity() const noexcept { return m_places->capacity; }

const std::shared_ptr<Buffers::VKBuffer>& InfluenceList::buffer() const noexcept { return m_places->buffer; }

} // namespace MayaFlux::Nexus
