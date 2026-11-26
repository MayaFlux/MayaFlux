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
 * @struct PlacementControls
 * @brief Where an Emitter, Sensor or Agent stands and which way it faces.
 *
 * `position` is a glm::vec3 and an entity without one is not indexed. `orientation`
 * is a glm::quat, local -Z forward. An empty member is unset, and
 * `Quantity::unset()` empties one.
 */
struct PlacementControls {
    Quantity position;
    Quantity orientation;
};

MF_DESCRIBE(PlacementControls)

/**
 * @struct EmitterControls
 * @brief What an Emitter puts out. `color` is a glm::vec3, the rest are numbers but for two.
 *
 * `cone` is a glm::vec2 of inner and outer angles about forward, radians from the axis, and
 * `attenuation` a glm::vec3 of constant, linear and quadratic terms. Empty, the Emitter
 * lights all ways and falls to its radius alone.
 */
struct EmitterControls {
    Quantity intensity;
    Quantity radius;
    Quantity color;
    Quantity size;
    Quantity cone;
    Quantity attenuation;
};

MF_DESCRIBE(EmitterControls)

/**
 * @struct SensorControls
 * @brief What a Sensor takes in: the radius it queries.
 */
struct SensorControls {
    Quantity query_radius;
};

MF_DESCRIBE(SensorControls)

/**
 * @struct AgentControls
 * @brief What an Agent puts out and the radius it queries. `cone` and `attenuation` as EmitterControls.
 */
struct AgentControls {
    Quantity intensity;
    Quantity radius;
    Quantity color;
    Quantity size;
    Quantity query_radius;
    Quantity cone;
    Quantity attenuation;
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

/**
 * @struct ExpanseControls
 * @brief The state of an Expanse: the box of its region.
 *
 * `bounds` is a Kinesis::AABB3D in world space. An Expanse with none is a predicate alone,
 * or contains nothing.
 */
struct ExpanseControls {
    Quantity bounds;
};

MF_DESCRIBE(ExpanseControls)

/**
 * @struct HullControls
 * @brief What a Hull adds to an Expanse: the box of its buffer's geometry in the buffer's stored coordinates.
 *
 * `local` is a Kinesis::AABB3D. It is set from the buffer when the Hull is built and on refit(),
 * and may be set from any thread.
 */
struct HullControls {
    Quantity local;
};

MF_DESCRIBE(HullControls)

/**
 * @struct MantleControls
 * @brief What a Mantle adds to an Expanse: the geometry it fits and how it fits it.
 *
 * `local` is the box of the buffer's geometry in stored coordinates, a Kinesis::AABB3D.
 * `fit` is a Portal::Graphics::FitMode. A change of either is applied to the buffer at the
 * next evaluation of the Expanse.
 */
struct MantleControls {
    Quantity local;
    Quantity fit;
};

MF_DESCRIBE(MantleControls)

}

#include "Controls.describe.hpp"
