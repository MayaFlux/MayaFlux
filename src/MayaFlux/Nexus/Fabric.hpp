#pragma once

#include "Agent.hpp"
#include "Emitter.hpp"
#include "Sensor.hpp"
#include "Wiring.hpp"

#include "MayaFlux/Kriya/Chain.hpp"

namespace MayaFlux::Vruta {
class TaskScheduler;
class EventManager;
}

namespace MayaFlux::Nexus {

/**
 * @class Fabric
 * @brief Orchestrates spatial indexing and scheduling for Nexus objects.
 *
 * Plain object, not a subsystem. Constructed with references to the engine's
 * scheduler and event manager. Owns a @c Kinesis::SpatialIndex3D populated
 * only for objects that have a position set.
 *
 * Objects are registered via @c wire(), which returns a @c Wiring builder.
 * The builder describes when and how the object's function is invoked.
 * All coroutines and events are owned by the scheduler and event manager
 * respectively. Fabric holds only the names needed to cancel them.
 *
 * @c commit() updates all positions in the index, publishes the snapshot,
 * and fires all registered objects. @c fire(id) fires a single object
 * against the current snapshot without republishing.
 *
 * Spatial queries read the last published snapshot and are safe from any
 * thread.
 */
class MAYAFLUX_API Fabric {
public:
    /**
     * @brief Construct with scheduler and event manager from the engine.
     * @param scheduler     Used to register coroutines created by Wiring.
     * @param event_manager Used to register event-driven coroutines.
     * @param cell_size     Grid cell edge length for the spatial index.
     */
    explicit Fabric(
        Vruta::TaskScheduler& scheduler,
        Vruta::EventManager& event_manager,
        float cell_size = 1.0F);

    ~Fabric();

    Fabric(const Fabric&) = delete;
    Fabric& operator=(const Fabric&) = delete;
    Fabric(Fabric&&) = delete;
    Fabric& operator=(Fabric&&) = delete;

    // =========================================================================
    // Registration
    // =========================================================================

    /**
     * @brief Begin wiring an Emitter into the Fabric.
     * @param emitter Object to register.
     * @return Wiring builder. Call @c Wiring::finalise() to apply.
     */
    [[nodiscard]] Wiring wire(std::shared_ptr<Emitter> emitter);

    /**
     * @brief Begin wiring a Sensor into the Fabric.
     * @param sensor Object to register.
     * @return Wiring builder. Call @c Wiring::finalise() to apply.
     */
    [[nodiscard]] Wiring wire(std::shared_ptr<Sensor> sensor);

    /**
     * @brief Begin wiring an Agent into the Fabric.
     * @param agent Object to register.
     * @return Wiring builder. Call @c Wiring::finalise() to apply.
     */
    [[nodiscard]] Wiring wire(std::shared_ptr<Agent> agent);

    /**
     * @brief Remove a registered object by id, cancelling any associated tasks.
     * @param id Stable id assigned at registration.
     */
    void remove(uint32_t id);

    // =========================================================================
    // Commit
    // =========================================================================

    /**
     * @brief Update all positions, publish the snapshot, fire all registered objects.
     *
     * For each object with a position, calls @c m_index->update() then
     * @c m_index->publish(). Then fires each object's function with a
     * context built from the new snapshot.
     */
    void commit();

    /**
     * @brief Fire a single object against the current snapshot without republishing.
     * @param id Id assigned at registration.
     */
    void fire(uint32_t id);

    // =========================================================================
    // Spatial queries
    // =========================================================================

    /**
     * @brief Find all objects within a radius of a point.
     * @param center Query origin.
     * @param radius Search radius in world-space units.
     * @return Unsorted results with squared distances.
     */
    [[nodiscard]] std::vector<Kinesis::QueryResult> within_radius(
        const glm::vec3& center, float radius) const;

    /**
     * @brief Find the k nearest objects to a point.
     * @param center Query origin.
     * @param k      Maximum number of results.
     * @return Results sorted by ascending squared distance.
     */
    [[nodiscard]] std::vector<Kinesis::QueryResult> k_nearest(
        const glm::vec3& center, uint32_t k) const;

private:
    friend class Wiring;

    using Member = std::variant<
        std::shared_ptr<Emitter>,
        std::shared_ptr<Sensor>,
        std::shared_ptr<Agent>>;

    struct Registration {
        Member member;
        std::string task_name;
        std::string event_name;
        std::string chain_name;
        bool commit_driven { false };
    };

    uint32_t assign_id(Member& m);
    void fire(const Registration& reg) const;

    Vruta::TaskScheduler& m_scheduler;
    Vruta::EventManager& m_event_manager;

    std::unique_ptr<Kinesis::SpatialIndex3D> m_index;
    std::unordered_map<uint32_t, Registration> m_registrations;
    uint32_t m_next_id { 1 };
};

} // namespace MayaFlux::Nexus
