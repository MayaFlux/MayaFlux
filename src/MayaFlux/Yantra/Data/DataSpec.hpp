#pragma once

#include "Eigen/Core"
#include "MayaFlux/Kakshya/SignalSourceContainer.hpp"

namespace MayaFlux::Yantra {
// clang-format off

/**
 * @concept ComputeData
 * @brief Universal concept for types that can be used as data in compute operations.
 *
 * Defines the valid types that can flow through the computation pipeline.
 * Supports both direct type matching and constructible conversion patterns.
 *
 * Following types are considered valid:
 * - Kakshya::DataVariant: Universal data container
 * - std::shared_ptr<Kakshya::SignalSourceContainer>: Shared signal sources
 * - Kakshya::Region: Spatial/temporal markers
 * - Kakshya::RegionGroup: Collections of regions
 * - Eigen::MatrixBase<T>: Any Eigen matrix type (e.g., Vector3f, MatrixXd)
 * - std::vector<Kakshya::RegionSegment>: Cached region data (with values)
 * - Any type constructible from Kakshya::DataVariant
 */
template <typename T>
concept ComputeData =
    std::same_as<T, Kakshya::DataVariant> ||
    std::same_as<T, std::shared_ptr<Kakshya::SignalSourceContainer>> ||
    std::same_as<T, Kakshya::Region> ||
    std::same_as<T, Kakshya::RegionGroup> ||
    std::same_as<T, std::vector<Kakshya::RegionSegment>> ||
    std::is_base_of_v<Eigen::MatrixBase<T>, T> ||
    std::constructible_from<Kakshya::DataVariant, T>;
// clang-format on
}
