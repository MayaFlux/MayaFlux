#pragma once

#include "MayaFlux/Buffers/VKBuffer.hpp"
#include "MayaFlux/Kinesis/Spatial/Bounds.hpp"

namespace MayaFlux::Portal::Forma {

/**
 * @struct Element
 * @brief A bounded, renderable region on a window surface.
 *
 * An Element pairs a spatial description with a GPU buffer whose output
 * occupies that region. The spatial description is deliberately open:
 *
 * - @c bounds_hint: optional AABB2D in NDC space, used as a fast-reject
 *   pre-filter before @c contains is evaluated. Not required.
 *
 * - @c contains: authoritative point-in-element test. If absent and
 *   @c bounds_hint is set, @c bounds_hint::contains is used directly.
 *   If both are absent the element never hits. For a rectangle, set only
 *   @c bounds_hint. For a circle, arbitrary polygon, or curve region, set
 *   @c contains (and optionally @c bounds_hint as the circumscribed rect
 *   for performance).
 *
 * The buffer field accepts any VKBuffer subclass: a plain geometry buffer,
 * a TextureBuffer, a TextBuffer, or a custom buffer. Portal::Forma::Layer
 * owns elements and drives their lifecycle; callers work with element ids
 * returned by Layer::add.
 *
 * @note Elements do not participate in the graphics subsystem scheduler
 *       directly. The caller registers the underlying buffer with the
 *       BufferManager and attaches a RenderProcessor as usual. Element
 *       holds only the spatial and identity metadata that Layer and
 *       Context need.
 */
struct Element {
    /// @brief Stable id assigned by Layer::add. Never zero.
    uint32_t id { 0 };

    /// @brief Optional fast-reject bounds in NDC space.
    ///        When set, Layer evaluates this before @c contains.
    std::optional<Kinesis::AABB2D> bounds_hint;

    /// @brief Authoritative containment test in NDC space.
    ///        When absent, @c bounds_hint is used as the sole test.
    std::function<bool(glm::vec2)> contains;

    /// @brief GPU buffer whose rendered output occupies this region.
    std::shared_ptr<Buffers::VKBuffer> buffer;

    /// @brief When false, hit testing skips this element.
    bool interactive { true };

    /// @brief When false, the element is excluded from Layer iteration.
    bool visible { true };

    /// @brief Optional human-readable label for Lila introspection and
    ///        debug logging. Not used by Layer or Context internally.
    std::string name;
};

} // namespace MayaFlux::Portal::Forma
