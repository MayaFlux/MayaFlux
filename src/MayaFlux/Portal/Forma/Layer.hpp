#pragma once

#include "Element.hpp"

namespace MayaFlux::Portal::Forma {

/**
 * @class Layer
 * @brief Flat registry of Elements on a surface.
 *
 * Owns a collection of Elements and provides spatial queries against them
 * in NDC space. Does not drive rendering, scheduling, or event wiring.
 *
 * Elements are stored and traversed back-to-front so later-added elements
 * are considered on top during hit testing, matching typical draw order.
 *
 * Pixel-space query overloads accept window dimensions and convert to NDC
 * internally using the same convention as Kinesis::to_ndc: top-left origin,
 * +Y down in pixel space, +Y up in NDC.
 */
class MAYAFLUX_API Layer {
public:
    Layer() = default;
    ~Layer() = default;

    Layer(const Layer&) = delete;
    Layer& operator=(const Layer&) = delete;
    Layer(Layer&&) = default;
    Layer& operator=(Layer&&) = default;

    // =========================================================================
    // Element registration
    // =========================================================================

    /**
     * @brief Add an element to the layer.
     * @return The stable id assigned to the element. Never zero.
     */
    uint32_t add(Element element);

    /**
     * @brief Remove an element by id.
     * @return True if found and removed.
     */
    bool remove(uint32_t id);

    /**
     * @brief Remove all elements.
     */
    void clear();

    // =========================================================================
    // Element mutation
    // =========================================================================

    /**
     * @brief Replace the bounds_hint on an existing element.
     * @return True if found.
     */
    bool set_bounds(uint32_t id, Kinesis::AABB2D bounds);

    /**
     * @brief Replace the contains callable on an existing element.
     * @return True if found.
     */
    bool set_contains(uint32_t id, std::function<bool(glm::vec2)> fn);

    bool set_interactive(uint32_t id, bool interactive);
    bool set_visible(uint32_t id, bool visible);

    /**
     * @brief Move element to the top of the hit-test order (drawn last).
     */
    bool bring_to_front(uint32_t id);

    /**
     * @brief Move element to the bottom of the hit-test order (drawn first).
     */
    bool send_to_back(uint32_t id);

    // =========================================================================
    // Spatial queries - NDC space
    // =========================================================================

    /**
     * @brief Return the topmost interactive, visible element containing @p ndc.
     *
     * Traverses back-to-front. An element passes if:
     *   1. bounds_hint is absent OR bounds_hint.contains(ndc) is true.
     *   2. contains is absent OR contains(ndc) is true.
     * An element with neither set never hits.
     *
     * @param ndc Point in NDC space (x, y in [-1, +1]).
     * @return Element id, or nullopt if no element hit.
     */
    [[nodiscard]] std::optional<uint32_t> hit_test(glm::vec2 ndc) const;

    /**
     * @brief Return all interactive, visible elements containing @p ndc,
     *        ordered back-to-front.
     */
    [[nodiscard]] std::vector<uint32_t> hit_test_all(glm::vec2 ndc) const;

    // =========================================================================
    // Spatial queries - pixel space
    // =========================================================================

    /**
     * @brief hit_test with pixel-space input, converted to NDC internally.
     * @param px       Cursor x in pixels (top-left origin).
     * @param py       Cursor y in pixels (top-left origin).
     * @param win_w    Window width in pixels.
     * @param win_h    Window height in pixels.
     */
    [[nodiscard]] std::optional<uint32_t> hit_test(
        double px, double py,
        uint32_t win_w, uint32_t win_h) const;

    [[nodiscard]] std::vector<uint32_t> hit_test_all(
        double px, double py,
        uint32_t win_w, uint32_t win_h) const;

    // =========================================================================
    // Introspection
    // =========================================================================

    [[nodiscard]] const Element* get(uint32_t id) const;
    [[nodiscard]] Element* get(uint32_t id);

    [[nodiscard]] const std::vector<Element>& elements() const { return m_elements; }
    [[nodiscard]] size_t size() const { return m_elements.size(); }
    [[nodiscard]] bool empty() const { return m_elements.empty(); }

private:
    std::vector<Element> m_elements;
    uint32_t m_next_id { 1 };

    [[nodiscard]] static glm::vec2 to_ndc(
        double px, double py,
        uint32_t win_w, uint32_t win_h) noexcept;

    [[nodiscard]] static bool test_element(
        const Element& el, glm::vec2 ndc) noexcept;
};

} // namespace MayaFlux::Portal::Forma
