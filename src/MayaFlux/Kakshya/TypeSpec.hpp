#pragma once

#include "Region.hpp"

namespace MayaFlux::Kakshya {

// Region access concepts
template <typename T>
concept RegionExtractable = requires(T t, const Region& region) {
    { extract_region_data(t, region) } -> ContiguousContainer;
};

// Dimension concepts
template <typename T>
concept DimensionalData = requires(T t) {
    { t.get_dimensions() } -> std::convertible_to<std::vector<DataDimension>>;
    { t.get_processed_data() } -> std::convertible_to<const DataVariant&>;
};
}
