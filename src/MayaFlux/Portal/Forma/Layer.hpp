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
    // =========================================================================
    // Slot
    // =========================================================================

    /**
     * @class Slot
     * @brief Handle returned by Layer::add, carrying the assigned element id.
     *
     * Provides chained post-registration mutations — relation, visibility,
     * z-order — without requiring the caller to keep the id in a separate
     * variable before acting on it.
     *
     * Slot holds a non-owning reference to its Layer. It must not outlive
     * the Layer. The recommended pattern is to extract the id immediately
     * after configuration is complete and store that instead:
     *
     * @code
     * const uint32_t id = layer.add(std::move(el))
     *     .relate_to(parent_id)
     *     .hidden()
     *     .id();
     * @endcode
     *
     * When the id is not needed, Slot converts implicitly to uint32_t so
     * existing callsites (el.element.id = layer.add(...)) compile unchanged.
     */
    class Slot {
    public:
        Slot(Layer& layer, uint32_t id) noexcept
            : m_layer(layer)
            , m_id(id)
        {
        }

        /**
         * @brief Implicit conversion to the element id.
         *
         * Allows Slot to substitute for uint32_t in existing call sites:
         * @code
         * mapped.element.id = layer.add(mapped.element);
         * @endcode
         */
        operator uint32_t() const noexcept { return m_id; }

        /**
         * @brief Explicit id accessor, for clarity in chained expressions.
         */
        [[nodiscard]] uint32_t id() const noexcept { return m_id; }

        // =====================================================================
        // Post-registration mutations — each returns Slot& for chaining
        // =====================================================================

        /**
         * @brief Record that this element belongs with @p primary_id.
         *
         * Visibility, z-order, and remove operations on @p primary_id cascade
         * to this element. Used to group body elements under a collapsible
         * header or to tie a label element to its controller.
         */
        Slot& relate_to(uint32_t primary_id)
        {
            m_layer.relate(primary_id, m_id);
            return *this;
        }

        /**
         * @brief Hide this element immediately after registration.
         *
         * Equivalent to layer.set_visible(id, false). Use when the element
         * starts hidden and should be revealed by a relation cascade or
         * an explicit state change.
         */
        Slot& hidden()
        {
            m_layer.set_visible(m_id, false);
            return *this;
        }

        /**
         * @brief Disable hit testing for this element immediately.
         *
         * Equivalent to layer.set_interactive(id, false).
         */
        Slot& non_interactive()
        {
            m_layer.set_interactive(m_id, false);
            return *this;
        }

        /**
         * @brief Move to the top of the hit-test order (drawn last, hit first).
         */
        Slot& to_front()
        {
            m_layer.bring_to_front(m_id);
            return *this;
        }

        /**
         * @brief Move to the bottom of the hit-test order (drawn first, hit last).
         */
        Slot& to_back()
        {
            m_layer.send_to_back(m_id);
            return *this;
        }

    private:
        Layer& m_layer;
        uint32_t m_id;
    };

    // =========================================================================
    // Lifecycle
    // =========================================================================

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
     *
     * Assigns a stable id and returns a Slot carrying that id. The Slot
     * supports chained post-registration mutations (relate_to, hidden,
     * non_interactive, to_front, to_back) and converts implicitly to
     * uint32_t, preserving all existing callsites.
     *
     * @return Slot for the registered element.
     */
    [[nodiscard]] Slot add(Element element);

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
        double px, double py, uint32_t win_w, uint32_t win_h) const;

    [[nodiscard]] std::vector<uint32_t> hit_test_all(
        double px, double py, uint32_t win_w, uint32_t win_h) const;

    // =========================================================================
    // Relations
    // =========================================================================

    /**
     * @brief Record that @p related_id belongs with @p primary_id.
     *
     * Cascaded lifecycle operations (remove, set_visible, bring_to_front,
     * send_to_back) on @p primary_id propagate to @p related_id.
     * Returns false if either id is not found.
     */
    bool relate(uint32_t primary_id, uint32_t related_id);

    /**
     * @brief Remove a specific relation between two elements.
     *        Does not remove either element from the layer.
     */
    bool unrelate(uint32_t primary_id, uint32_t related_id);

    /**
     * @brief Return the ids related to @p primary_id, or empty if none.
     */
    [[nodiscard]] std::vector<uint32_t> related_ids(uint32_t primary_id) const;

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

    std::unordered_map<uint32_t, std::vector<uint32_t>> m_relations;

    [[nodiscard]] static glm::vec2 to_ndc(
        double px, double py,
        uint32_t win_w, uint32_t win_h) noexcept;

    [[nodiscard]] static bool test_element(
        const Element& el, glm::vec2 ndc) noexcept;
};

} // namespace MayaFlux::Portal::Forma
