#include "Hull.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Nexus/Pheme/Survey.hpp"

namespace MayaFlux::Nexus {

namespace {

    /** The way back from drawn to stored coordinates, if the placement has one. */
    std::optional<glm::mat4> inverse_placement(const glm::mat4& drawn)
    {
        const float det = glm::determinant(glm::mat3(drawn));
        if (det == 0.0F || !std::isfinite(det)) {
            return std::nullopt;
        }
        return glm::inverse(drawn);
    }

}

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

    if (const auto bounds = read_bounds(m_buf, m_collection)) {
        m_hull.local.observe(*bounds);
    } else {
        MF_WARN(Journal::Component::Nexus, Journal::Context::Init,
            "Hull contains nothing: no geometry could be read from the buffer");
    }
}

bool Hull::contains(const glm::vec3& p) const
{
    const auto box = quantity_value<Kinesis::AABB3D>(m_hull.local);
    if (!m_buf || !box) {
        return false;
    }

    const auto back = inverse_placement(read_placement(m_buf, m_collection));
    return back && box->contains(glm::vec3(*back * glm::vec4(p, 1.0F)));
}

std::optional<float> Hull::intersect(const Kinesis::Ray& ray) const
{
    const auto box = quantity_value<Kinesis::AABB3D>(m_hull.local);
    if (!m_buf || !box) {
        return std::nullopt;
    }

    const auto back = inverse_placement(read_placement(m_buf, m_collection));
    return back ? box->ray_entry(Kinesis::transform_ray(*back, ray)) : std::nullopt;
}

std::pair<glm::vec3, float> Hull::reach() const
{
    const auto box = quantity_value<Kinesis::AABB3D>(m_hull.local);
    if (!m_buf || !box) {
        return Expanse::reach();
    }

    const glm::mat4 drawn = read_placement(m_buf, m_collection);
    const glm::vec3 center = glm::vec3(drawn * glm::vec4(box->center(), 1.0F));

    float radius = 0.0F;
    for (int corner = 0; corner < 8; ++corner) {
        const glm::vec3 local {
            (corner & 1) != 0 ? box->max.x : box->min.x,
            (corner & 2) != 0 ? box->max.y : box->min.y,
            (corner & 4) != 0 ? box->max.z : box->min.z,
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
        set(HullControls { .local = *bounds });
    } else {
        MF_WARN(Journal::Component::Nexus, Journal::Context::Runtime,
            "Hull::refit kept the previous bounds: no geometry could be read from the buffer");
    }
}

} // namespace MayaFlux::Nexus
