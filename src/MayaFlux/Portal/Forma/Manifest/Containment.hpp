#pragma once

#include "MayaFlux/Kinesis/Morphology.hpp"
#include "MayaFlux/Kinesis/Spatial/Bounds.hpp"
#include "MayaFlux/Kinesis/Spatial/HitRegion.hpp"
#include "MayaFlux/Portal/Forma/Element.hpp"
#include "MayaFlux/Portal/Forma/Primitives/Mapped.hpp"
#include "MayaFlux/Portal/Graphics/GraphicsUtils.hpp"
#include "MayaFlux/Transitive/Memory/FlatMap.hpp"

namespace MayaFlux::Portal::Forma {

/**
 * @brief The spatial fields of an Element as one value: Kinesis::HitRegion2D.
 *
 * Any factory may produce one, rectangular or not. bounds must circumscribe
 * contains, because Layer rejects outside bounds before asking contains.
 */
using Containment = Kinesis::HitRegion2D;

/**
 * @brief Write a hit region onto an element.
 * @note clip is written only when the region carries one, so a clip set by a
 * container such as Scrollable survives.
 */
inline void apply_containment(Element& element, const Containment& hit)
{
    element.bounds_hint = hit.bounds;
    element.contains = hit.contains;
    if (hit.clip)
        element.clip = hit.clip;
}

/**
 * @brief A geometry function that draws a value and claims a fixed containment.
 *
 * @tparam T State type the draw callable reads.
 * @param draw Writes the vertex bytes for a value.
 * @param region Written onto the element on every draw.
 */
template <typename T>
[[nodiscard]] GeometryFn<T> geometry_with_containment(
    std::function<void(const T&, std::vector<uint8_t>&)> draw, Containment region)
{
    return [draw = std::move(draw), region = std::move(region)](
               T value, std::vector<uint8_t>& out, Element& element) {
        draw(value, out);
        apply_containment(element, region);
    };
}

/** @brief The rectangle itself as bounds, with no containment test. */
[[nodiscard]] inline Containment rect_containment(Kinesis::AABB2D region)
{
    return Containment { .bounds = region };
}

/**
 * @brief Hit region derived from placed vertex positions.
 *
 * Triangle topologies hit inside any non-degenerate triangle. Other
 * topologies get the circumscribing box as bounds only. In both cases the box
 * of all positions is the bounds.
 *
 * @param positions Placed positions in NDC.
 * @param topology Primitive topology the positions are drawn with.
 */
[[nodiscard]] inline Containment containment_of(
    std::span<const glm::vec2> positions, Graphics::PrimitiveTopology topology)
{
    Containment hit;
    if (positions.empty())
        return hit;
    hit.bounds = Kinesis::aabb(positions);

    using Topology = Graphics::PrimitiveTopology;
    std::vector<glm::vec2> triangles;
    if (topology == Topology::TRIANGLE_LIST) {
        triangles.assign(positions.begin(), positions.end());
    } else if (topology == Topology::TRIANGLE_STRIP) {
        for (size_t i = 0; i + 2 < positions.size(); ++i)
            triangles.insert(triangles.end(), { positions[i], positions[i + 1], positions[i + 2] });
    } else if (topology == Topology::TRIANGLE_FAN) {
        for (size_t i = 1; i + 1 < positions.size(); ++i)
            triangles.insert(triangles.end(), { positions[0], positions[i], positions[i + 1] });
    }
    if (!triangles.empty())
        hit.contains = Kinesis::triangle_list_bounds(triangles);
    return hit;
}

/**
 * @struct ContainmentSource
 * @brief What a hit region provider may use: the placed geometry and, when the
 *        placement has one, its bounds.
 */
struct ContainmentSource {
    std::span<const glm::vec2> positions;
    Graphics::PrimitiveTopology topology { Graphics::PrimitiveTopology::TRIANGLE_LIST };
    std::optional<Kinesis::AABB2D> placement;
};

/**
 * @brief Produces a hit region from placed geometry, or nullopt with an error.
 *
 * Any callable qualifies: a circle, a polygon, a union, a host function. The
 * provider must return bounds that circumscribe its containment test.
 */
using ContainmentProvider = std::function<std::optional<Containment>(const ContainmentSource&, std::string&)>;

/**
 * @class ContainmentTable
 * @brief Hit region providers by name.
 *
 * A plain value like every table here: nothing is registered until its
 * builder adds it, and a host, a library or a user each build their own.
 * A factory that lets a document choose its hit region declares a name
 * among its described inputs and resolves it here.
 */
class ContainmentTable {
public:
    /** @brief Register a provider, replacing any of the same name. */
    void expose(std::string name, ContainmentProvider provider)
    {
        m_providers.insert_or_assign(name, std::move(provider));
    }

    /** @brief Add another table under a prefix, as FlatMap::merge_prefixed. */
    bool merge(const ContainmentTable& other, std::string_view prefix,
        std::string& error, bool replace = false)
    {
        return m_providers.merge_prefixed(other.m_providers, prefix, error, replace);
    }

    [[nodiscard]] bool contains(std::string_view name) const
    {
        return m_providers.find(name) != m_providers.end();
    }

    /** @brief Registered names in sorted order. */
    [[nodiscard]] std::vector<std::string> names() const
    {
        std::vector<std::string> result;
        for (const auto& entry : m_providers)
            result.push_back(entry.first);
        return result;
    }

    /** @brief Run the provider of this name on a source. */
    [[nodiscard]] std::optional<Containment> resolve(
        std::string_view name, const ContainmentSource& source, std::string& error) const
    {
        const auto provider = m_providers.find(name);
        if (provider == m_providers.end()) {
            error = "no registered hit region " + std::string(name);
            return std::nullopt;
        }
        return std::invoke(provider->second, source, error);
    }

    /**
     * @brief The named containment, or the fallback when the name is empty.
     *
     * A factory derives its own containment from its geometry and lets an
     * input name replace it, so the fallback is that derived value.
     *
     * @param error Receives the reason when nullopt is returned.
     */
    [[nodiscard]] std::optional<Containment> resolve_or(std::string_view name,
        const ContainmentSource& source, Containment fallback, std::string& error) const
    {
        if (name.empty())
            return fallback;
        return resolve(name, source, error);
    }

private:
    Memory::FlatMap<std::string, ContainmentProvider, std::less<>> m_providers;
};

}
