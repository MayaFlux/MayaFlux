#pragma once

#include "MayaFlux/Kinesis/Spatial/Bounds.hpp"
#include "MayaFlux/Nexus/Pheme/Influence.hpp"

namespace MayaFlux::Nexus {

/**
 * @struct CrossingContext
 * @brief Data passed to a typed Expanse action for one entity.
 *
 * Entity ids are per Fabric, and one Expanse may be registered on several, so
 * an entity is only identified by @c fabric_id together with @c entity.
 */
struct CrossingContext {
    uint32_t fabric_id {}; ///< Id of the Fabric that evaluated the Expanse.
    uint32_t entity {}; ///< Id of the entity within that Fabric.
    std::optional<glm::vec3> position; ///< Position at this commit. Empty for an exit when the entity has left the Fabric.
};

/**
 * @class Expanse
 * @brief A defined region of space with a containment test and crossing actions.
 *
 * An Expanse is an extent, not a point. It answers whether a world position
 * lies within it, and fires actions when an entity crosses its edge: one on
 * entry, one on exit, and optionally one on every commit while inside. The
 * Fabric evaluates the Expanse against every indexed position on each commit
 * and diffs membership against the previous commit. The Expanse does not
 * query; the Fabric drives it, because only the Fabric sees consecutive
 * snapshots.
 *
 * The region is a box, a predicate, or both. A box is plain data: it moves or
 * resizes with set_bounds, which any influence can drive through bind_bounds.
 * A predicate is a function from position to bool that closes over whatever
 * state shapes the region. With both, the box is a cheap first test and both
 * must pass. Derived classes may replace contains().
 *
 * Crossing actions are either a plain function of the entity id, or any number
 * of typed actions added with add_entry, add_exit and add_inside. A typed
 * action builds a value from the CrossingContext and hands it to its targets,
 * so the targets in FieldTargets.hpp work here as they do for an Agent. What
 * crossing means computationally is the action's decision. Typed actions are
 * runtime objects and are not part of encoded state.
 *
 * The id is assigned by Fabric on registration.
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
     * @brief Construct with a containment predicate only, for an Expanse whose
     *        behaviour comes from typed actions.
     * @param contains Returns true when a world position lies within the Expanse.
     */
    explicit Expanse(ContainsFn contains)
        : m_contains(std::move(contains))
    {
    }

    /**
     * @brief Construct as a box.
     * @param bounds   World-space box.
     * @param on_enter Fired with the entity id when it enters. May be empty.
     * @param on_exit  Fired with the entity id when it leaves. May be empty.
     */
    explicit Expanse(const Kinesis::AABB3D& bounds, CrossingFn on_enter = {}, CrossingFn on_exit = {})
        : m_on_enter(std::move(on_enter))
        , m_on_exit(std::move(on_exit))
        , m_bounds(bounds)
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

    /**
     * @brief Construct with named containment predicate and named crossing actions.
     * @param fn_name          Identifier for the containment predicate.
     * @param on_enter_fn_name Identifier for the on_enter callable.
     * @param on_exit_fn_name  Identifier for the on_exit callable.
     * @param contains         Returns true when a world position lies within the Expanse.
     * @param on_enter         Fired with the entity id when it enters. May be empty.
     * @param on_exit          Fired with the entity id when it leaves. May be empty.
     */
    Expanse(std::string fn_name,
        std::string on_enter_fn_name,
        std::string on_exit_fn_name,
        ContainsFn contains,
        CrossingFn on_enter,
        CrossingFn on_exit)
        : m_fn_name(std::move(fn_name))
        , m_on_enter_fn_name(std::move(on_enter_fn_name))
        , m_on_exit_fn_name(std::move(on_exit_fn_name))
        , m_contains(std::move(contains))
        , m_on_enter(std::move(on_enter))
        , m_on_exit(std::move(on_exit))
    {
    }

    virtual ~Expanse() = default;

    /** @brief Test whether a world position lies within the Expanse. */
    [[nodiscard]] virtual bool contains(const glm::vec3& p) const
    {
        if (m_bounds && !m_bounds->contains(p)) {
            return false;
        }
        return m_contains ? m_contains(p) : m_bounds.has_value();
    }

    /** @brief Set or replace the box. */
    virtual void set_bounds(const Kinesis::AABB3D& bounds) { m_bounds = bounds; }

    /** @brief Remove the box, leaving the predicate. */
    void clear_bounds() { m_bounds.reset(); }

    /** @brief The box, if set. */
    [[nodiscard]] const std::optional<Kinesis::AABB3D>& bounds() const { return m_bounds; }

    /** @brief Identifier assigned to the containment predicate, empty if anonymous. */
    [[nodiscard]] const std::string& fn_name() const { return m_fn_name; }

    /** @brief Set or replace the predicate identifier. */
    void set_fn_name(std::string name) { m_fn_name = std::move(name); }

    /** @brief Identifier assigned to the on_enter callable, empty if anonymous. */
    [[nodiscard]] const std::string& on_enter_fn_name() const { return m_on_enter_fn_name; }

