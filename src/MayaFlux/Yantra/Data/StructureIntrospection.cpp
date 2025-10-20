#include "StructureIntrospection.hpp"

#include <algorithm>

#include "MayaFlux/Kakshya/Utils/DataUtils.hpp"
#include "MayaFlux/Kakshya/Utils/RegionUtils.hpp"

namespace MayaFlux::Yantra {

std::pair<std::vector<Kakshya::DataDimension>, Kakshya::DataModality>
infer_from_data_variant(const Kakshya::DataVariant& data)
{
    auto dimensions = Kakshya::detect_data_dimensions(data);
    auto modality = Kakshya::detect_data_modality(dimensions);

    return std::make_pair(std::move(dimensions), modality);
}

std::pair<std::vector<Kakshya::DataDimension>, Kakshya::DataModality>
infer_from_data_variant_vector(const std::vector<Kakshya::DataVariant>& data)
{
    auto dimensions = Kakshya::detect_data_dimensions(data);
    auto modality = Kakshya::detect_data_modality(dimensions);

    return std::make_pair(std::move(dimensions), modality);
}

std::pair<std::vector<Kakshya::DataDimension>, Kakshya::DataModality>
infer_from_container(const std::shared_ptr<Kakshya::SignalSourceContainer>& container)
{
    if (!container) {
        throw std::invalid_argument("Cannot infer structure from null container");
    }

    const auto& structure = container->get_structure();
    auto dimensions = structure.dimensions;

    auto modality = structure.modality;
    if (modality == Kakshya::DataModality::UNKNOWN) {
        modality = Kakshya::detect_data_modality(dimensions);
    }

    if (dimensions.empty()) {
        auto total_elements = structure.get_total_elements();
        if (total_elements > 0) {
            dimensions.emplace_back(Kakshya::DataDimension::time(total_elements));
            modality = Kakshya::DataModality::AUDIO_1D;
        }
    }

    return std::make_pair(std::move(dimensions), modality);
}

std::pair<std::vector<Kakshya::DataDimension>, Kakshya::DataModality>
infer_from_region(const Kakshya::Region& region, const std::shared_ptr<Kakshya::SignalSourceContainer>& container)
{
    if (!container) {
        std::vector<Kakshya::DataDimension> dimensions;
        dimensions.emplace_back(Kakshya::DataDimension::time(1));
        return std::make_pair(std::move(dimensions), Kakshya::DataModality::UNKNOWN);
    }

    auto structure = container->get_structure();

    auto [dimensions, modality] = infer_from_container(container);

    if (!region.start_coordinates.empty() && !region.end_coordinates.empty() && region.end_coordinates[1] != container->get_frame_size()) {
        std::vector<uint64_t> shape;
        int size = static_cast<int>(region.end_coordinates[0] - region.start_coordinates[0]);
        shape.push_back(std::abs(size) + 1);
        shape.push_back(region.end_coordinates[1]);

        if (auto region_modality = region.get_attribute<Kakshya::DataModality>("modality");
            region_modality.has_value()) {

            dimensions = Kakshya::DataDimension::create_dimensions(region_modality.value(), shape, structure.memory_layout);
            modality = region_modality.value();
        } else {
            dimensions = Kakshya::DataDimension::create_dimensions(modality, shape, structure.memory_layout);
        }
    }

    return { dimensions, modality };
}

std::pair<std::vector<Kakshya::DataDimension>, Kakshya::DataModality>
infer_from_segments(const std::vector<Kakshya::RegionSegment>& segments, const std::shared_ptr<Kakshya::SignalSourceContainer>& container)
{
    if (segments.empty() || !container) {
        return { { { Kakshya::DataDimension::time(0) } }, Kakshya::DataModality::UNKNOWN };
    }

    bool consistent_coords = std::ranges::all_of(segments,
        [&](const auto& seg) {
            return seg.source_region.end_coordinates.size() == segments[0].source_region.end_coordinates.size();
        });

    if (consistent_coords) {
        return infer_from_region(segments[0].source_region, container);
    }

    auto max_coord_segment = std::ranges::max_element(segments,
        [](const auto& a, const auto& b) {
            return a.source_region.end_coordinates.size() < b.source_region.end_coordinates.size();
        });
    return infer_from_region(max_coord_segment->source_region, container);
}

std::pair<std::vector<Kakshya::DataDimension>, Kakshya::DataModality>
infer_from_region_group(const Kakshya::RegionGroup& group, const std::shared_ptr<Kakshya::SignalSourceContainer>& container)
{
    if (!container || group.regions.empty()) {
        return { { { Kakshya::DataDimension::time(0) } }, Kakshya::DataModality::UNKNOWN };
    }

    auto structure = container->get_structure();
    auto bounds_info = Kakshya::extract_group_bounds_info(group);

    if (bounds_info.contains("bounding_min") && bounds_info.contains("bounding_max")) {
        auto min_coords = std::any_cast<std::vector<uint64_t>>(bounds_info["bounding_min"]);
        auto max_coords = std::any_cast<std::vector<uint64_t>>(bounds_info["bounding_max"]);

        std::vector<uint64_t> shape;
        int size = static_cast<int>(max_coords[0] - min_coords[0]);
        shape.push_back(std::abs(size) + 1);
        shape.push_back(max_coords[1]);

        auto modality = group.attributes.contains("modality") ? std::any_cast<Kakshya::DataModality>(group.attributes.at("modality")) : structure.modality;

        auto dimensions = Kakshya::DataDimension::create_dimensions(modality, shape, structure.memory_layout);
        return { dimensions, modality };
    }

    return infer_from_region(group.regions[0], container);
}

}
