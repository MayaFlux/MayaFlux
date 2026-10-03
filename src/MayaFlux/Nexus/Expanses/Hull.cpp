#include "Hull.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Nexus/Pheme/Survey.hpp"

namespace MayaFlux::Nexus {

Hull::Hull(std::shared_ptr<Buffers::VKBuffer> buf, CrossingFn on_enter, CrossingFn on_exit)
    : Expanse(ContainsFn {}, std::move(on_enter), std::move(on_exit))
    , m_buf(std::move(buf))
{
    if (!m_buf) {
        MF_ERROR(Journal::Component::Nexus, Journal::Context::Init,
            "Hull needs a buffer");
        return;
    }

    m_local = read_bounds(m_buf);
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

void Hull::refit()
{
    if (!m_buf) {
        return;
    }

    if (const auto bounds = read_bounds(m_buf)) {
        m_local = bounds;
    } else {
        MF_WARN(Journal::Component::Nexus, Journal::Context::Runtime,
            "Hull::refit kept the previous bounds: no geometry could be read from the buffer");
    }
}

} // namespace MayaFlux::Nexus
