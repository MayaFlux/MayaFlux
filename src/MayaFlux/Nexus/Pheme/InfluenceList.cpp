#include "InfluenceList.hpp"

#include "MayaFlux/Buffers/Shaders/RenderProcessor.hpp"
#include "MayaFlux/Buffers/VKBuffer.hpp"
#include "MayaFlux/Registry/BackendRegistry.hpp"
#include "MayaFlux/Registry/Service/BufferService.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Nexus {

namespace {

    constexpr size_t k_header_bytes = 4 * sizeof(uint32_t);

}

struct InfluenceList::Places {
    std::shared_ptr<Buffers::VKBuffer> buffer;
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

    const size_t bytes = k_header_bytes + m_places->capacity * sizeof(InfluenceBlock);
    m_places->buffer = std::make_shared<Buffers::VKBuffer>(
        bytes, Buffers::VKBuffer::Usage::HOST_STORAGE, Kakshya::DataModality::UNKNOWN);

    const auto service = Registry::BackendRegistry::instance().get_service<Registry::Service::BufferService>();
    if (!service) {
        error<std::runtime_error>(Journal::Component::Nexus, Journal::Context::Init, std::source_location::current(),
            "InfluenceList needs a buffer service");
    }
    service->initialize_buffer(m_places->buffer);

    const std::vector<uint8_t> zeros(bytes, 0);
    m_places->write(0, zeros.data(), zeros.size());
}

void InfluenceList::bind(const std::shared_ptr<Buffers::RenderProcessor>& renderer) const
{
    if (renderer) {
        renderer->bind_buffer("influences", m_places->buffer);
    }
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
