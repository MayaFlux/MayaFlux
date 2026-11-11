#include "StructureIntrospection.hpp"

#include "MayaFlux/Kakshya/NDData/Composite.hpp"
#include "MayaFlux/Kakshya/Source/CompositeContainer.hpp"
#include "MayaFlux/Kakshya/Source/TensorContainer.hpp"
#include "MayaFlux/Kakshya/Utils/DataUtils.hpp"

namespace MayaFlux::Yantra {

namespace {

    const Kakshya::Region& representative_region(const std::vector<Kakshya::RegionSegment>& segments)
    {
        const bool consistent_coords = std::ranges::all_of(segments,
            [&](const auto& seg) {
                return seg.source_region.end_coordinates.size() == segments[0].source_region.end_coordinates.size();
            });

        if (consistent_coords) {
            return segments[0].source_region;
        }

        return std::ranges::max_element(segments,
            [](const auto& a, const auto& b) {
                return a.source_region.end_coordinates.size() < b.source_region.end_coordinates.size();
            })
            ->source_region;
    }

    std::pair<std::vector<Kakshya::DataDimension>, Kakshya::DataModality> no_container_structure()
    {
        return { { { Kakshya::DataDimension::time(0) } }, Kakshya::DataModality::UNKNOWN };
    }

}

std::pair<std::vector<Kakshya::DataDimension>, Kakshya::DataModality>
infer_from_data_variant(const Kakshya::DataVariant& data)
{
    auto dimensions = Kakshya::detect_data_dimensions(data);
    auto modality = Kakshya::detect_data_modality(dimensions);

    return std::make_pair(std::move(dimensions), modality);
}

std::pair<std::vector<Kakshya::DataDimension>, Kakshya::DataModality>
infer_from_data_variant_vector(
    const std::vector<Kakshya::DataVariant>& data,
    const std::shared_ptr<Kakshya::SignalSourceContainer>& container)
{
    if (container && !container->get_structure().dimensions.empty()) {
        uint64_t elements = 0;
        for (const auto& variant : data) {
            elements += std::visit([](const auto& values) -> uint64_t { return values.size(); }, variant);
        }
        if (elements == container->get_total_elements()) {
            return infer_from_container(container);
        }
    }

    auto dimensions = Kakshya::detect_data_dimensions(data);
    auto modality = Kakshya::detect_data_modality(dimensions);

    return std::make_pair(std::move(dimensions), modality);
}

std::pair<std::vector<Kakshya::DataDimension>, Kakshya::DataModality>
infer_from_container(const std::shared_ptr<Kakshya::SignalSourceContainer>& container)
{
    if (!container) {
        error<std::invalid_argument>(Journal::Component::Yantra, Journal::Context::Runtime, std::source_location::current(),
            "Cannot infer structure from null container");
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
infer_from_composite_array(const Kakshya::CompositeArray& data)
{
    std::vector<Kakshya::DataDimension> dimensions;
    dimensions.emplace_back("elements", data.size(), 1, Kakshya::DataDimension::Role::CUSTOM);
    return { std::move(dimensions), Kakshya::DataModality::TENSOR_ND };
}

std::pair<std::vector<Kakshya::DataDimension>, Kakshya::DataModality>
infer_from_composite_container(const std::shared_ptr<Kakshya::CompositeContainer>& container)
{
    if (!container) {
        error<std::invalid_argument>(Journal::Component::Yantra, Journal::Context::Runtime,
            std::source_location::current(), "Cannot infer structure from null CompositeContainer");
    }

    auto dimensions = container->get_dimensions();
    auto modality = container->get_structure().modality;
    if (modality == Kakshya::DataModality::UNKNOWN)
        modality = Kakshya::DataModality::TENSOR_ND;

    return { std::move(dimensions), modality };
}

std::pair<std::vector<Kakshya::DataDimension>, Kakshya::DataModality>
infer_from_region(const Kakshya::Region& region, const std::shared_ptr<Kakshya::SignalSourceContainer>& container)
{
    if (!container) {
        std::vector<Kakshya::DataDimension> dimensions;
        dimensions.emplace_back(Kakshya::DataDimension::time(1));
        return std::make_pair(std::move(dimensions), Kakshya::DataModality::UNKNOWN);
    }

    auto [dimensions, modality] = infer_from_container(container);

    if (!region.start_coordinates.empty() && !region.end_coordinates.empty()) {
        if (region.start_coordinates.size() != region.end_coordinates.size()) {
            error<std::invalid_argument>(Journal::Component::Yantra, Journal::Context::Runtime,
                std::source_location::current(), "Region start and end coordinates must have equal dimensionality");
        }

        std::vector<uint64_t> shape;
        shape.reserve(region.start_coordinates.size());
        for (size_t axis = 0; axis < region.start_coordinates.size(); ++axis) {
            const auto start = region.start_coordinates[axis];
            const auto end = region.end_coordinates[axis];
            shape.push_back(std::max(start, end) - std::min(start, end) + 1);
        }

        modality = region.modality.value_or(modality);
        if (modality == Kakshya::DataModality::AUDIO_1D && shape.size() == 2 && shape[1] == 1)
            shape.pop_back();

        dimensions = Kakshya::DataDimension::create_dimensions(modality, shape, container->get_structure().memory_layout);
    }

    return { dimensions, modality };
}

std::pair<std::vector<Kakshya::DataDimension>, Kakshya::DataModality>
infer_from_segments(const std::vector<Kakshya::RegionSegment>& segments, const std::shared_ptr<Kakshya::SignalSourceContainer>& container)
{
    if (segments.empty() || !container) {
        return no_container_structure();
    }

    return infer_from_region(representative_region(segments), container);
}

std::pair<std::vector<Kakshya::DataDimension>, Kakshya::DataModality>
infer_from_region_group(const Kakshya::RegionGroup& group, const std::shared_ptr<Kakshya::SignalSourceContainer>& container)
{
    if (!container || group.regions.empty()) {
        return no_container_structure();
    }

    auto bounds = group.get_bounding_region();
    const auto first = group.regions.front().modality;
    const bool uniform = std::ranges::all_of(group.regions,
        [&first](const Kakshya::Region& region) { return region.modality == first; });
    bounds.modality = (uniform && first) ? *first : container->get_structure().modality;

    return infer_from_region(bounds, container);
}

std::pair<std::vector<Kakshya::DataDimension>, Kakshya::DataModality>
infer_from_tensor_container(const std::shared_ptr<Kakshya::TensorContainer>& container)
{
    if (!container) {
        error<std::invalid_argument>(Journal::Component::Yantra, Journal::Context::Runtime,
            std::source_location::current(), "Cannot infer structure from null TensorContainer");
    }

    auto dimensions = container->get_dimensions();
    auto modality = container->get_structure().modality;
    if (modality == Kakshya::DataModality::UNKNOWN)
        modality = Kakshya::DataModality::TENSOR_ND;

    return { std::move(dimensions), modality };
}

std::pair<std::vector<Kakshya::DataDimension>, Kakshya::DataModality>
infer_from_region(const Kakshya::Region& region, const std::shared_ptr<Kakshya::TensorContainer>& container)
{
    if (!container) {
        return { { { Kakshya::DataDimension::time(1) } }, Kakshya::DataModality::UNKNOWN };
    }

    if (region.start_coordinates.empty() && region.end_coordinates.empty()) {
        return infer_from_tensor_container(container);
    }

    const auto window = container->get_data().spec().window(region);
    if (!window) {
        error<std::invalid_argument>(Journal::Component::Yantra, Journal::Context::Runtime,
            std::source_location::current(), "Region does not select a box of the tensor");
    }

    auto modality = container->get_structure().modality;
    if (modality == Kakshya::DataModality::UNKNOWN)
        modality = Kakshya::DataModality::TENSOR_ND;

    return { window->dimensions(), region.modality.value_or(modality) };
}

std::pair<std::vector<Kakshya::DataDimension>, Kakshya::DataModality>
infer_from_region(const Kakshya::Region& region, const std::shared_ptr<Kakshya::CompositeContainer>& container)
{
    if (!container) {
        return { { { Kakshya::DataDimension::time(1) } }, Kakshya::DataModality::UNKNOWN };
    }

    if (region.start_coordinates.empty() && region.end_coordinates.empty()) {
        return infer_from_composite_container(container);
    }

    if (region.start_coordinates.size() != region.end_coordinates.size()) {
        error<std::invalid_argument>(Journal::Component::Yantra, Journal::Context::Runtime,
            std::source_location::current(), "Region start and end coordinates must have equal dimensionality");
    }

    const auto first = region.start_coordinates.front();
    const auto last = region.end_coordinates.front();
    const auto span = std::max(first, last) - std::min(first, last) + 1;

    auto modality = container->get_structure().modality;
    if (modality == Kakshya::DataModality::UNKNOWN)
        modality = Kakshya::DataModality::TENSOR_ND;

    std::vector<Kakshya::DataDimension> dimensions;
    dimensions.emplace_back("elements", span, 1, Kakshya::DataDimension::Role::CUSTOM);
    return { std::move(dimensions), region.modality.value_or(modality) };
}

std::pair<std::vector<Kakshya::DataDimension>, Kakshya::DataModality>
infer_from_region_group(const Kakshya::RegionGroup& group, const std::shared_ptr<Kakshya::TensorContainer>& container)
{
    if (!container || group.regions.empty()) {
        return no_container_structure();
    }
    return infer_from_region(group.get_bounding_region(), container);
}

std::pair<std::vector<Kakshya::DataDimension>, Kakshya::DataModality>
infer_from_region_group(const Kakshya::RegionGroup& group, const std::shared_ptr<Kakshya::CompositeContainer>& container)
{
    if (!container || group.regions.empty()) {
        return no_container_structure();
    }
    return infer_from_region(group.get_bounding_region(), container);
}

std::pair<std::vector<Kakshya::DataDimension>, Kakshya::DataModality>
infer_from_segments(const std::vector<Kakshya::RegionSegment>& segments, const std::shared_ptr<Kakshya::TensorContainer>& container)
{
    if (segments.empty() || !container) {
        return no_container_structure();
    }
    return infer_from_region(representative_region(segments), container);
}

std::pair<std::vector<Kakshya::DataDimension>, Kakshya::DataModality>
infer_from_segments(const std::vector<Kakshya::RegionSegment>& segments, const std::shared_ptr<Kakshya::CompositeContainer>& container)
{
    if (segments.empty() || !container) {
        return no_container_structure();
    }
    return infer_from_region(representative_region(segments), container);
}

}
