#pragma once

#include "MayaFlux/Kinesis/Viewport/Frustum.hpp"
#include "MayaFlux/Transitive/Memory/OwnedCell.hpp"

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
 *   A Locus is one driver of a viewpoint cell. It makes one of its own and drives
 *   it, or joins one it is given, and its view targets draw from that viewpoint,
 *   each with the aspect of its own size. While it drives, perception writes the
 *   navigation state's viewpoint. While something else drives, such as a camera
 *   path, the Locus does not move: its position and orientation follow the
 *   viewpoint, and when the viewpoint is handed back navigation carries on from
 *   there, roll and frustum included.
 *
 * The base perception and influence callables still fire. Locus layers its view
 * behaviour on top: the user callables run, then the view is integrated and
 * distributed. A user perception callable can read the freshly produced
 * transform through the Locus; a user influence callable can add domain effects
 * alongside the view push.
 *
 * The navigation state is plain data, mutated directly through nav() or via the
 * Kinesis free functions (apply_mouse_delta, apply_scroll, snap_ortho). Wiring a
 * Locus with every(1.0/60.0) drives the whole cycle each frame.
 */
class MAYAFLUX_API Locus : public Agent {
public:
    ~Locus() override;

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
    [[deprecated("use set(LocusControls { .aspect = ... })")]] void set_aspect(float aspect)
    {
        m_locus.aspect.assign(Quantity(aspect));
    }

    /** @brief Current aspect ratio. */
    [[nodiscard]] float aspect() const
    {
        return quantity_value<float>(m_locus.aspect).value_or(1.0F);
    }

    /**
     * @brief Advance the eye by a fixed number of seconds on every perception.
     *
     * Without one the eye advances by the wall time since the last perception.
     * A fixed timestep makes the same input give the same eye path.
     */
    [[deprecated("use set(LocusControls { .timestep = ... })")]] void set_timestep(float dt)
    {
        m_locus.timestep.assign(Quantity(dt));
    }

    /** @brief Return to advancing by wall time. */
    [[deprecated("use set(LocusControls { .timestep = Quantity::unset() })")]] void clear_timestep()
    {
        m_locus.timestep.assign(Quantity::unset());
    }

    /** @brief The fixed timestep, if one is set. */
    [[nodiscard]] std::optional<float> timestep() const
    {
        return quantity_value<float>(m_locus.timestep);
    }

    // =========================================================================
    // The held viewpoint and what draws from it
    // =========================================================================

    /** @brief The viewpoint this Locus is a driver of. */
    [[nodiscard]] const std::shared_ptr<Memory::OwnedCell<Kinesis::Viewpoint>>& view() const { return m_held; }

    /** @brief This Locus's id on its viewpoint, for view()->hand_to(). */
    [[nodiscard]] uint32_t driver() const { return m_driver; }

    /**
     * @brief Join another viewpoint in place of the present one, without taking it over.
     *
     * The view targets move to it. Hand it to driver() for this Locus to drive it. Ignored if null.
     */
    void set_view(const std::shared_ptr<Memory::OwnedCell<Kinesis::Viewpoint>>& view);

    /**
     * @brief Take this Locus's viewpoint from a function until unfollow().
     *
     * The function is called whenever the view is read, so it must be safe on the threads
     * that read it. Blend two views with Kinesis::mix_viewpoints over their cells and a
     * weight, such as one a Presence reports. A function that reads this viewpoint again,
     * as two Locuses following each other would, answers where this one stood.
     * @return False for an empty function.
     */
    bool follow(std::function<Kinesis::Viewpoint()> source);

    /**
     * @brief follow() another viewpoint, such as another Locus's view().
     * @param other The viewpoint to follow. Held weakly.
     * @param offset Applied in the followed viewpoint's own frame.
     * @return False for a null cell or this Locus's own.
     */
    bool follow(const std::shared_ptr<Memory::OwnedCell<Kinesis::Viewpoint>>& other,
        const glm::mat4& offset = glm::mat4(1.0F));

    /** @brief Stop following, and carry on from where the view is if it was still being followed. */
    void unfollow();

    /** @brief Whether a follow() has not been ended. */
    [[nodiscard]] bool is_following() const noexcept { return m_follower != 0; }

    /** @brief The viewpoint followed and its offset, for a follow() of a cell. */
    [[nodiscard]] std::optional<std::pair<std::shared_ptr<Memory::OwnedCell<Kinesis::Viewpoint>>, glm::mat4>>
    following() const;

    /**
     * @brief Draw a render processor from this Locus's viewpoint.
     *
     * Adding the same processor twice is a no-op.
     *
     * @param proc Render processor. Ignored if null.
     */
    void add_view_target(std::shared_ptr<Buffers::RenderProcessor> proc);

    /**
     * @brief Stop drawing a render processor from the viewpoint.
     * @param proc Processor previously added via add_view_target().
     */
    void remove_view_target(const std::shared_ptr<Buffers::RenderProcessor>& proc);

    /**
     * @brief The render processors added as view targets.
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
     * @brief Influence: the inherited influence path, with its audio and render sinks and
     *        attachment. The view targets need nothing pushed, they draw from the viewpoint.
     *
     * @param ctx Influence context built by Fabric from the Locus's state.
     */
    void invoke_influence(const InfluenceContext& ctx) const override;

private:
    LocusControls& m_locus { open_locus() };
    Kinesis::NavigationState m_nav;
    Kinesis::ViewTransform m_view {};
    std::vector<std::shared_ptr<Buffers::RenderProcessor>> m_view_targets;
    std::shared_ptr<Memory::OwnedCell<Kinesis::Viewpoint>> m_held;
    uint32_t m_driver {};
    uint32_t m_follower {};
    std::weak_ptr<Memory::OwnedCell<Kinesis::Viewpoint>> m_followed;
    glm::mat4 m_follow_offset { 1.0F };

    [[nodiscard]] LocusControls& open_locus();
    void open_view();
    void join_view(const std::shared_ptr<Memory::OwnedCell<Kinesis::Viewpoint>>& view);
};

} // namespace MayaFlux::Nexus
