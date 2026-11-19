#pragma once

#include "BoundsText.hpp"

#include "MayaFlux/Kinesis/Spatial/Bounds.hpp"
#include "MayaFlux/Portal/Forma/Primitives/Geometry.hpp"
#include "MayaFlux/Transitive/Reflect/AttributeChecks.hpp"

namespace MayaFlux::Portal::Forma {

/**
 * @struct ElementHit
 * @brief The spatial fields a document gives an element, as Element carries them.
 *
 * bounds is Element::bounds_hint, the fast reject box, and contains is Element::contains, the
 * authoritative test. Either may be empty, and an empty one leaves what the form wrote.
 */
struct ElementHit {
    std::optional<Kinesis::AABB2D> bounds;
    std::function<bool(glm::vec2)> contains;
};

/** @brief Whether an element of this name is a region: circle, polygon, stroke or a combination of two. */
[[nodiscard]] inline bool is_region_element(std::string_view name)
{
    return name == "circle" || name == "polygon" || name == "stroke" || name == "union_bounds"
        || name == "intersect_bounds" || name == "subtract_bounds";
}

/** @brief Whether an element of this name sets spatial fields: bounds, or a region. */
[[nodiscard]] inline bool is_hit_child(std::string_view name)
{
    return name == "bounds" || is_region_element(name);
}

/**
 * @brief Write a document's spatial fields after a form's own.
 *
 * A form's geometry function writes bounds_hint and contains on every draw, so the document's
 * are applied after it, as Forma::geometry_with_containment does for a form's own.
 */
template <typename T>
void apply_hit(Geometry::Form<T>& form, const ElementHit& hit)
{
    if (!hit.bounds && !hit.contains)
        return;
    form.geometry = [inner = std::move(form.geometry), hit](T value, std::vector<uint8_t>& out, Element& element) {
        inner(std::move(value), out, element);
        if (hit.bounds)
            element.bounds_hint = hit.bounds;
        if (hit.contains)
            element.contains = hit.contains;
    };
}

namespace Detail {

    template <typename Source, typename Access>
    bool hit_fail(const Source& at, const std::string& message, std::string& error)
    {
        error = Reflect::located_message(Access::location(at),
            "<" + std::string(Access::name(at)) + "> " + message);
        return false;
    }

    template <typename Source, typename Access, typename T>
    bool read_hit_attribute(const Source& node, std::string_view name, T& value, std::string& error)
    {
        const auto text = Access::attribute(node, name);
        if (!text)
            return hit_fail<Source, Access>(node, "needs a " + std::string(name) + " attribute", error);
        Reflect::TextError reason = Reflect::TextError::Invalid;
        if (!Reflect::TextCodec<T>::decode(*text, value, reason)) {
            return hit_fail<Source, Access>(node,
                "has an invalid " + std::string(name) + " of " + std::string(*text), error);
        }
        return true;
    }

    template <typename Source, typename Access>
    bool only_hit_attributes(const Source& node, std::initializer_list<std::string_view> allowed, std::string& error)
    {
        return Access::for_each_attribute(node, [&](std::string_view name, std::string_view) {
            if (std::ranges::find(allowed, name) != allowed.end())
                return true;
            return hit_fail<Source, Access>(node, "has an unsupported attribute " + std::string(name), error);
        });
    }

