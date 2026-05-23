#pragma once

#include "MayaFlux/Kinesis/Spatial/SpatialIndex.hpp"

namespace MayaFlux::Nexus {

/**
 * @struct PerceptionContext
 * @brief Data passed to a Sensor or Agent perception function on each commit.
 *
 * @c spatial_results contains the entities within the sensor's query radius
 * at the moment of the last published snapshot. Results hold entity ids and
 * squared distances as returned by @c SpatialIndex::within_radius.
 *
 * Fields marked @note future are reserved for later domain expansions.
 */
struct PerceptionContext {
    glm::vec3 position {};
    std::span<const Kinesis::QueryResult> spatial_results;

    // @note future: EnergyAnalysis audio_energy {};
    // @note future: std::span<const double> audio_snapshot {};
};

} // namespace MayaFlux::Nexus
