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

    // =========================================================================
    // Commit
    // =========================================================================

    /**
     * @brief Commit all woven Fabrics in insertion order.
     */
    void commit_all();

private:
    std::shared_ptr<Vruta::TaskScheduler> m_scheduler;
    std::shared_ptr<Vruta::EventManager> m_event_manager;

    std::vector<std::shared_ptr<Fabric>> m_fabrics;
    std::unordered_map<std::string, std::weak_ptr<Fabric>> m_named_fabrics;
};

} // namespace MayaFlux::Nexus