    /** @brief Set or replace the on_enter callable identifier. */
    void set_on_enter_fn_name(std::string name) { m_on_enter_fn_name = std::move(name); }

    /** @brief Identifier assigned to the on_exit callable, empty if anonymous. */
    [[nodiscard]] const std::string& on_exit_fn_name() const { return m_on_exit_fn_name; }

    /** @brief Set or replace the on_exit callable identifier. */
    void set_on_exit_fn_name(std::string name) { m_on_exit_fn_name = std::move(name); }

    /** @brief Stable id assigned by Fabric on registration. */
    [[nodiscard]] uint32_t id() const { return m_id; }

    /** @brief Entity ids inside this Expanse for @p fabric_id, or nullptr if that fabric has no occupants. */
    [[nodiscard]] const std::unordered_set<uint32_t>* occupants(uint32_t fabric_id) const
    {
        auto it = m_occupants_by_fabric.find(fabric_id);
        return it != m_occupants_by_fabric.end() ? &it->second : nullptr;
    }

    /** @brief All fabric ids that currently have at least one occupant inside this Expanse. */
    [[nodiscard]] std::vector<uint32_t> occupied_fabrics() const
    {
        std::vector<uint32_t> result;
        result.reserve(m_occupants_by_fabric.size());
        for (const auto& [fid, _] : m_occupants_by_fabric)
            result.push_back(fid);
        return result;
    }

    /**
     * @brief Add an action of any type that runs when an entity enters.
     * @tparam T Type the producer returns; whatever the targets take.
     * @param producer Builds the value from the crossing context.
     * @return The influence, to add its targets with Influence::add_target.
     */
    template <typename T>
    std::shared_ptr<Influence<T, CrossingContext>> add_entry(
        typename Influence<T, CrossingContext>::Producer producer)
    {
        return add_action<T>(m_entries, std::move(producer));
    }

    /**
     * @brief Add an action of any type that runs when an entity leaves.
     * @tparam T Type the producer returns; whatever the targets take.
     * @param producer Builds the value from the crossing context.
     * @return The influence, to add its targets with Influence::add_target.
     */
    template <typename T>
    std::shared_ptr<Influence<T, CrossingContext>> add_exit(
        typename Influence<T, CrossingContext>::Producer producer)
    {
        return add_action<T>(m_exits, std::move(producer));
    }

    /**
     * @brief Add an action of any type that runs on every commit for each entity inside.
     * @tparam T Type the producer returns; whatever the targets take.
     * @param producer Builds the value from the crossing context.
     * @return The influence, to add its targets with Influence::add_target.
     */
    template <typename T>
    std::shared_ptr<Influence<T, CrossingContext>> add_inside(
        typename Influence<T, CrossingContext>::Producer producer)
    {
        return add_action<T>(m_insides, std::move(producer));
    }

    /** @brief Stop an action added with add_entry(), add_exit() or add_inside(). */
    template <typename T>
    void remove_crossing(const std::shared_ptr<Influence<T, CrossingContext>>& action)
    {
        for (auto* list : { &m_entries, &m_exits, &m_insides }) {
            std::erase_if(*list, [&action](const auto& entry) { return entry.first == action; });
        }
    }

    /** @brief Stop every typed action. */
    void clear_crossings()
    {
        m_entries.clear();
        m_exits.clear();
        m_insides.clear();
    }

    /**
     * @brief Evaluate a spatial snapshot from one Fabric against this Expanse.
     *
     * Tests each position in @p snapshot with contains(), diffs the result
     * against the previous occupant set for @p fabric_id, fires the entry and
     * exit actions for the difference and the inside actions for everything
     * inside, and updates the stored set. Each Fabric maintains independent
     * occupant state.
     *
     * @param fabric_id  Stable id of the calling Fabric.
     * @param snapshot   All indexed positions from that Fabric's spatial index.
     */
    void evaluate(uint32_t fabric_id,
        std::span<const std::pair<uint32_t, glm::vec3>> snapshot);

protected:
    /** @brief Called at the start of every evaluate(), before any position is tested. */
    virtual void begin_evaluate() { }

private:
    using Action = std::function<void(const CrossingContext&)>;
    using Actions = std::vector<std::pair<std::shared_ptr<void>, Action>>;

    template <typename T>
    static std::shared_ptr<Influence<T, CrossingContext>> add_action(
        Actions& list,
        typename Influence<T, CrossingContext>::Producer producer)
    {
        auto action = std::make_shared<Influence<T, CrossingContext>>(std::move(producer));
        list.emplace_back(action,
            [action](const CrossingContext& ctx) { action->invoke(ctx); });
        return action;
    }

    std::string m_fn_name;
    std::string m_on_enter_fn_name;
    std::string m_on_exit_fn_name;
    ContainsFn m_contains;
    CrossingFn m_on_enter;
    CrossingFn m_on_exit;
    std::optional<Kinesis::AABB3D> m_bounds;

    Actions m_entries;
    Actions m_exits;
    Actions m_insides;

    uint32_t m_id { 0 };
    std::unordered_map<uint32_t, std::unordered_set<uint32_t>> m_occupants_by_fabric;

    friend class Fabric;
};

} // namespace MayaFlux::Nexus
