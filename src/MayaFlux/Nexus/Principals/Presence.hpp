#pragma once

#include "Agent.hpp"

namespace MayaFlux::Nexus {

/**
 * @class Presence
 * @brief Agent that affects its surroundings by being there.
 *
 * An Agent already carries both a perception function and an influence
 * function plus a query radius. Presence makes both concrete around the act of
 * radiating into nearby entities:
 *
 *   Perception is sensing the neighborhood. The spatial query Fabric runs on
 *   each commit populates the perception context with the entities inside the
 *   query radius. Presence stores that result so the influence half can act on
 *   it, after the user perception callable runs.
 *
 *   Influence is radiation. For every neighbor found, Presence computes a
 *   falloff weight from the neighbor's squared distance against the falloff
 *   radius, then invokes a per-neighbor callable with the neighbor's id and its
 *   weight. The user callable decides what being near this Presence does to that
 *   neighbor: a parameter written, a force applied, a value pushed, anything.
 *   The Presence does not know what a neighbor is, only how near it is.
 *
 * The falloff radius is independent of the query radius. The query radius
 * decides who is a neighbor; the falloff radius decides how the weight decays
 * across that neighborhood. By default the falloff radius tracks the query
 * radius. The falloff curve is linear by default and replaceable.
 *
 * The base perception and influence callables still fire. Presence layers its
 * neighborhood sensing and radiation on top: the user perception callable runs,
 * then the result is captured; the user influence callable runs, then each
 * neighbor is radiated into.
 *
 * Wiring a Presence with every(1.0/60.0) drives the whole cycle each frame. A
 * Presence without a position senses an empty neighborhood and radiates into
 * nothing.
 */
class MAYAFLUX_API Presence : public Agent {
public:
    /**
     * @brief Falloff curve mapping normalized distance to weight.
     *
     * Receives distance / falloff_radius clamped to [0, 1] and returns the
     * weight at that distance. The default is linear: 1 at the center, 0 at the
     * radius.
     */
    using FalloffFn = std::function<float(float)>;

    /**
     * @brief Per-neighbor radiation callable.
     *
     * Invoked once per neighbor inside the falloff radius on each influence.
     *
     * @param id     Neighbor entity id from the spatial snapshot.
     * @param weight Falloff weight in [0, 1], 1 at the Presence center.
     */
    using RadiateFn = std::function<void(uint32_t id, float weight)>;

    /**
     * @brief Construct with perception, influence, and radiation functions.
     * @param query_radius Radius passed to the spatial index on each commit.
     * @param perception   User perception callable, fired before the neighborhood is captured.
     * @param influence    User influence callable, fired before radiation.
     * @param radiate      Invoked per neighbor inside the falloff radius.
     */
    Presence(float query_radius,
        PerceptionFn perception,
        InfluenceFn influence,
        RadiateFn radiate);

    /**
     * @brief Construct with named perception and influence functions and a radiation function.
     * @param query_radius        Radius passed to the spatial index on each commit.
     * @param perception_fn_name  Identifier for the perception function.
     * @param perception          User perception callable, fired before the neighborhood is captured.
     * @param influence_fn_name   Identifier for the influence function.
     * @param influence           User influence callable, fired before radiation.
     * @param radiate             Invoked per neighbor inside the falloff radius.
     */
    Presence(float query_radius,
        std::string perception_fn_name, PerceptionFn perception,
        std::string influence_fn_name, InfluenceFn influence,
        RadiateFn radiate);

    // =========================================================================
    // Falloff
    // =========================================================================

    /**
     * @brief Set the falloff radius, the distance over which the weight decays.
     *
     * Independent of the query radius. If never set, the falloff radius tracks
     * the query radius.
     *
     * @param r Falloff radius in world-space units.
     */
    void set_falloff_radius(float r) { m_falloff_radius = r; }

    /**
     * @brief Current falloff radius, or the query radius if none was set.
     */
    [[nodiscard]] float falloff_radius() const
    {
        return m_falloff_radius > 0.0F ? m_falloff_radius : query_radius();
    }

    /**
     * @brief Replace the falloff curve.
     * @param fn Maps normalized distance in [0, 1] to a weight.
     */
    void set_falloff(FalloffFn fn) { m_falloff = std::move(fn); }

    /**
     * @brief Replace the per-neighbor radiation callable.
     * @param fn Invoked per neighbor on each influence.
     */
    void set_radiate(RadiateFn fn) { m_radiate = std::move(fn); }

    /**
     * @brief Neighbors captured on the most recent perception, with weights.
     *
     * Each entry is the neighbor id paired with its falloff weight, valid after
     * the first commit-driven or wired invocation.
     */
    [[nodiscard]] const std::vector<std::pair<uint32_t, float>>& neighbors() const
    {
        return m_neighbors;
    }

    // =========================================================================
    // Perception and influence — Presence behaviour layered on the Agent path
    // =========================================================================

    void invoke_perception(const PerceptionContext& ctx) override;

    void invoke_influence(const InfluenceContext& ctx) const override;

private:
    float m_falloff_radius { 0.0F };
    FalloffFn m_falloff;
    RadiateFn m_radiate;
    mutable std::vector<std::pair<uint32_t, float>> m_neighbors;
};

} // namespace MayaFlux::Nexus