    template <typename Source, typename Access>
    std::optional<std::function<bool(glm::vec2)>> read_region(const Source& node, std::string& error)
    {
        using Region = std::function<bool(glm::vec2)>;
        const std::string_view kind = Access::name(node);

        if (kind == "circle") {
            glm::vec2 center {};
            float radius {};
            if (!only_hit_attributes<Source, Access>(node, { "center", "radius" }, error)
                || !read_hit_attribute<Source, Access>(node, "center", center, error)
                || !read_hit_attribute<Source, Access>(node, "radius", radius, error)) {
                return std::nullopt;
            }
            if (!(radius > 0.F)) {
                hit_fail<Source, Access>(node, "needs a positive radius", error);
                return std::nullopt;
            }
            return Kinesis::circular_bounds(center, radius);
        }

        if (kind == "polygon" || kind == "stroke") {
            const bool stroke = kind == "stroke";
            std::vector<glm::vec2> points;
            float half_thickness {};
            const bool read = stroke
                ? only_hit_attributes<Source, Access>(node, { "points", "half_thickness" }, error)
                    && read_hit_attribute<Source, Access>(node, "half_thickness", half_thickness, error)
                : only_hit_attributes<Source, Access>(node, { "points" }, error);
            if (!read || !read_hit_attribute<Source, Access>(node, "points", points, error))
                return std::nullopt;
            if (points.size() < (stroke ? 2U : 3U)) {
                hit_fail<Source, Access>(node,
                    stroke ? "needs at least two x y pairs" : "needs at least three x y pairs", error);
                return std::nullopt;
            }
            if (stroke && !(half_thickness > 0.F)) {
                hit_fail<Source, Access>(node, "needs a positive half_thickness", error);
                return std::nullopt;
            }
            return stroke ? Kinesis::stroke_bounds(points, half_thickness) : Kinesis::polygon_bounds(points);
        }

        if (!only_hit_attributes<Source, Access>(node, {}, error))
            return std::nullopt;
        std::vector<Region> operands;
        const bool traversed = Access::for_each_child(node, [&](const Source& child) {
            if (!is_region_element(Access::name(child))) {
                return hit_fail<Source, Access>(child, "is not a region, and <" + std::string(kind)
                        + "> combines two regions", error);
            }
            auto region = read_region<Source, Access>(child, error);
            if (!region)
                return false;
            operands.push_back(std::move(*region));
            return true;
        });
        if (!traversed)
            return std::nullopt;
        if (operands.size() != 2) {
            hit_fail<Source, Access>(node, "needs exactly two regions", error);
            return std::nullopt;
        }
        if (kind == "union_bounds")
            return Kinesis::union_bounds(std::move(operands[0]), std::move(operands[1]));
        if (kind == "intersect_bounds")
            return Kinesis::intersect_bounds(std::move(operands[0]), std::move(operands[1]));
        return Kinesis::subtract_bounds(std::move(operands[0]), std::move(operands[1]));
    }

}

/**
 * @brief Read one spatial child of an element into @p hit.
 *
 * bounds is Element::with_bounds, with min and max. A region is Element::with_contains over the
 * Kinesis function of the same name: circle (center, radius), polygon (points), stroke (points,
 * half_thickness), and union_bounds, intersect_bounds and subtract_bounds, each of two
 * regions. An element has at most one bounds and one region.
 *
 * @param error Receives a located message when false is returned.
 */
template <typename Source, typename Access = Reflect::RecordAccess<Source>>
[[nodiscard]] bool read_hit_child(const Source& child, ElementHit& hit, std::string& error)
{
    if (Access::name(child) == "bounds") {
        if (hit.bounds)
            return Detail::hit_fail<Source, Access>(child, "may appear only once in an <element>", error);
        Kinesis::AABB2D box {};
        if (!Detail::only_hit_attributes<Source, Access>(child, { "min", "max" }, error)
            || !Detail::read_hit_attribute<Source, Access>(child, "min", box.min, error)
            || !Detail::read_hit_attribute<Source, Access>(child, "max", box.max, error)) {
            return false;
        }
        if (!(box.max.x > box.min.x) || !(box.max.y > box.min.y))
            return Detail::hit_fail<Source, Access>(child, "needs a max above its min", error);
        hit.bounds = box;
        return true;
    }

    if (hit.contains) {
        return Detail::hit_fail<Source, Access>(child,
            "is a second region, and an <element> has only one", error);
    }
    auto region = Detail::read_region<Source, Access>(child, error);
    if (!region)
        return false;
    hit.contains = std::move(*region);
    return true;
}

}
