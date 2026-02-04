#pragma once

#include "SortingHelper.hpp"
#include "UniversalSorter.hpp"

/**
 * @file StandardSorter.hpp
 * @brief Example concrete implementation of UniversalSorter
 *
 * Demonstrates how to implement a concrete sorter that works with the modern
 * concept-based architecture. This sorter handles standard comparison-based
 * sorting for various data types.
 */

namespace MayaFlux::Yantra {

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
 */
template <ComputeData InputType = Kakshya::DataVariant, ComputeData OutputType = InputType>
class MAYAFLUX_API StandardSorter : public UniversalSorter<InputType, OutputType> {
public:
    using base_type = UniversalSorter<InputType, OutputType>;
    using input_type = typename base_type::input_type;
    using output_type = typename base_type::output_type;

    /**
     * @brief Constructor with default configuration
     */
    StandardSorter()
    {
        this->set_direction(SortingDirection::ASCENDING);
        this->set_strategy(SortingStrategy::COPY_SORT);
        this->set_granularity(SortingGranularity::RAW_DATA);
    }

    /**
     * @brief Get sorting type category
     */
    [[nodiscard]] SortingType get_sorting_type() const override
    {
        return SortingType::STANDARD;
    }

    /**
     * @brief Configure sorting algorithm
     */
    void set_algorithm(SortingAlgorithm algorithm) { m_algorithm = algorithm; }
    [[nodiscard]] SortingAlgorithm get_algorithm() const { return m_algorithm; }

protected:
    /**
     * @brief Get sorter name
     */
    [[nodiscard]] std::string get_sorter_name() const override
    {
        return "StandardSorter";
    }

    /**
     * @brief Main sorting implementation with type dispatch
     */
    output_type sort_implementation(const input_type& input) override
    {
        switch (this->get_strategy()) {
        case SortingStrategy::IN_PLACE:
            return sort_in_place(input);
        case SortingStrategy::COPY_SORT:
            return sort_copy(input);
        case SortingStrategy::INDEX_ONLY:
            return sort_indices_only(input);
        case SortingStrategy::PARTIAL_SORT:
            return sort_partial(input);
        case SortingStrategy::CHUNKED_SORT:
            return sort_chunked(input);
        case SortingStrategy::PARALLEL_SORT:
            return sort_parallel(input);
        default:
            return sort_copy(input);
        }
    }

    /**
     * @brief Input validation
     */
    bool validate_sorting_input(const input_type& input) const override
    {
        return validate_input_type(input.data);
    }

    /**
     * @brief Custom parameter handling
     */
    void set_sorting_parameter(const std::string& name, std::any value) override
    {
        if (name == "algorithm") {
            if (auto alg_result = safe_any_cast<SortingAlgorithm>(value)) {
                m_algorithm = *alg_result.value;
                return;
            }
            if (auto str_result = safe_any_cast<std::string>(value)) {
                auto algorithm_enum = Reflect::string_to_enum_case_insensitive<SortingAlgorithm>(*str_result.value);
                if (algorithm_enum) {
                    m_algorithm = *algorithm_enum;
                    return;
                }
            }
        }
        if (name == "chunk_size") {
            if (auto size_result = safe_any_cast<size_t>(value)) {
                m_chunk_size = *size_result.value;
                return;
            }
        }
        base_type::set_sorting_parameter(name, std::move(value));
    }

    [[nodiscard]] std::any get_sorting_parameter(const std::string& name) const override
    {
        if (name == "algorithm") {
            return m_algorithm;
        }
        if (name == "chunk_size") {
            return m_chunk_size;
        }
        return base_type::get_sorting_parameter(name);
    }

private:
    SortingAlgorithm m_algorithm = SortingAlgorithm::STANDARD;
    size_t m_chunk_size = 1024;

