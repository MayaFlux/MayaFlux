#pragma once

#include "Sorter.hpp"

/**
 * @file StandardSorter.hpp
 * @brief Example concrete implementation of Sorter
 *
 * Demonstrates how to implement a concrete sorter that works with the modern
 * concept-based architecture. This sorter handles standard comparison-based
 * sorting for various data types.
 */

namespace MayaFlux::Yantra {

/**
 * @enum SortPath
 * @brief Which sorting path produced a result.
 */
enum class SortPath : uint8_t {
    COPY, ///< Sorted copy of the input
    IN_PLACE, ///< Sorted the input's own data
    INDICES_ONLY, ///< Sort indices only
    PARTIAL, ///< Top-K partial sort
    PARALLEL, ///< Parallel sort
    CHUNKED_MERGED ///< Sorted per chunk, then merged
};

/**
 * @struct StandardSorterConfig
 * @brief Settings for standard sorting.
 *
 * Stored whole on the operation. Adds the chunk size used by chunked sorting
 * to the settings every sorter family accepts.
 */
struct StandardSorterConfig : SorterConfig {
    static constexpr auto describe_facets()
    {
        return std::tuple_cat(
            SorterConfig::describe_facets(),
            std::tuple {
                FacetMember<&StandardSorterConfig::algorithm> {},
                FacetMember<&StandardSorterConfig::direction> {},
                FacetMember<&StandardSorterConfig::strategy> {},
                FacetMember<&StandardSorterConfig::comparator> {},
                FacetMember<&StandardSorterConfig::chunk_size> {},
            });
    }

    /** Sorting algorithm to use, as a SortingAlgorithm value. */
    Reflect::EnumByte<SortingAlgorithm> algorithm { SortingAlgorithm::STANDARD };
    /** Order of the sorted result. CUSTOM takes its order from comparator. */
    SortingDirection direction { SortingDirection::ASCENDING };
    /** Comparator used when direction is CUSTOM. Ignored otherwise. */
    Kinesis::Discrete::DoubleComparator comparator;
    /** How the input is handled during sorting. */
    SortingStrategy strategy { SortingStrategy::COPY_SORT };
    /** Number of elements per chunk for chunked sorting. Must be greater than 0. */
    size_t chunk_size { 1024 };
};

/**
 * @struct SortRecord
 * @brief Record of one StandardSorter run, stored on the output's facets.
 */
struct SortRecord {
    static constexpr auto describe_facets()
    {
        return std::tuple {
            FacetMember<&SortRecord::config> {},
            FacetMember<&SortRecord::path> {},
        };
    }

    /** Settings the sort was produced under. */
    StandardSorterConfig config;
    /** The sorting path that produced the result. */
    Reflect::EnumByte<SortPath> path { SortPath::COPY };
};

/**
 * @class StandardSorter
 * @brief Concrete implementation for standard comparison-based sorting
 *
 * This sorter handles most common sorting scenarios using standard algorithms
 * and comparators. It supports:
 * - Numeric containers (vector<double>, vector<float>, etc.)
 * - DataVariant sorting with type dispatch
 * - Region-based sorting by coordinates/duration
 * - Eigen matrix/vector sorting
 * - Complex number sorting by magnitude
 * - Multi-key sorting with configurable weights
 *
 * The result carries a SortRecord on its facets.
 */
template <ComputeData InputType = Kakshya::DataVariant, ComputeData OutputType = InputType>
class StandardSorter : public Sorter<InputType, OutputType> {
public:
    using base_type = Sorter<InputType, OutputType>;
    using input_type = typename base_type::input_type;
    using output_type = typename base_type::output_type;
    using supports = Supports<StandardSorterConfig>;

    static_assert(!std::same_as<InputType, Kakshya::RegionGroup>,
        "StandardSorter does not sort region groups; use RegionKeySorter");

    /**
     * @brief Construct a standard sorter from its settings.
     * @param config Sorting settings, validated before they are stored.
     */
    explicit StandardSorter(StandardSorterConfig config = {})
    {
        validate_config(config);
        this->set_config(std::move(config));
    }

    /**
     * @brief The stored settings.
     */
    [[nodiscard]] ConfigRefs configs() const override
    {
        return this->stored_configs(supports {});
    }

    /**
     * @brief Get sorting type category
     */
    [[nodiscard]] SortingType get_sorting_type() const override
    {
        return SortingType::STANDARD;
    }

protected:
    /**
     * @brief Main sorting implementation with type dispatch
     */
    output_type sort_implementation(const input_type& input) override
    {
        switch (this->template config<StandardSorterConfig>().strategy) {
        case SortingStrategy::IN_PLACE:
            return sort_in_place(input);
        case SortingStrategy::COPY_SORT:
            return sort_copy(input, this->template config<StandardSorterConfig>().algorithm, SortPath::COPY);
        case SortingStrategy::INDEX_ONLY:
            return sort_indices_only(input);
        case SortingStrategy::PARTIAL_SORT:
            return sort_partial(input);
        case SortingStrategy::CHUNKED_SORT:
            return sort_chunked(input);
        case SortingStrategy::PARALLEL_SORT:
            return sort_parallel(input);
        default:
            return sort_copy(input, this->template config<StandardSorterConfig>().algorithm, SortPath::COPY);
        }
    }

