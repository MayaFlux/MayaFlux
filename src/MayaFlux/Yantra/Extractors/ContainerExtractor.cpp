#include "ContainerExtractor.hpp"

#include "MayaFlux/EnumUtils.hpp"
#include "MayaFlux/Kakshya/Utils/CoordUtils.hpp"
#include "MayaFlux/Kakshya/Utils/RegionUtils.hpp"

namespace MayaFlux::Yantra {

ContainerExtractor::ContainerExtractor()
    : m_contiguous_processor(std::make_shared<Kakshya::ContiguousAccessProcessor>())
    , m_region_processor(std::make_shared<Kakshya::RegionOrganizationProcessor>(m_container))
{
    set_parameter("channel_index", 0U);
    set_parameter("frame_index", 0U);
    set_parameter("slice_start", std::vector<uint64_t> {});
    set_parameter("slice_end", std::vector<uint64_t> {});
    set_parameter("subsample_factor", 1U);
    set_parameter("cache_enabled", true);
}

std::vector<std::string> ContainerExtractor::get_available_methods() const
{
    auto names = Utils::get_enum_names_lowercase<ContainerExtractionMethod>();
    return std::vector<std::string>(names.begin(), names.end());
}

std::vector<std::string> ContainerExtractor::get_methods_for_type_impl(std::type_index type_info) const
{
    if (type_info == std::type_index(typeid(std::shared_ptr<Kakshya::SignalSourceContainer>))) {
        return get_available_methods();
    }

    static const std::vector<std::string> region_methods = {
        "region_data", "region_bounds", "region_metadata"
    };

    if (type_info == std::type_index(typeid(Kakshya::Region))) {
        return region_methods;
    }

    static const std::vector<std::string> group_methods = {
        "region_data", "region_bounds", "region_metadata", "all_regions"
    };

    if (type_info == std::type_index(typeid(Kakshya::RegionGroup))) {
        return group_methods;
    }

    static const std::vector<std::string> segment_methods = {
        "region_data", "region_metadata"
    };

    if (type_info == std::type_index(typeid(std::vector<Kakshya::RegionSegment>))) {
        return segment_methods;
    }

    static const std::vector<std::string> data_methods = {
        "data_type", "total_elements"
    };

    if (type_info == std::type_index(typeid(Kakshya::DataVariant))) {
        return data_methods;
    }

    return {};
}

ExtractorOutput ContainerExtractor::extract_impl(std::shared_ptr<Kakshya::SignalSourceContainer> container)
{
    if (!container) {
        throw std::invalid_argument("Container is null");
    }

    const std::string method = get_extraction_method();
    const auto extraction_method = string_to_method(method);

    const bool cache_enabled = std::any_cast<bool>(get_parameter("cache_enabled"));
    if (cache_enabled) {
        const std::string cache_key = method + "_" + std::to_string(reinterpret_cast<uintptr_t>(container.get()));
        auto it = m_extraction_cache.find(cache_key);
        if (it != m_extraction_cache.end()) {
            // Cast from std::any back to ExtractorOutput
            if (auto cached_result = std::any_cast<ExtractorOutput>(&it->second)) {
                return *cached_result;
            }
        }
    }

    ExtractorOutput result;

    switch (extraction_method) {
    case ContainerExtractionMethod::TOTAL_ELEMENTS:
        result = create_typed_output(container->get_total_elements());
        break;
    case ContainerExtractionMethod::FRAME_SIZE:
        result = create_typed_output(container->get_frame_size());
        break;
    case ContainerExtractionMethod::NUM_FRAMES:
        result = create_typed_output(container->get_num_frames());
        break;
    case ContainerExtractionMethod::MEMORY_LAYOUT:
        result = create_typed_output(static_cast<int>(container->get_memory_layout()));
        break;

    case ContainerExtractionMethod::DIMENSIONS:
        result = create_typed_output(Kakshya::create_dimension_info(container->get_dimensions()));
        break;
    case ContainerExtractionMethod::DIMENSION_ROLES:
        result = create_typed_output(Kakshya::extract_dimension_roles(container->get_dimensions()));
        break;
    case ContainerExtractionMethod::DIMENSION_SIZES:
        result = create_typed_output(Kakshya::extract_dimension_sizes(container->get_dimensions()));
        break;
    case ContainerExtractionMethod::STRIDES:
        result = create_typed_output(Kakshya::calculate_strides(container->get_dimensions()));
        break;
    case ContainerExtractionMethod::COORDINATE_MAPPING:
        result = create_typed_output(Kakshya::create_coordinate_mapping(container));
        break;

    case ContainerExtractionMethod::CHANNEL_DATA:
        result = extract_parametric_region_data(container, "channel");
        break;
    case ContainerExtractionMethod::FRAME_DATA:
        result = extract_parametric_region_data(container, "frame");
        break;
    case ContainerExtractionMethod::SLICE_DATA:
        result = extract_parametric_region_data(container, "slice");
        break;

    case ContainerExtractionMethod::REGION_DATA:
    case ContainerExtractionMethod::ALL_REGIONS:
        result = create_typed_output(extract_all_regions(container));
        break;

    case ContainerExtractionMethod::PROCESSING_STATE:
        result = create_typed_output(extract_processing_state(container));
        break;
    case ContainerExtractionMethod::PROCESSOR_INFO:
        result = create_typed_output(extract_processing_state(container));
        break;

    default:
        throw std::runtime_error("Unsupported extraction method: " + method);
    }

    if (cache_enabled) {
        const std::string cache_key = method + "_" + std::to_string(reinterpret_cast<uintptr_t>(container.get()));
        m_extraction_cache[cache_key] = result;
    }

    return result;
}

ExtractorOutput ContainerExtractor::extract_impl(const Kakshya::Region& region)
{
    const std::string method = get_extraction_method();
    const auto extraction_method = string_to_method(method);

    switch (extraction_method) {
    case ContainerExtractionMethod::REGION_DATA:
        return create_typed_output(get_context_container()->get_region_data(region));
    case ContainerExtractionMethod::REGION_BOUNDS:
        return create_typed_output(extract_region_bounds(region));
    case ContainerExtractionMethod::REGION_METADATA:
        return create_typed_output(region.attributes);
    default:
        throw std::runtime_error("Unsupported region extraction method: " + method);
    }
}

ExtractorOutput ContainerExtractor::extract_impl(const Kakshya::RegionGroup& group)
{
    const std::string method = get_extraction_method();
    const auto extraction_method = string_to_method(method);

    switch (extraction_method) {
    case ContainerExtractionMethod::REGION_DATA:
        return create_typed_output(extract_group_data(group, get_context_container()));
    case ContainerExtractionMethod::REGION_BOUNDS:
        return create_typed_output(extract_group_bounds(group));
    case ContainerExtractionMethod::REGION_METADATA:
        return extract_group_metadata(group);
    case ContainerExtractionMethod::ALL_REGIONS:
        return create_typed_output(group.regions);
    default:
        throw std::runtime_error("Unsupported group extraction method: " + method);
    }
}

ExtractorOutput ContainerExtractor::extract_impl(const std::vector<Kakshya::RegionSegment>& segments)
{
    const std::string method = get_extraction_method();
    const auto extraction_method = string_to_method(method);

    switch (extraction_method) {
    case ContainerExtractionMethod::REGION_DATA:
        return create_typed_output(Kakshya::extract_segments_data(segments, get_context_container()));
    case ContainerExtractionMethod::REGION_METADATA:
        return create_typed_output(Kakshya::extract_segments_metadata(segments));
    default:
        throw std::runtime_error("Unsupported segments extraction method: " + method);
    }
}

ExtractorOutput ContainerExtractor::extract_impl(const Kakshya::DataVariant& data)
{
    const std::string method = get_extraction_method();
    const auto extraction_method = string_to_method(method);

    switch (extraction_method) {
    case ContainerExtractionMethod::DATA_TYPE: {
        std::string type_name = std::visit([](const auto& vec) -> std::string {
            using T = typename std::decay_t<decltype(vec)>::value_type;
            return typeid(T).name();
        },
            data);
        return create_typed_output(type_name);
    }
    case ContainerExtractionMethod::TOTAL_ELEMENTS: {
        uint64_t total = std::visit([](const auto& vec) -> uint64_t {
            return vec.size();
        },
            data);
        return create_typed_output(total);
    }
    default:
        throw std::runtime_error("Unsupported data variant extraction method: " + method);
    }
}

ExtractorOutput ContainerExtractor::extract_parametric_region_data(
    std::shared_ptr<Kakshya::SignalSourceContainer> container,
    const std::string& extraction_type)
{
    const auto dimensions = container->get_dimensions();
    Kakshya::Region region;

    if (extraction_type == "channel") {
        const auto channel_index = safe_any_cast_or_throw<uint32_t>(get_parameter("channel_index"));

        for (size_t i = 0; i < dimensions.size(); ++i) {
            if (dimensions[i].role == Kakshya::DataDimension::Role::CHANNEL) {
                if (channel_index >= dimensions[i].size) {
                    throw std::out_of_range("Channel index out of range");
                }

                region = Kakshya::Region::audio_span(0, dimensions[0].size, channel_index, channel_index + 1);
                return create_typed_output(container->get_region_data(region));
            }
        }
        throw std::runtime_error("No channel dimension found");
    }

    else if (extraction_type == "frame") {
        const auto frame_index = safe_any_cast_or_throw<uint32_t>(get_parameter("frame_index"));

        if (dimensions.empty() || frame_index >= dimensions[0].size) {
            throw std::out_of_range("Frame index out of range");
        }

        region = Kakshya::Region::time_span(frame_index, frame_index + 1);
        return create_typed_output(container->get_region_data(region));
    }

    else if (extraction_type == "slice") {
        const auto slice_start = safe_any_cast_or_throw<std::vector<uint64_t>>(get_parameter("slice_start"));
        const auto slice_end = safe_any_cast_or_throw<std::vector<uint64_t>>(get_parameter("slice_end"));

        if (slice_start.empty() || slice_end.empty()) {
            throw std::invalid_argument("Slice coordinates cannot be empty");
        }

        if (!Kakshya::validate_slice_bounds(slice_start, slice_end, dimensions)) {
            throw std::invalid_argument("Invalid slice coordinates");
        }

        region = Kakshya::Region(slice_start, slice_end);
        return create_typed_output(container->get_region_data(region));
    }

    throw std::invalid_argument("Unknown extraction type: " + extraction_type);
}

ExtractorOutput ContainerExtractor::extract_group_metadata(const Kakshya::RegionGroup& group)
{
    std::unordered_map<std::string, std::any> metadata;
    metadata["group_name"] = group.name;
    metadata["group_attributes"] = group.attributes;

    std::vector<std::unordered_map<std::string, std::any>> region_metadata;
    region_metadata.reserve(group.regions.size());

    for (const auto& region : group.regions) {
        region_metadata.push_back(region.attributes);
    }

    metadata["region_attributes"] = std::move(region_metadata);
    return create_typed_output(std::move(metadata));
}

ContainerExtractionMethod ContainerExtractor::string_to_method(const std::string& method_str)
{
    return Utils::string_to_enum_or_throw_case_insensitive<ContainerExtractionMethod>(method_str, "ContainerExtractionMethod");
}

std::string ContainerExtractor::method_to_string(ContainerExtractionMethod method)
{
    return static_cast<std::string>(Utils::enum_to_lowercase_string(method));
}

}
