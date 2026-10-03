#include "Mantle.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Kinesis/GeometryPrimitives.hpp"
#include "MayaFlux/Nexus/Pheme/Survey.hpp"

namespace MayaFlux::Nexus {

namespace {

    constexpr float k_epsilon = 1.0e-6F;

    float uniform_scale(const glm::vec3& from, const glm::vec3& to, bool largest)
    {
        const std::array<std::pair<float, float>, 3> axes { {
            { from.x, to.x },
            { from.y, to.y },
            { from.z, to.z },
        } };

        bool any = false;
        float scale = 1.0F;
        for (const auto& [source, target] : axes) {
            if (source <= k_epsilon) {
                continue;
            }
            const float ratio = target / source;
            scale = !any ? ratio : (largest ? std::max(scale, ratio) : std::min(scale, ratio));
            any = true;
        }
        return scale;
    }

    Kinesis::AABB3D resolve(
        const Kinesis::AABB3D& source,
        const Kinesis::AABB3D& target,
        Portal::Graphics::FitMode mode)
    {
        using Portal::Graphics::FitMode;

        const glm::vec3 from = source.extent();
        const glm::vec3 to = target.extent();

        switch (mode) {
        case FitMode::CONTAIN:
            return Kinesis::AABB3D::from_center(target.center(), from * uniform_scale(from, to, false) * 0.5F);
        case FitMode::COVER:
            return Kinesis::AABB3D::from_center(target.center(), from * uniform_scale(from, to, true) * 0.5F);
        case FitMode::CENTER:
            return Kinesis::AABB3D::from_center(target.center(), from * 0.5F);
        case FitMode::STRETCH:
        case FitMode::TILE:
        case FitMode::TILE_MIRRORED:
            break;
        }
        return target;
    }

}

Mantle::Mantle(const Kinesis::AABB3D& bounds,
    std::shared_ptr<Buffers::VKBuffer> buf,
    Portal::Graphics::FitMode fit,
    CrossingFn on_enter,
    CrossingFn on_exit)
    : Expanse(bounds, std::move(on_enter), std::move(on_exit))
    , m_look { .buf = std::move(buf), .transform = std::make_shared<glm::mat4>(1.0F) }
    , m_fit(fit)
{
    if (!m_look.buf) {
        MF_ERROR(Journal::Component::Nexus, Journal::Context::Init,
            "Mantle needs a buffer");
        return;
    }

    if (fit == Portal::Graphics::FitMode::TILE || fit == Portal::Graphics::FitMode::TILE_MIRRORED) {
        MF_WARN(Journal::Component::Nexus, Journal::Context::Init,
            "Mantle fits tiling modes as STRETCH: tiling has no meaning for geometry");
    }

    m_local = read_bounds(m_look.buf);
    if (!m_local) {
        MF_WARN(Journal::Component::Nexus, Journal::Context::Init,
            "Mantle leaves the buffer where it is: no geometry could be read from it");
        return;
    }

    refresh();
}

Mantle::~Mantle()
{
    release_attachment(m_look);
}

void Mantle::set_bounds(const Kinesis::AABB3D& bounds)
{
    Expanse::set_bounds(bounds);
    refresh();
}

void Mantle::refit()
{
    if (!m_look.buf) {
        return;
    }

    if (const auto local = read_bounds(m_look.buf)) {
        m_local = local;
        refresh();
    } else {
        MF_WARN(Journal::Component::Nexus, Journal::Context::Runtime,
            "Mantle::refit kept the previous geometry: no geometry could be read from the buffer");
    }
}

void Mantle::begin_evaluate()
{
    if (m_local) {
        place_attachment(m_look);
    }
}

void Mantle::refresh()
{
    if (!m_local || !bounds()) {
        return;
    }

    *m_look.transform = Kinesis::box_transform(*m_local, resolve(*m_local, *bounds(), m_fit));
    place_attachment(m_look);
}

} // namespace MayaFlux::Nexus
