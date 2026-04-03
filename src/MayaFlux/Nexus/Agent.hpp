#pragma once

#include "InfluenceContext.hpp"
#include "PerceptionContext.hpp"

namespace MayaFlux::Nexus {

/**
 * @class Agent
 * @brief Object that both perceives nearby entities and acts on MayaFlux objects.
 *
 * Constructed with only a query radius, a perception function, and an influence
 * function. Position is optional: call @c set_position before registering with
 * @c Fabric if spatial behaviour is required. Without a position the spatial
 * index is not consulted and @c spatial_results will be empty on each commit.
 *
 * On each commit the perception function is invoked first, then the influence
 * function. Both receive contexts populated from the same spatial snapshot.
 *
 * The id is assigned by @c Fabric::wire and is stable for the object's lifetime.
 */
class Agent {
public:
    using InfluenceFn = std::function<void(const InfluenceContext&)>;
    using PerceptionFn = std::function<void(const PerceptionContext&)>;

    /**
     * @brief Construct with query radius, perception function, and influence function.
     * @param query_radius Radius passed to the spatial index on each commit.
     *                     Ignored if no position has been set.
     * @param perception   Called first on every commit.
     * @param influence    Called second on every commit.
     */
    explicit Agent(float query_radius, PerceptionFn perception, InfluenceFn influence)
        : m_query_radius(query_radius)
        , m_perception_fn(std::move(perception))
        , m_influence_fn(std::move(influence))
    {
    }

    /**
     * @brief Return the current position, if set.
     */
    [[nodiscard]] const std::optional<glm::vec3>& position() const { return m_position; }

    /**
     * @brief Set the position, enabling spatial indexing and queries for this object.
     * @param p World-space coordinates.
     */
    void set_position(const glm::vec3& p) { m_position = p; }

    /**
     * @brief Clear the position, removing this object from spatial operations.
     */
    void clear_position() { m_position.reset(); }

    /**
     * @brief Return the query radius.
     */
    [[nodiscard]] float query_radius() const { return m_query_radius; }

    /**
     * @brief Set the query radius.
     * @param r New radius in world-space units.
     */
    void set_query_radius(float r) { m_query_radius = r; }

    /**
     * @brief Return the stable object id assigned by Fabric.
     */
    [[nodiscard]] uint32_t id() const { return m_id; }

    /**
     * @brief Invoke the perception function with the supplied context.
     * @param ctx Populated context for this commit.
     */
    void invoke_perception(const PerceptionContext& ctx) const
    {
        if (m_perception_fn) {
            m_perception_fn(ctx);
        }
    }

    /**
     * @brief Invoke the influence function with the supplied context.
     * @param ctx Populated context for this commit.
     */
    void invoke_influence(const InfluenceContext& ctx) const
    {
        if (m_influence_fn) {
            m_influence_fn(ctx);
        }
    }

private:
    std::optional<glm::vec3> m_position;
    float m_query_radius;
    PerceptionFn m_perception_fn;
    InfluenceFn m_influence_fn;
    uint32_t m_id { 0 };

    friend class Fabric;
};

} // namespace MayaFlux::Nexus
