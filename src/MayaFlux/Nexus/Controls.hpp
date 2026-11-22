#pragma once

#include "MayaFlux/Nexus/Quantity.hpp"
#include "MayaFlux/Transitive/Facet/ControlValueStore.hpp"
#include "MayaFlux/Transitive/Reflect/Describe.hpp"

namespace MayaFlux::Nexus {

/** @brief The store every Nexus principal holds its controls in. */
using QuantityStore = ControlValueStore<Quantity>;

/**
 * @brief The value a quantity holds, or nothing when it is empty.
 * @tparam T The type it was set with.
 */
template <typename T>
[[nodiscard]] std::optional<T> quantity_value(const Quantity& slot)
{
    return slot.has_value() ? std::optional<T> { slot.value<T>() } : std::nullopt;
}

/**
 * @struct EmitterControls
 * @brief The state of an Emitter. An empty member is unset, and `Quantity::unset()` empties one.
 *
 * `position` is a glm::vec3 and an Emitter without one is not indexed. `color` is a
 * glm::vec3. `intensity`, `radius` and `size` are numbers.
 */
struct EmitterControls {
    Quantity position;
    Quantity intensity;
    Quantity radius;
    Quantity color;
    Quantity size;
};

MF_DESCRIBE(EmitterControls)

/**
 * @struct SensorControls
 * @brief The state of a Sensor: a glm::vec3 position and the radius it queries.
 */
struct SensorControls {
    Quantity position;
    Quantity query_radius;
};

MF_DESCRIBE(SensorControls)

/**
 * @struct AgentControls
 * @brief The state of an Agent. `orientation` is a glm::quat, local +Z forward.
 */
struct AgentControls {
    Quantity position;
    Quantity orientation;
    Quantity intensity;
    Quantity radius;
    Quantity color;
    Quantity size;
    Quantity query_radius;
};

MF_DESCRIBE(AgentControls)

/**
 * @struct PresenceControls
 * @brief What a Presence adds to an Agent: the distance its weight decays over and the curve.
 *
 * An empty `falloff_radius` follows the query radius. `falloff_curve` is a
 * Presence::FalloffCurve, applied at the next perception.
 */
struct PresenceControls {
    Quantity falloff_radius;
    Quantity falloff_curve;
};

MF_DESCRIBE(PresenceControls)

/**
 * @struct LocusControls
 * @brief What a Locus adds to an Agent: the aspect ratio of its view and a fixed timestep.
 *
 * An empty `timestep` advances the eye by wall time.
 */
struct LocusControls {
    Quantity aspect;
    Quantity timestep;
};

MF_DESCRIBE(LocusControls)

}

#include "Controls.describe.hpp"
