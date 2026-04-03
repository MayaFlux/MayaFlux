#pragma once

#include "InfluenceContext.hpp"

namespace MayaFlux::Nexus {

/**
 * @class Emitter
 * @brief Object that acts on existing MayaFlux objects when committed.
 *
 * Constructed with only an influence function. Position is optional: call
 * @c set_position before registering with @c Fabric if spatial indexing is
 * required. Entities without a position are committed normally but are not
 * inserted into the spatial index.
 *
 * The id is assigned by @c Fabric::wire and is stable for the object's
 * lifetime. It is zero until the object has been registered.
 */
class Emitter {
public:
    using InfluenceFn = std::function<void(const InfluenceContext&)>;

    /**
     * @brief Construct with an influence function.
     * @param fn Called on every commit with the current context.
     */
    explicit Emitter(InfluenceFn fn)
        : m_fn(std::move(fn))
    {
    }

    /**
     * @brief Return the current position, if set.
     */
    [[nodiscard]] const std::optional<glm::vec3>& position() const { return m_position; }

    /**
     * @brief Set the position, enabling spatial indexing for this object.
     * @param p World-space coordinates.
     */
    void set_position(const glm::vec3& p) { m_position = p; }

    /**
     * @brief Clear the position, removing this object from spatial queries.
     */
    void clear_position() { m_position.reset(); }

    /**
     * @brief Return the stable object id assigned by Fabric.
     */
    [[nodiscard]] uint32_t id() const { return m_id; }

    /**
     * @brief Invoke the influence function with the supplied context.
     * @param ctx Populated context for this commit.
     */
    void invoke(const InfluenceContext& ctx) const
    {
        if (m_fn) {
            m_fn(ctx);
        }
    }

private:
    std::optional<glm::vec3> m_position;
    InfluenceFn m_fn;
    uint32_t m_id { 0 };

    friend class Fabric;
};

} // namespace MayaFlux::Nexus
