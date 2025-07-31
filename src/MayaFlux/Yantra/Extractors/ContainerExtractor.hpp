#pragma once

#include "MayaFlux/Kakshya/Processors/ContiguousAccessProcessor.hpp"
#include "MayaFlux/Kakshya/Processors/RegionProcessors.hpp"
#include "MayaFlux/Kakshya/Utils/DataUtils.hpp"
#include "MayaFlux/Yantra/Extractors/UniversalExtractor.hpp"

/**
 * @file ContainerExtractor.hpp
 * @brief Digital-first extractor for SignalSourceContainer data and metadata
 *
 * ContainerExtractor provides comprehensive extraction capabilities for container-based
 * data, focusing purely on data retrieval without analysis. It leverages KakshyaUtils,
 * existing processors, and C++20 features to provide efficient, type-safe container
 * data access in the digital paradigm.
 *
 * Key Features:
 * - **Pure extraction:** Retrieves data without analysis (delegates analysis to analyzers)
 * - **Multi-modal support:** Works with audio, spectral, spatial, and N-dimensional data
 * - **Region-aware:** Integrated region extraction and manipulation
 * - **Processor integration:** Uses existing ContiguousAccessProcessor and RegionProcessors
 * - **C++20 features:** Concepts, spans, structured bindings, and optional
 * - **Efficient memory access:** Leverages KakshyaUtils for optimal data access patterns
 * - **Type-safe extraction:** Compile-time type checking with concepts
 */

namespace MayaFlux::Yantra {

/**
 * @concept ExtractableContainerData
 * @brief Concept for data types that can be extracted from containers
 */
template <typename T>
concept ExtractableContainerData = requires {
    std::is_arithmetic_v<T> || std::is_same_v<T, std::complex<float>> || std::is_same_v<T, std::complex<double>>;
};

/**
 * @enum ContainerExtractionMethod
 * @brief Available container extraction methods
 */
enum class ContainerExtractionMethod {
    // Basic container properties
    DIMENSIONS,
    TOTAL_ELEMENTS,
    FRAME_SIZE,
    NUM_FRAMES,
    MEMORY_LAYOUT,
    DATA_TYPE,

    // Raw data extraction
    // RAW_DATA,
    CHANNEL_DATA,
    FRAME_DATA,
    SLICE_DATA,

    // Region-based extraction
    REGION_DATA,
    REGION_BOUNDS,
    REGION_METADATA,
    ALL_REGIONS,

    // Processing state
    PROCESSING_STATE,
    READ_POSITION,
    PROCESSOR_INFO,

    // Dimension analysis
    DIMENSION_ROLES,
    DIMENSION_SIZES,
    STRIDES,
    COORDINATE_MAPPING,

    // Advanced extraction
    SUBSAMPLE_DATA,
    INTERLEAVED_DATA,
    CONTIGUOUS_DATA
};

/**
 * @class ContainerExtractor
 * @brief Universal extractor for SignalSourceContainer data and metadata
 *
 * Provides comprehensive, digital-first extraction of container data, metadata,
 * and structural information. Focuses on efficient data retrieval while delegating
 * analysis to appropriate analyzers.
 */
class ContainerExtractor : public UniversalExtractor {
public:
    ContainerExtractor();
    ~ContainerExtractor() = default;

    /**
     * @brief Get all available extraction methods
     */
    std::vector<std::string> get_available_methods() const override;

protected:
    // Core extraction implementations
    ExtractorOutput extract_impl(std::shared_ptr<Kakshya::SignalSourceContainer> container) override;
    ExtractorOutput extract_impl(const Kakshya::Region& region) override;
    ExtractorOutput extract_impl(const Kakshya::RegionGroup& group) override;
    ExtractorOutput extract_impl(const std::vector<Kakshya::RegionSegment>& segments) override;
    ExtractorOutput extract_impl(const Kakshya::DataVariant& data) override;