    /**
     * @brief Input validation
     */
    bool validate_sorting_input(const input_type& input) const override
    {
        return validate_input_type(input.data);
    }

private:
    /**
     * @brief Rejects settings that would make sorting undefined.
     */
    static void validate_config(const StandardSorterConfig& cfg)
    {
        if (cfg.chunk_size == 0) {
            error<std::invalid_argument>(Journal::Component::Yantra, Journal::Context::ComputeMatrix, std::source_location::current(), "Chunk size must be greater than 0");
        }
    }

    /**
     * @brief The Kinesis sort order: the direction, or the stored comparator for CUSTOM.
     */
    [[nodiscard]] Kinesis::Discrete::SortOrder sort_order() const
    {
        const auto& cfg = this->template config<StandardSorterConfig>();
        if (cfg.direction == SortingDirection::CUSTOM) {
            return Kinesis::Discrete::SortOrder { cfg.comparator };
        }
        return Kinesis::Discrete::SortOrder { cfg.direction };
    }

    /**
     * @brief Stamps the output with the settings and the path that produced it.
     */
    void record_sort(output_type& out, SortPath path) const
    {
        SortRecord record;
        record.config = this->template config<StandardSorterConfig>();
        record.path = path;
        out.facets.set(record);
    }

    /**
     * @brief Copy-based sorting (preserves input)
     */
    output_type sort_copy(const input_type& input, SortingAlgorithm algorithm, SortPath path)
    {
        try {
            std::vector<std::vector<double>> working_buffer;
            auto [working_spans, structure_info] = OperationHelper::setup_operation_buffer(
                const_cast<input_type&>(input), working_buffer);

            Kinesis::Discrete::sort_channels(working_spans, sort_order(), algorithm);

            output_type result = this->convert_result(working_buffer, structure_info);
            result.metadata = input.metadata;
            result.facets = input.facets;
            record_sort(result, path);

            return result;

        } catch (const std::exception& e) {
            MF_ERROR(Journal::Component::Yantra, Journal::Context::ComputeMatrix, "Sorting failed: {}", e.what());
            output_type passthrough;
            passthrough.metadata = input.metadata;
            passthrough.facets = input.facets;
            return passthrough;
        }
    }

    /**
     * @brief In-place sorting (modifies input)
     */
    output_type sort_in_place(const input_type& input)
    {
        if constexpr (std::same_as<InputType, OutputType>) {
            auto result = input;
            sort_data_in_place(result);
            record_sort(result, SortPath::IN_PLACE);
            return result;
        } else {
            return sort_copy(input, this->template config<StandardSorterConfig>().algorithm, SortPath::COPY);
        }
    }

    /**
     * @brief Generate sort indices only
     */
    output_type sort_indices_only(const input_type& input)
    {
        if constexpr (std::same_as<OutputType, std::vector<std::vector<size_t>>>) {
            auto result = output_type {};
            result.data = generate_compute_data_indices(input, sort_order());
            result.metadata = input.metadata;
            result.facets = input.facets;
            record_sort(result, SortPath::INDICES_ONLY);
            return result;
        } else {
            // If output type is not vector<size_t>, fall back to copy sort
            return sort_copy(input, this->template config<StandardSorterConfig>().algorithm, SortPath::COPY);
        }
    }

    /**
     * @brief Partial sorting (top-K elements)
     */
    output_type sort_partial(const input_type& input)
    {
        auto result = input;
        if constexpr (std::same_as<InputType, OutputType>) {
            result.data = sort_data_partial(input);
            record_sort(result, SortPath::PARTIAL);
            return result;
        } else {
            auto converted = convert_and_sort(input);
            converted.facets = input.facets;
            record_sort(converted, SortPath::PARTIAL);
            return converted;
        }
    }

    /**
     * @brief Parallel sorting
     */
    output_type sort_parallel(const input_type& input)
    {
        return sort_copy(input, SortingAlgorithm::PARALLEL, SortPath::PARALLEL);
    }

    // ===== Type-specific sorting implementations =====

    /**
     * @brief Sort data with copy semantics
     */
    InputType sort_data_copy(const input_type& data)
    {
        return sort_compute_data_extract(data, sort_order(), this->template config<StandardSorterConfig>().algorithm);
    }

