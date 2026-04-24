#pragma once

#include "PerceptionContext.hpp"

namespace MayaFlux::Nexus {

/**
 * @class Sensor
 * @brief Object that reacts to nearby entities when committed.
 *
 * Constructed with only a perception function and a query radius. Position
 * is optional: call @c set_position before registering with @c Fabric if
 * spatial queries are required. A Sensor without a position receives an
 * empty @c spatial_results span on each commit.
 *
 * The id is assigned by @c Fabric::wire and is stable for the object's
 * lifetime.
 */
class Sensor {
public:
    using PerceptionFn = std::function<void(const PerceptionContext&)>;

    /**
     * @brief Construct with a query radius and a perception function.
     * @param query_radius Radius passed to the spatial index on each commit.
     *                     Ignored if no position has been set.
     * @param fn           Called on every commit with the current context.
     */
    explicit Sensor(float query_radius, PerceptionFn fn)
        : m_query_radius(query_radius)
        , m_fn(std::move(fn))
    {
    }

    /**
     * @brief Construct with a query radius and a named perception function.
     * @param query_radius Radius passed to the spatial index on each commit.
     * @param fn_name      Identifier used for state encoding.
     * @param fn           Called on every commit with the current context.
     */
    Sensor(float query_radius, std::string fn_name, PerceptionFn fn)
        : m_query_radius(query_radius)
        , m_fn_name(std::move(fn_name))
        , m_fn(std::move(fn))
    {
    }

    /** @brief Identifier assigned to the perception function, empty if anonymous. */
    [[nodiscard]] const std::string& fn_name() const { return m_fn_name; }

    /** @brief Set or replace the perception function's identifier. */
    void set_fn_name(std::string name) { m_fn_name = std::move(name); }

    /**
     * @brief Return the current position, if set.
     */
    [[nodiscard]] const std::optional<glm::vec3>& position() const { return m_position; }

    /**
     * @brief Set the position, enabling spatial queries for this object.
     * @param p World-space coordinates.
     */
    void set_position(const glm::vec3& p) { m_position = p; }

    /**
     * @brief Clear the position, disabling spatial queries for this object.
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
    void invoke(const PerceptionContext& ctx) const
    {
        if (m_fn) {
            m_fn(ctx);
        }
    }

private:
    std::optional<glm::vec3> m_position;
    float m_query_radius;
    std::string m_fn_name;
    PerceptionFn m_fn;
    uint32_t m_id { 0 };

    friend class Fabric;
};

} // namespace MayaFlux::Nexus