    std::vector<std::string> get_methods_for_type_impl(std::type_index type_info) const override;

private:
    // Container data extraction methods
    ExtractorOutput extract_dimensions(std::shared_ptr<Kakshya::SignalSourceContainer> container);
    // ExtractorOutput extract_raw_data(std::shared_ptr<Kakshya::SignalSourceContainer> container);
    ExtractorOutput extract_channel_data(std::shared_ptr<Kakshya::SignalSourceContainer> container);
    ExtractorOutput extract_frame_data(std::shared_ptr<Kakshya::SignalSourceContainer> container);
    ExtractorOutput extract_slice_data(std::shared_ptr<Kakshya::SignalSourceContainer> container);
    ExtractorOutput extract_processing_state(std::shared_ptr<Kakshya::SignalSourceContainer> container);
    ExtractorOutput extract_dimension_analysis(std::shared_ptr<Kakshya::SignalSourceContainer> container);

    // Region extraction methods
    ExtractorOutput extract_region_data(const Kakshya::Region& region,
        std::shared_ptr<Kakshya::SignalSourceContainer> container = nullptr);
    ExtractorOutput extract_region_bounds(const Kakshya::Region& region);
    ExtractorOutput extract_region_metadata(const Kakshya::Region& region);
    ExtractorOutput extract_all_regions(std::shared_ptr<Kakshya::SignalSourceContainer> container);

    // Region group extraction methods
    ExtractorOutput extract_group_data(const Kakshya::RegionGroup& group,
        std::shared_ptr<Kakshya::SignalSourceContainer> container = nullptr);
    ExtractorOutput extract_group_bounds(const Kakshya::RegionGroup& group);
    ExtractorOutput extract_group_metadata(const Kakshya::RegionGroup& group);

    // Segment extraction methods
    ExtractorOutput extract_segments_data(const std::vector<Kakshya::RegionSegment>& segments);
    ExtractorOutput extract_segments_metadata(const std::vector<Kakshya::RegionSegment>& segments);

    // Advanced extraction methods
    ExtractorOutput extract_subsample_data(std::shared_ptr<Kakshya::SignalSourceContainer> container);
    ExtractorOutput extract_coordinate_mapping(std::shared_ptr<Kakshya::SignalSourceContainer> container);

    // Utility methods
    static ContainerExtractionMethod string_to_method(const std::string& method_str);
    static std::string method_to_string(ContainerExtractionMethod method);

    template <ExtractableContainerData T>
    std::vector<T> extract_typed_data(
        std::shared_ptr<Kakshya::SignalSourceContainer> container,
        const std::optional<Kakshya::Region>& region)
    {

        const auto data_variant = region ? container->get_region_data(*region) : container->get_processed_data();

        auto extracted = Kakshya::extract_from_variant<T>(data_variant);
        if (!extracted) {
            throw std::runtime_error("Failed to extract data as requested type");
        }

        return *extracted;
    }

    template <typename T>
    ExtractorOutput create_typed_output(T&& data)
    {
        if constexpr (std::is_same_v<std::decay_t<T>, Kakshya::DataVariant>) {
            return ExtractorOutput { std::forward<T>(data) };
        } else if constexpr (std::is_arithmetic_v<std::decay_t<T>>) {
            return ExtractorOutput { std::vector<double> { static_cast<double>(data) } };
        } else {
            return ExtractorOutput { Kakshya::DataVariant { std::vector<double> {} } };
        }
    }

    std::shared_ptr<Kakshya::ContiguousAccessProcessor> m_contiguous_processor;
    std::shared_ptr<Kakshya::RegionOrganizationProcessor> m_region_processor;

    mutable std::unordered_map<std::string, std::any> m_extraction_cache;
    mutable std::shared_ptr<Kakshya::SignalSourceContainer> m_container;

    ExtractorOutput extract_parametric_region_data(std::shared_ptr<Kakshya::SignalSourceContainer> container,
        const std::string& extraction_type);
};
}