    /**
     * @brief Sort data in-place
     */
    void sort_data_in_place(input_type& data)
    {
        sort_compute_data_inplace(data, sort_order(), this->template config<StandardSorterConfig>().algorithm);
    }

    /**
     * @brief Partial sorting implementation
     */
    InputType sort_data_partial(const input_type& data)
    {
        return sort_compute_data_extract(data, sort_order(), SortingAlgorithm::PARTIAL);
    }

    std::vector<InputType> extract_chunked_data(std::vector<std::span<double>> channels, DataStructureInfo info)
    {
        std::vector<InputType> chunks;
        const size_t chunk_size = this->template config<StandardSorterConfig>().chunk_size;

        for (size_t start = 0; start < channels[0].size(); start += chunk_size) {
            std::vector<std::vector<double>> chunk_data;
            chunk_data.resize(channels.size());

            for (size_t ch = 0; ch < channels.size(); ++ch) {
                size_t end = std::min(start + chunk_size, channels[ch].size());
                auto chunk_span = channels[ch].subspan(start, end - start);

                chunk_data[ch].assign(chunk_span.begin(), chunk_span.end());
                Kinesis::Discrete::sort_span(std::span<double>(chunk_data[ch]), sort_order(), this->template config<StandardSorterConfig>().algorithm);
            }

            chunks.push_back(OperationHelper::reconstruct_from_double<InputType>(chunk_data, info));
        }

        return chunks;
    }

    /**
     * @brief Chunked sorting implementation
     */
    output_type sort_chunked(const input_type& data)
    {
        try {
            auto [data_span, structure_info] = OperationHelper::extract_structured_double(
                const_cast<input_type&>(data));
            return merge_chunks_to_result(extract_chunked_data(data_span, structure_info), data, structure_info);

        } catch (...) {
            return { sort_data_copy(data) };
        }
    }

    /**
     * @brief Merge chunks back to single result
     */
    output_type merge_chunks_to_result(const std::vector<InputType>& chunks, const input_type& original_input, DataStructureInfo info)
    {
        output_type result {};
        result.metadata = original_input.metadata;
        result.facets = original_input.facets;

        if constexpr (std::same_as<InputType, OutputType>) {
            if (chunks.empty()) {
                record_sort(result, SortPath::CHUNKED_MERGED);
                return result;
            }

            try {
                std::vector<std::vector<double>> merged_channels;

                for (const auto& chunk : chunks) {
                    auto chunk_channels = OperationHelper::extract_numeric_data(chunk);

                    if (merged_channels.empty()) {
                        merged_channels.resize(chunk_channels.size());
                    }

                    for (size_t ch = 0; ch < chunk_channels.size() && ch < merged_channels.size(); ++ch) {
                        merged_channels[ch].insert(merged_channels[ch].end(),
                            chunk_channels[ch].begin(), chunk_channels[ch].end());
                    }
                }

                result = this->convert_result(merged_channels, info);
                result.metadata = original_input.metadata;
                result.facets = original_input.facets;

            } catch (...) {
                result.data = chunks[0];
            }
        }

        record_sort(result, SortPath::CHUNKED_MERGED);
        return result;
    }

    /**
     * @brief Handle type conversion scenarios
     */
    output_type convert_and_sort(const input_type& input)
    {
        std::vector<std::vector<double>> working_buffer;
        auto [working_spans, structure_info] = OperationHelper::setup_operation_buffer(
            const_cast<input_type&>(input), working_buffer);

        Kinesis::Discrete::sort_channels(working_spans, sort_order(), SortingAlgorithm::PARTIAL);
        return this->convert_result(working_buffer, structure_info);
    }

    /**
     * @brief Validate input type for sorting
     */
    bool validate_input_type(const input_type& data) const
    {
        if constexpr (RequiresContainer<InputType>) {
            return data.get_container() != nullptr;
        }
        return true;
    }
};

// ===== Convenience Specializations =====

/// Standard sorter for DataVariant
using StandardDataSorter = StandardSorter<std::vector<Kakshya::DataVariant>>;

/// Standard sorter for numeric vectors (DataVariant contains these)
template <typename T>
using StandardVectorSorter = StandardSorter<std::vector<std::vector<T>>>;

/// Standard sorter for region segments
using StandardSegmentSorter = StandardSorter<std::vector<Kakshya::RegionSegment>>;

/// Standard sorter for Eigen matrices
using StandardMatrixSorter = StandardSorter<Eigen::MatrixXd>;

/// Standard sorter for Eigen vectors
using StandardVectorSorterEigen = StandardSorter<Eigen::VectorXd>;

/// Standard sorter that generates indices
template <ComputeData InputType = std::vector<Kakshya::DataVariant>>
using StandardIndexSorter = StandardSorter<InputType, std::vector<std::vector<size_t>>>;

} // namespace MayaFlux::Yantra
