#pragma once

#include "MayaFlux/Kinesis/Spatial/Bounds.hpp"
#include "MayaFlux/Nexus/Controls.hpp"
#include "MayaFlux/Nexus/Pheme/Influence.hpp"
#include "MayaFlux/Nexus/Pheme/Perception.hpp"

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
 * The region is a box, a predicate, or both. The box is the `bounds` control of
 * ExpanseControls: `set({ .bounds = box })` moves or resizes it from any thread, and
 * any influence can drive it through bind_bounds. A predicate is a function from
 * position to bool that closes over whatever state shapes the region. With both,
 * the box is a cheap first test and both must pass. Derived classes may replace
 * contains(). `extend<C>()` adds a struct of the user's own controls, as the
 * entities have.
 *
 * Crossing actions are either a plain function of the entity id, or any number
 * of typed actions added with add_entry, add_exit and add_inside. A typed
 * action builds a value from the CrossingContext and hands it to its targets,
 * so the targets in FieldTargets.hpp work here as they do for an Agent. What
 * crossing means computationally is the action's decision.
 *
 * An Expanse also perceives its own contents, as a Sensor perceives its
 * neighbourhood. add_perception sources receive a PerceptionContext centred on
 * the region, with the occupants as the spatial results, once per evaluate and
 * before any crossing action. Typed actions and perceptions are runtime objects
 * and are not part of encoded state.
 *
 * The id is assigned at construction and is the same on every Fabric the
 * Expanse is registered on.
 */
class MAYAFLUX_API Expanse {
public:
    using Controls = ExpanseControls;
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
    {
        m_controls.bounds.observe(bounds);
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
        const auto box = bounds();
        if (box && !box->contains(p)) {
            return false;
        }
        return m_contains ? m_contains(p) : box.has_value();
    }

    // =========================================================================
    // Controls
    // =========================================================================

    /**
     * @brief Apply the members of @p incoming that hold a value.
     * @tparam C ExpanseControls, the struct of a derived class or one added with extend().
     *
     * Members left empty are untouched, and `Quantity::unset()` empties a member.
     * Safe from any thread.
     */
    template <typename C = ExpanseControls>
    void set(const C& incoming)
    {
        m_store.merge(incoming);
    }

    /** @brief A copy of the controls of type C as they are now. */
    template <typename C = ExpanseControls>
    [[nodiscard]] C get() const
    {
        return m_store.snapshot<C>();
    }

    /** @brief Add a struct of the user's own controls beside ExpanseControls and return it. */
    template <typename C>
    [[nodiscard]] C& extend()
    {
        return m_store.extend<C>();
    }

    /** @brief The live controls, read without a copy. */
    [[nodiscard]] const ExpanseControls& controls() const noexcept { return m_controls; }

    /** @brief Every control by name, for tools that list or set them. */
    [[nodiscard]] const QuantityStore& store() const noexcept { return m_store; }

    /** @brief The box, if set. */
    [[nodiscard]] std::optional<Kinesis::AABB3D> bounds() const
    {
        return quantity_value<Kinesis::AABB3D>(m_controls.bounds);
    }

    /** @brief Set or replace the box. */
    [[deprecated("use set({ .bounds = ... })")]] void set_bounds(const Kinesis::AABB3D& bounds)
    {
        m_controls.bounds.assign(Quantity(bounds));
    }

    /** @brief Remove the box, leaving the predicate. */
    [[deprecated("use set({ .bounds = Quantity::unset() })")]] void clear_bounds()
    {
        m_controls.bounds.assign(Quantity::unset());
    }

    /** @brief The containment predicate, empty for a box-only Expanse. */
    [[nodiscard]] const ContainsFn& contains_fn() const { return m_contains; }

    /** @brief The on_enter callable, empty if none. */
    [[nodiscard]] const CrossingFn& on_enter_fn() const { return m_on_enter; }

    /** @brief The on_exit callable, empty if none. */
    [[nodiscard]] const CrossingFn& on_exit_fn() const { return m_on_exit; }

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

    /** @brief Id assigned at construction. Unique across Expanses and stable for its lifetime. */
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
     * @brief Add a perception of any type over the entities inside, with as many targets as are wanted.
     * @tparam T Type the source returns; whatever the targets take.
     * @param source Reads the value from a PerceptionContext whose position and radius
     *               are the region's centre and reach, and whose spatial results are
     *               the occupants: entity id and squared distance from the centre.
     * @return The perception, to add its targets with Perception::add_target.
     *
     * Runs once per evaluate for each Fabric, before the crossing actions.
     */
    template <typename T>
    std::shared_ptr<Perception<T>> add_perception(typename Perception<T>::Source source)
    {
        auto perception = std::make_shared<Perception<T>>(std::move(source));
        m_perceptions.emplace_back(perception,
            [perception](const PerceptionContext& ctx) { perception->invoke(ctx); });
        return perception;
    }

    /** @brief Stop a perception added with add_perception(). */
    template <typename T>
    void remove_perception(const std::shared_ptr<Perception<T>>& perception)
    {
        std::erase_if(m_perceptions,
            [&perception](const auto& entry) { return entry.first == perception; });
    }

    /** @brief Stop every perception added with add_perception(). */
    void clear_perceptions() { m_perceptions.clear(); }

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
     * @param snapshot   Entity id and position of every positioned entity in that Fabric.
     */
    void evaluate(uint32_t fabric_id,
        std::span<const std::pair<uint32_t, glm::vec3>> snapshot);

    /**
     * @brief Fire the exit actions for everything inside for @p fabric_id and forget it.
     *
     * Called by a Fabric when it drops this Expanse, so state an entry action
     * changed is not left in the entered state.
     *
     * @param fabric_id Stable id of the Fabric dropping the Expanse.
     */
    void evict(uint32_t fabric_id);

private:
    QuantityStore m_store;

protected:
    /** @brief Called at the start of every evaluate(), before any position is tested. */
    virtual void begin_evaluate() { }

    /**
     * @brief Where the region is centred and how far it reaches, for perceptions.
     *
     * The centre and the half diagonal of the box if there is one, else the
     * origin and zero. A derived Expanse whose region is not the box overrides it.
     */
    [[nodiscard]] virtual std::pair<glm::vec3, float> reach() const
    {
        if (const auto box = bounds()) {
            return { box->center(), glm::length(box->half_extent()) };
        }
        return { glm::vec3(0.0F), 0.0F };
    }

    /** @brief The live controls, for a derived class to consume what changed. */
    ExpanseControls& m_controls { open(m_store) };

private:
    [[nodiscard]] static ExpanseControls& open(QuantityStore& store);

    using Action = std::function<void(const CrossingContext&)>;
    using Actions = std::vector<std::pair<std::shared_ptr<void>, Action>>;
    using PerceptionFn = std::function<void(const PerceptionContext&)>;

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

    Actions m_entries;
    Actions m_exits;
    Actions m_insides;
    std::vector<std::pair<std::shared_ptr<void>, PerceptionFn>> m_perceptions;

    static uint32_t allocate_id();

    uint32_t m_id { allocate_id() };
    std::unordered_map<uint32_t, std::unordered_set<uint32_t>> m_occupants_by_fabric;
};

} // namespace MayaFlux::Nexus
