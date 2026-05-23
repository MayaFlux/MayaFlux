#pragma once

#include "MayaFlux/Kinesis/Spatial/SpatialIndex.hpp"

namespace MayaFlux::Nexus {

/**
 * @class Expanse
 * @brief A defined region of space with a containment test and crossing actions.
 *
 * An Expanse is an extent, not a point. It holds a containment predicate that
 * answers whether a world position lies within it, and two actions fired when
 * an entity crosses its edge: one on entry, one on exit. The Fabric evaluates
 * the predicate against every indexed position on each commit and diffs
 * membership against the previous commit, firing the actions for the
 * difference. The Expanse does not query; the Fabric drives it, because only
 * the Fabric sees consecutive snapshots.
 *
 * The containment predicate is a plain function from position to bool. A fixed
 * box, a sphere, a half-space, or an extent driven by an evolving value are all
 * the same kind of thing: the predicate closes over whatever state shapes the
 * region, so an Expanse whose bounds move or breathe is expressed by a
 * predicate that reads that state, with no change to the mechanism.
 *
 * The entry and exit actions receive the crossing entity id. What crossing
 * means computationally is the action's decision: a tone gated, a parameter
 * written, another entity activated, a value pushed. The Expanse supplies the
 * spatial fact of crossing; the action supplies the consequence.
 *
 * The id is assigned by Fabric on registration and is stable for the Expanse's
 * lifetime.
 */
class MAYAFLUX_API Expanse {
public:
    using ContainsFn = std::function<bool(const glm::vec3&)>;
    using CrossingFn = std::function<void(uint32_t)>;

    /**
     * @brief Construct with a containment predicate and crossing actions.
     * @param contains Returns true when a world position lies within the Expanse.
     * @param on_enter Fired with the entity id when it enters. May be empty.
     * @param on_exit  Fired with the entity id when it leaves. May be empty.
     */
    Expanse(ContainsFn contains, CrossingFn on_enter, CrossingFn on_exit)
        : m_contains(std::move(contains))
        , m_on_enter(std::move(on_enter))
        , m_on_exit(std::move(on_exit))
    {
    }

    /**
     * @brief Construct with a named containment predicate and crossing actions.
     * @param fn_name  Identifier used for state encoding.
     * @param contains Returns true when a world position lies within the Expanse.
     * @param on_enter Fired with the entity id when it enters. May be empty.
     * @param on_exit  Fired with the entity id when it leaves. May be empty.
     */
    Expanse(std::string fn_name, ContainsFn contains, CrossingFn on_enter, CrossingFn on_exit)
        : m_fn_name(std::move(fn_name))
        , m_contains(std::move(contains))
        , m_on_enter(std::move(on_enter))
        , m_on_exit(std::move(on_exit))
    {
    }

    /** @brief Test whether a world position lies within the Expanse. */
    [[nodiscard]] bool contains(const glm::vec3& p) const
    {
        return m_contains && m_contains(p);
    }

    /** @brief Identifier assigned to the containment predicate, empty if anonymous. */
    [[nodiscard]] const std::string& fn_name() const { return m_fn_name; }

    /** @brief Set or replace the predicate identifier. */
    void set_fn_name(std::string name) { m_fn_name = std::move(name); }

    /** @brief Entity ids currently within the Expanse, as of the last commit. */
    [[nodiscard]] const std::unordered_set<uint32_t>& occupants() const { return m_occupants; }

    /** @brief Stable id assigned by Fabric on registration. */
    [[nodiscard]] uint32_t id() const { return m_id; }

private:
    std::string m_fn_name;
    ContainsFn m_contains;
    CrossingFn m_on_enter;
    CrossingFn m_on_exit;

    uint32_t m_id { 0 };
    std::unordered_set<uint32_t> m_occupants;

    friend class Fabric;
};

} // namespace MayaFlux::Nexus
