#include "StructureIntrospection.hpp"
#include "MayaFlux/Kakshya/Utils/DataUtils.hpp"

namespace MayaFlux::Yantra {

std::pair<std::vector<Kakshya::DataDimension>, Kakshya::DataModality>
infer_from_data_variant(const Kakshya::DataVariant& data)
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

    auto dimensions = container->get_dimensions();
    auto modality = Kakshya::detect_data_modality(dimensions);

    return std::make_pair(std::move(dimensions), modality);
}

std::pair<std::vector<Kakshya::DataDimension>, Kakshya::DataModality>
infer_from_region(const Kakshya::Region& region)
{

    // Regions are spatial/temporal markers, not data containers
    // Create placeholder dimensions based on region bounds
    std::vector<Kakshya::DataDimension> dimensions;

    if (!region.start_coordinates.empty() && !region.end_coordinates.empty()) {
        // Create dimensions based on region bounds
        for (size_t i = 0; i < region.start_coordinates.size(); ++i) {
            u_int64_t size = (i < region.end_coordinates.size()) ? (region.end_coordinates[i] - region.start_coordinates[i]) : 1;
            dimensions.emplace_back(Kakshya::DataDimension::spatial(size, 'x' + static_cast<char>(i), i));
        }
    } else {
        // Default single dimension
        dimensions.emplace_back(Kakshya::DataDimension::time(1));
    }

    // Regions typically indicate spatial/temporal selections
    Kakshya::DataModality modality = (dimensions.size() == 1) ? Kakshya::DataModality::AUDIO_1D : Kakshya::DataModality::TENSOR_ND;

    return std::make_pair(std::move(dimensions), modality);
}

std::pair<std::vector<Kakshya::DataDimension>, Kakshya::DataModality>
infer_from_segments(const std::vector<Kakshya::RegionSegment>& segments)
{

    if (segments.empty()) {
        std::vector<Kakshya::DataDimension> dimensions;
        dimensions.emplace_back(Kakshya::DataDimension::time(0));
        return std::make_pair(std::move(dimensions), Kakshya::DataModality::UNKNOWN);
    }

    // Try to extract data from first segment to infer structure
    const auto& first_segment = segments[0];
    auto data_it = first_segment.processing_metadata.find("data");

    if (data_it != first_segment.processing_metadata.end()) {
        try {
            // Try as DataVariant first
            auto data_variant = std::any_cast<Kakshya::DataVariant>(data_it->second);
            return infer_from_data_variant(data_variant);
        } catch (const std::bad_any_cast&) {
            try {
                // Try as vector<double>
                auto double_vec = std::any_cast<std::vector<double>>(data_it->second);
                Kakshya::DataVariant variant { double_vec };
                return infer_from_data_variant(variant);
            } catch (const std::bad_any_cast&) {
                // Fallback to default
            }
        }
    }

    // Fallback: create structure based on segment count
    std::vector<Kakshya::DataDimension> dimensions;
    dimensions.emplace_back(Kakshya::DataDimension::time(segments.size()));
    return std::make_pair(std::move(dimensions), Kakshya::DataModality::AUDIO_1D);
}

std::pair<std::vector<Kakshya::DataDimension>, Kakshya::DataModality>
infer_from_region_group(const Kakshya::RegionGroup& group)
{

    if (group.regions.empty()) {
        // Empty group - create minimal structure
        std::vector<Kakshya::DataDimension> dimensions;
        dimensions.emplace_back(Kakshya::DataDimension::time(0));
        return std::make_pair(std::move(dimensions), Kakshya::DataModality::UNKNOWN);
    }

    // Infer from first region as representative
    return infer_from_region(group.regions[0]);
}

}
