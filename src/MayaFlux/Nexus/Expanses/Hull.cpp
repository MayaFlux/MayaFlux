#include "Hull.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Nexus/Pheme/Survey.hpp"

namespace MayaFlux::Nexus {

Hull::Hull(std::shared_ptr<Buffers::VKBuffer> buf,
    CrossingFn on_enter,
    CrossingFn on_exit,
    std::optional<uint32_t> collection)
    : Expanse(ContainsFn {}, std::move(on_enter), std::move(on_exit))
    , m_buf(std::move(buf))
    , m_collection(collection)
{
    if (!m_buf) {
        MF_ERROR(Journal::Component::Nexus, Journal::Context::Init,
            "Hull needs a buffer");
        return;
    }

    m_local = read_bounds(m_buf, m_collection);
    if (!m_local) {
        MF_WARN(Journal::Component::Nexus, Journal::Context::Init,
            "Hull contains nothing: no geometry could be read from the buffer");
    }
}

bool Hull::contains(const glm::vec3& p) const
{
    if (!m_buf || !m_local) {
        return false;
    }

    const glm::mat4 drawn = read_placement(m_buf);
    const float det = glm::determinant(glm::mat3(drawn));
    if (det == 0.0F || !std::isfinite(det)) {
        return false;
    }

    return m_local->contains(glm::vec3(glm::inverse(drawn) * glm::vec4(p, 1.0F)));
}

std::pair<glm::vec3, float> Hull::reach() const
{
    if (!m_buf || !m_local) {
        return Expanse::reach();
    }

    const glm::mat4 drawn = read_placement(m_buf);
    const glm::vec3 center = glm::vec3(drawn * glm::vec4(m_local->center(), 1.0F));

    float radius = 0.0F;
    for (int corner = 0; corner < 8; ++corner) {
        const glm::vec3 local {
            (corner & 1) != 0 ? m_local->max.x : m_local->min.x,
            (corner & 2) != 0 ? m_local->max.y : m_local->min.y,
            (corner & 4) != 0 ? m_local->max.z : m_local->min.z,
        };
        radius = std::max(radius, glm::length(glm::vec3(drawn * glm::vec4(local, 1.0F)) - center));
    }
    return { center, radius };
}

void Hull::refit()
{
    if (!m_buf) {
        return;
    }

    if (const auto bounds = read_bounds(m_buf, m_collection)) {
        m_local = bounds;
    } else {
        MF_WARN(Journal::Component::Nexus, Journal::Context::Runtime,
            "Hull::refit kept the previous bounds: no geometry could be read from the buffer");
    }
}

} // namespace MayaFlux::Nexus
