#include "YantraUtils.hpp"

namespace MayaFlux::Yantra {

auto create_duration_comparator(SortDirection direction)
{
    return [direction](const Kakshya::RegionSegment& a, const Kakshya::RegionSegment& b) -> bool {
        if (!a.source_region.start_coordinates.empty() && !a.source_region.end_coordinates.empty() && !b.source_region.start_coordinates.empty() && !b.source_region.end_coordinates.empty()) {

            auto duration_a = a.source_region.end_coordinates[0] - a.source_region.start_coordinates[0];
            auto duration_b = b.source_region.end_coordinates[0] - b.source_region.start_coordinates[0];

            return direction == SortDirection::ASCENDING ? duration_a < duration_b : duration_a > duration_b;
        }

        if (!a.source_region.start_coordinates.empty() && !b.source_region.start_coordinates.empty()) {
            return direction == SortDirection::ASCENDING ? a.source_region.start_coordinates[0] < b.source_region.start_coordinates[0] : a.source_region.start_coordinates[0] > b.source_region.start_coordinates[0];
        }

        return false;
    };
}

bool is_complex_data(const Kakshya::DataVariant& data)
{
    return std::visit([](const auto& vec) -> bool {
        using VecType = std::decay_t<decltype(vec)>;

        if constexpr (std::ranges::range<VecType>) {
            using ValueType = typename VecType::value_type;
            return std::same_as<ValueType, std::complex<float>> || std::same_as<ValueType, std::complex<double>>;
        }
        return false;
    },
        data);
}

bool is_standard_sortable_data(const Kakshya::DataVariant& data)
{
    return std::visit([](const auto& vec) -> bool {
        using VecType = std::decay_t<decltype(vec)>;

        if constexpr (std::ranges::range<VecType>) {
            using ValueType = typename VecType::value_type;
            return StandardSortable<ValueType>;
        }
        return false;
    },
        data);
}

/* template void sort_container<std::vector<double>>(std::vector<double>&, SortDirection, SortingAlgorithm);
template void sort_container<std::vector<float>>(std::vector<float>&, SortDirection, SortingAlgorithm);
template void sort_container<std::vector<int>>(std::vector<int>&, SortDirection, SortingAlgorithm);

template void sort_complex_container<std::vector<std::complex<double>>>(std::vector<std::complex<double>>&, SortDirection, SortingAlgorithm);
template void sort_complex_container<std::vector<std::complex<float>>>(std::vector<std::complex<float>>&, SortDirection, SortingAlgorithm);

template std::vector<std::vector<double>> sort_chunked_standard<std::vector<double>>(const std::vector<double>&, size_t, SortDirection, SortingAlgorithm);
template std::vector<std::vector<float>> sort_chunked_standard<std::vector<float>>(const std::vector<float>&, size_t, SortDirection, SortingAlgorithm); */

void sort_regions(std::vector<Kakshya::Region>& regions, SortDirection direction, SortingAlgorithm algorithm)
{
    auto comparator = create_coordinate_comparator<Kakshya::Region>(direction);
    execute_sorting_algorithm(regions.begin(), regions.end(), comparator, algorithm);
}

void sort_segments(std::vector<Kakshya::RegionSegment>& segments, SortDirection direction, SortingAlgorithm algorithm)
{
    auto comparator = create_duration_comparator(direction);
    execute_sorting_algorithm(segments.begin(), segments.end(), comparator, algorithm);
}

} // namespace MayaFlux::Yantra
