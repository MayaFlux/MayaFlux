#pragma once

#include "Agent.hpp"

#include "MayaFlux/Kinesis/Viewport/NavigationState.hpp"

namespace MayaFlux::Buffers {
class RenderProcessor;
}

namespace MayaFlux::Nexus {

/**
 * @class Locus
 * @brief Agent that perceives the world from a navigation state and influences
 *        how the things it touches are viewed.
 *
 * An Agent already carries both a perception function and an influence
 * function. Locus makes both concrete around a Kinesis::NavigationState:
 *
 *   Perception is observation. The navigation state is the lens. Held movement
 *   flags, yaw, and pitch advance the eye; the perception path integrates that
 *   motion against elapsed time and produces the current ViewTransform. The
 *   Locus sees the world from wherever its navigation state currently sits, and
 *   its inherited Agent position follows the eye so the spatial index and any
 *   PerceptionContext observe the same point.
 *
 *   Influence is re-viewing. A Locus holds a set of view targets: render
 *   processors whose view transform it drives. On each influence the current
 *   ViewTransform is pushed to every target. The same motion that determines
 *   what the Locus perceives determines how everything under its influence is
 *   seen. Multiple processors driven by one Locus share its viewpoint; multiple
 *   Loci over disjoint targets partition the scene into independently-viewed
 *   regions.
 *
 * The base perception and influence callables still fire. Locus layers its view
 * behaviour on top: the user callables run, then the view is integrated and
 * distributed. A user perception callable can read the freshly produced
 * transform through the Locus; a user influence callable can add domain effects
 * alongside the view push.
 *
 * The navigation state is plain data, mutated directly through nav() or via the
 * Kinesis free functions (apply_mouse_delta, apply_scroll, snap_ortho). Wiring a
 * Locus with every(1.0/60.0) drives the whole cycle each frame. Because the view
 * push writes a render processor UBO, wire the Locus so its influence fires on
 * the graphics thread when it drives view targets.
 */
class MAYAFLUX_API Locus : public Agent {
public:
    /**
     * @brief Construct from a NavigationConfig with perception and influence functions.
     * @param config       Initial navigation configuration. eye seeds the position.
     * @param query_radius Radius passed to the spatial index on each commit.
     * @param perception   User perception callable, fired before the view integrates.
     * @param influence    User influence callable, fired before the view is pushed.
     */
    Locus(const Kinesis::NavigationConfig& config,
        float query_radius,
        PerceptionFn perception,
        InfluenceFn influence);

    /**
     * @brief Construct from a NavigationConfig with named perception and influence functions.
     * @param config              Initial navigation configuration. eye seeds the position.
     * @param query_radius        Radius passed to the spatial index on each commit.
     * @param perception_fn_name  Identifier for the perception function.
     * @param perception          User perception callable, fired before the view integrates.
     * @param influence_fn_name   Identifier for the influence function.
     * @param influence           User influence callable, fired before the view is pushed.
     */
    Locus(const Kinesis::NavigationConfig& config,
        float query_radius,
        std::string perception_fn_name, PerceptionFn perception,
        std::string influence_fn_name, InfluenceFn influence);

    // =========================================================================
    // Navigation state
    // =========================================================================

    /**
     * @brief Mutable access to the navigation lens.
     *
     * Movement flags, yaw, pitch, and tuning fields are mutated through this
     * reference or via the Kinesis free functions. The eye advances on the next
     * perceive(); the inherited position follows.
     */
    [[nodiscard]] Kinesis::NavigationState& nav() { return m_nav; }

    /**
     * @brief Read-only access to the navigation lens.
     */
    [[nodiscard]] const Kinesis::NavigationState& nav() const { return m_nav; }

    /**
     * @brief The ViewTransform produced by the most recent perceive().
     *
     * Valid after the first commit-driven or wired invocation. Before that it
     * is the transform of the initial navigation state at unit aspect.
     */
    [[nodiscard]] const Kinesis::ViewTransform& current_view() const { return m_view; }

    /**
     * @brief Aspect ratio used when integrating the view.
     *
     * Defaults to 1. Set from the target window's framebuffer dimensions so the
     * projection matches what is on screen.
     */
    void set_aspect(float aspect) { m_aspect = aspect; }

    /** @brief Current aspect ratio. */
    [[nodiscard]] float aspect() const { return m_aspect; }

    // =========================================================================
    // View targets — the processors this Locus re-views
    // =========================================================================

    /**
     * @brief Add a render processor whose view transform this Locus drives.
     *
     * On each influence the processor receives the Locus's current
     * ViewTransform. Adding the same processor twice is a no-op. The processor
     * must outlive the Locus or be removed via remove_view_target().
     *
     * @param proc Render processor to drive. Ignored if null.
     */
    void add_view_target(std::shared_ptr<Buffers::RenderProcessor> proc);

    /**
     * @brief Stop driving a render processor's view transform.
     * @param proc Processor previously added via add_view_target().
     */
    void remove_view_target(const std::shared_ptr<Buffers::RenderProcessor>& proc);

    /**
     * @brief All render processors currently driven by this Locus.
     */
    [[nodiscard]] const std::vector<std::shared_ptr<Buffers::RenderProcessor>>& view_targets() const
    {
        return m_view_targets;
    }

    // =========================================================================
    // Perception and influence — Locus behaviour layered on the Agent path
    // =========================================================================

    /**
     * @brief Perceive: run the user perception callable, then integrate the view.
     *
     * Fires the inherited perception callable with @p ctx, advances the eye by
     * held movement flags scaled by elapsed time, mirrors the new eye into the
     * inherited position, and stores the resulting ViewTransform. The Locus
     * now sees the world from its updated state.
     *
     * @param ctx Perception context built by Fabric from the spatial snapshot.
     */
    void invoke_perception(const PerceptionContext& ctx) override;

    /**
     * @brief Influence: run the user influence callable, then push the current
     *        view to every view target.
     *
     * Fires the inherited influence callable with @p ctx and dispatches the
     * Locus's audio and render sinks, then sets the stored ViewTransform on
     * each render processor in view_targets(). Everything under this Locus's
     * influence is seen from its viewpoint.
     *
     * @param ctx Influence context built by Fabric from the Locus's state.
     */
    void invoke_influence(const InfluenceContext& ctx) const override;

private:
    Kinesis::NavigationState m_nav;
    Kinesis::ViewTransform m_view {};
    float m_aspect { 1.0F };
    std::vector<std::shared_ptr<Buffers::RenderProcessor>> m_view_targets;
};

} // namespace MayaFlux::Nexus