    /**
     * @brief Copy-based sorting (preserves input)
     */
    output_type sort_copy(const input_type& input)
    {
        try {
            std::vector<std::vector<double>> working_buffer;
            auto [working_spans, structure_info] = OperationHelper::setup_operation_buffer(
                const_cast<input_type&>(input), working_buffer);

            sort_channels_inplace(working_spans, this->get_direction(), m_algorithm);

            output_type result = this->convert_result(working_buffer, structure_info);
            result.metadata = input.metadata;
            result.container = input.container;
            result.metadata["sort_type"] = "copy";

            return result;

        } catch (const std::exception& e) {
            output_type error_result;
            error_result.metadata = input.metadata;
            error_result.container = input.container;
            error_result.metadata["error"] = std::string("Sorting failed: ") + e.what();
            return error_result;
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
            return result;
        } else {
            return sort_copy(input);
        }
    }

    /**
     * @brief Generate sort indices only
     */
    output_type sort_indices_only(const input_type& input)
    {
        if constexpr (std::same_as<OutputType, std::vector<size_t>>) {
            auto result = output_type {};
            result.data = generate_compute_data_indices(input, this->get_direction());
            result.metadata = input.metadata;
            result.metadata["sort_type"] = "indices_only";
            return result;
        } else {
            // If output type is not vector<size_t>, fall back to copy sort
            return sort_copy(input);
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
            return result;
        } else {
            return convert_and_sort(input);
        }
    }

    /**
     * @brief Parallel sorting
     */
    output_type sort_parallel(const input_type& input)
    {
        auto old_algorithm = m_algorithm;
        m_algorithm = SortingAlgorithm::PARALLEL;
        auto result = sort_copy(input);
        m_algorithm = old_algorithm;
        result.metadata["sort_type"] = "parallel";
        return result;
    }

    // ===== Type-specific sorting implementations =====

    /**
     * @brief Sort data with copy semantics
     */
    InputType sort_data_copy(const input_type& data)
    {
        return sort_compute_data_extract(data, this->get_direction(), m_algorithm);
    }

    /**
     * @brief Sort data in-place
     */
    void sort_data_in_place(input_type& data)
    {
        sort_compute_data_inplace(data, this->get_direction(), m_algorithm);
    }

    /**
     * @brief Partial sorting implementation
     */
    InputType sort_data_partial(const input_type& data)
    {
        auto old_algorithm = m_algorithm;
        m_algorithm = SortingAlgorithm::PARTIAL;
        auto result = sort_compute_data_extract(data, this->get_direction(), m_algorithm);
        m_algorithm = old_algorithm;
        return result;
    }

    std::vector<InputType> extract_chunked_data(std::vector<std::span<double>> channels, DataStructureInfo info)
    {
        std::vector<InputType> chunks;

        for (size_t start = 0; start < channels[0].size(); start += m_chunk_size) {
            std::vector<std::vector<double>> chunk_data;
            chunk_data.resize(channels.size());

            for (size_t ch = 0; ch < channels.size(); ++ch) {
                size_t end = std::min(start + m_chunk_size, channels[ch].size());
                auto chunk_span = channels[ch].subspan(start, end - start);

                chunk_data[ch].assign(chunk_span.begin(), chunk_span.end());
                sort_span_inplace(std::span<double>(chunk_data[ch]), this->get_direction(), m_algorithm);
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
        result.metadata["sort_type"] = "chunked_merged";

        if constexpr (std::same_as<InputType, OutputType>) {
            if (chunks.empty()) {
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
                result.metadata["sort_type"] = "chunked_merged";

            } catch (...) {
                result.data = chunks[0];
            }
        }

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

        sort_channels_inplace(working_spans, this->get_direction(), SortingAlgorithm::PARTIAL);
        return this->convert_result(working_buffer, structure_info);
    }

    /**
     * @brief Validate input type for sorting
     */
    bool validate_input_type(const input_type& data) const
    {
        if constexpr (RequiresContainer<InputType>) {
            return data.has_container();
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

/// Standard sorter for region groups (proper way to handle multiple regions)
using StandardRegionGroupSorter = StandardSorter<Kakshya::RegionGroup>;

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
