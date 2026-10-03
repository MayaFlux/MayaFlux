#pragma once

#include "Fabric.hpp"

namespace MayaFlux::Vruta {
class TaskScheduler;
class EventManager;
}

namespace MayaFlux::Nexus {

/**
 * @class Tapestry
 * @brief Owner of one or more Fabrics and the shared state they rely on.
 *
 * A Tapestry holds shared_ptrs to the TaskScheduler and EventManager each
 * Fabric needs, and serves as the authoring-surface layer above raw Fabric
 * construction. Fabrics woven through a Tapestry can be looked up by name
 * for state encoding, cross-fabric queries, and coordinated commits.
 *
 * Tapestry holds owning references to the scheduler and event manager so
 * that every Fabric it create_fabrics is guaranteed a valid backing for the
 * Tapestry's lifetime.
 */
class MAYAFLUX_API Tapestry {
public:
    /**
     * @brief Construct with shared scheduler and event manager.
     */
    Tapestry(
        std::shared_ptr<Vruta::TaskScheduler> scheduler,
        std::shared_ptr<Vruta::EventManager> event_manager);

    ~Tapestry();

    Tapestry(const Tapestry&) = delete;
    Tapestry& operator=(const Tapestry&) = delete;
    Tapestry(Tapestry&&) = delete;
    Tapestry& operator=(Tapestry&&) = delete;

    // =========================================================================
    // Fabric lifecycle
    // =========================================================================

    /**
     * @brief create_fabric a new unnamed Fabric into the Tapestry.
     * @param cell_size Grid cell edge length for the spatial index.
     */
    [[nodiscard]] std::shared_ptr<Fabric> create_fabric(float cell_size = 1.0F);

    /**
     * @brief create_fabric a new named Fabric into the Tapestry.
     * @param name      Identifier for lookup; must not collide with existing names.
     * @param cell_size Grid cell edge length for the spatial index.
     */
    [[nodiscard]] std::shared_ptr<Fabric> create_fabric(std::string name, float cell_size = 1.0F);

    /**
     * @brief Remove a Fabric by pointer. No-op if not owned by this Tapestry.
     */
    void remove_fabric(const std::shared_ptr<Fabric>& fabric);

    /**
     * @brief Remove a Fabric by name. No-op if name is not registered.
     */
    void remove_fabric(std::string_view name);

    /**
     * @brief Look up a named Fabric.
     * @return The Fabric, or nullptr if name is not registered.
     */
    [[nodiscard]] std::shared_ptr<Fabric> get_fabric(std::string_view name) const;

    /**
     * @brief All woven Fabrics, named and unnamed.
     */
    [[nodiscard]] const std::vector<std::shared_ptr<Fabric>>& all_fabrics() const;

    /**
     * @brief Read-only view of all Expanses owned by this Tapestry.
     */
    [[nodiscard]] const std::unordered_map<std::string, std::shared_ptr<Expanse>>&
    all_expanses() const
    {
        return m_expanses;
    }

    // =========================================================================
    // Expanse
    // =========================================================================

    /**
     * @brief Create and register a named Expanse.
     *
     * The name also serves as the containment function's registry name for state
     * encoding, unless set_fn_name is called. A name already in use warns and
     * returns the existing Expanse, as create_fabric does.
     *
     * @return Shared pointer to the Expanse, for passing to Fabric::add_expanse.
     */
    [[nodiscard]] std::shared_ptr<Expanse> create_expanse(
        std::string name,
        Expanse::ContainsFn contains,
        Expanse::CrossingFn on_enter,
        Expanse::CrossingFn on_exit);

    /**
     * @brief Create and register a named Expanse from a containment predicate alone.
     * @return Shared pointer to the new Expanse, for passing to Fabric::add_expanse.
     */
    [[nodiscard]] std::shared_ptr<Expanse> create_expanse(
        std::string name,
        Expanse::ContainsFn contains);

    /**
     * @brief Create and register a named Expanse that is a box.
     * @return Shared pointer to the new Expanse, for passing to Fabric::add_expanse.
     */
    [[nodiscard]] std::shared_ptr<Expanse> create_expanse(
        std::string name,
        const Kinesis::AABB3D& bounds);

    /**
     * @brief Create and register a named Expanse of a derived type.
     * @tparam T Expanse subclass, for example Hull.
     * @param args Forwarded to T's constructor.
     * @return Shared pointer to the Expanse, for passing to Fabric::add_expanse. A name
     *         already in use warns and returns the existing Expanse, or null if that
     *         one is not a T.
     */
    template <typename T, typename... Args>
    [[nodiscard]] std::shared_ptr<T> create_expanse(std::string name, Args&&... args)
    {
        static_assert(std::is_base_of_v<Expanse, T>, "T must derive from Expanse");
        return std::dynamic_pointer_cast<T>(
            claim(std::move(name), std::make_shared<T>(std::forward<Args>(args)...)));
    }

    /**
     * @brief Look up a named Expanse.
     * @return The Expanse, or nullptr if name is not registered.
     */
    [[nodiscard]] std::shared_ptr<Expanse> get_expanse(std::string_view name) const;

    /**
     * @brief Remove Tapestry's owning reference to a named Expanse.
     *
     * Fabrics holding the shared_ptr keep it alive and functional, so nothing
     * fires here; Fabric::remove_expanse fires the exits for its own occupants.
     */
    void remove_expanse(std::string_view name);

    // =========================================================================
    // Commit
    // =========================================================================

    /**
     * @brief Commit all woven Fabrics in insertion order.
     */
    void commit_all();

private:
    std::shared_ptr<Expanse> claim(std::string name, std::shared_ptr<Expanse> expanse);

    std::shared_ptr<Vruta::TaskScheduler> m_scheduler;
    std::shared_ptr<Vruta::EventManager> m_event_manager;

    std::vector<std::shared_ptr<Fabric>> m_fabrics;
    std::unordered_map<std::string, std::weak_ptr<Fabric>> m_named_fabrics;
    std::unordered_map<std::string, std::shared_ptr<Expanse>> m_expanses;

    uint32_t m_next_id { 1 };
};

} // namespace MayaFlux::Nexus
