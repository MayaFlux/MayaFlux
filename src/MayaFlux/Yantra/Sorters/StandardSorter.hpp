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
class StandardSorter : public UniversalSorter<InputType, OutputType> {
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
        // Use strategy pattern for different sorting approaches
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
                auto algorithm_enum = Utils::string_to_enum_case_insensitive<SortingAlgorithm>(*str_result.value);
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
        if constexpr (std::same_as<InputType, OutputType>) {
            auto result = input;
            result.data = sort_data_copy(input.data);
            return result;
        } else {
            // Handle type conversion case
            return convert_and_sort(input);
        }
    }

    /**
     * @brief In-place sorting (modifies input)
     */
    output_type sort_in_place(const input_type& input)
    {
        if constexpr (std::same_as<InputType, OutputType>) {
            auto result = input;
            sort_data_in_place(result.data);
            return result;
        } else {
            // For type conversion, fall back to copy sort
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
            result.data = generate_compute_data_indices(input.data, this->get_direction());
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
            result.data = sort_data_partial(input.data);
            return result;
        } else {
            return convert_and_sort(input);
        }
    }

    /**
     * @brief Chunked sorting for large datasets
     */
    output_type sort_chunked(const input_type& input)
    {
        if constexpr (std::same_as<OutputType, std::vector<InputType>>) {
            // Return chunks as separate items
            auto chunks = sort_data_chunked(input.data);
            output_type result {};
            result.data = chunks;
            result.metadata = input.metadata;
            result.metadata["sort_type"] = "chunked";
            result.metadata["chunk_count"] = chunks.size();
            return result;
        } else {
            // Merge chunks back into single result
            auto chunks = sort_data_chunked(input.data);
            return merge_chunks_to_result(chunks, input);
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
    InputType sort_data_copy(const InputType& data)
    {
        return sort_compute_data_extract(data, this->get_direction(), m_algorithm);
    }

    /**
     * @brief Sort data in-place
     */
    void sort_data_in_place(InputType& data)
    {
        sort_compute_data_inplace(data, this->get_direction(), m_algorithm);
    }

    /**
     * @brief Partial sorting implementation
     */
    InputType sort_data_partial(const InputType& data)
    {
        auto old_algorithm = m_algorithm;
        m_algorithm = SortingAlgorithm::PARTIAL;
        auto result = sort_compute_data_extract(data, this->get_direction(), m_algorithm);
        m_algorithm = old_algorithm;
        return result;
    }

    /**
     * @brief Chunked sorting implementation
     */
    std::vector<InputType> sort_data_chunked(const InputType& data)
    {
        if constexpr (SortableContainerType<InputType>) {
            return MayaFlux::Yantra::sort_chunked(data, m_chunk_size, this->get_direction(), m_algorithm);
        } else {
            // For non-container types, return single "chunk"
            return { sort_data_copy(data) };
        }
    }

    /**
     * @brief Merge chunks back to single result
     */
    output_type merge_chunks_to_result(const std::vector<InputType>& chunks, const input_type& original_input)
    {
        if constexpr (std::same_as<InputType, OutputType> && SortableContainerType<InputType>) {
            // Merge sorted chunks
            InputType merged;
            for (const auto& chunk : chunks) {
                merged.insert(merged.end(), chunk.begin(), chunk.end());
            }

            output_type result {};
            result.data = merged;
            result.metadata = original_input.metadata;
            result.metadata["sort_type"] = "chunked_merged";
            return result;
        } else {
            // If can't merge, return first chunk or empty
            output_type result {};
            if (!chunks.empty()) {
                if constexpr (std::same_as<InputType, OutputType>) {
                    result.data = chunks[0];
                }
            }
            result.metadata = original_input.metadata;
            return result;
        }
    }

    /**
     * @brief Handle type conversion scenarios
     */
    output_type convert_and_sort(const input_type& input)
    {
        // Use OperationHelper for type conversion if needed
        // This is a simplified version - you'd integrate with your actual OperationHelper
        output_type result {};
        result.metadata = input.metadata;
        result.metadata["conversion_applied"] = true;

        // TODO: Implement actual conversion logic using OperationHelper
        // For now, just indicate conversion is needed
        result.metadata["needs_conversion"] = true;

        return result;
    }

    /**
     * @brief Validate input type for sorting
     */
    bool validate_input_type(const InputType& data) const
    {
        if constexpr (std::same_as<InputType, Kakshya::DataVariant>) {
            return is_data_variant_sortable(data);
        } else if constexpr (SortableContainerType<InputType>) {
            return !data.empty();
        } else if constexpr (EigenSortable<InputType>) {
            return data.size() > 0;
        } else {
            return true; // Accept other types optimistically
        }
    }
};

// ===== Convenience Specializations =====

/// Standard sorter for DataVariant
using StandardDataSorter = StandardSorter<Kakshya::DataVariant>;

/// Standard sorter for numeric vectors (DataVariant contains these)
template <typename T>
using StandardVectorSorter = StandardSorter<std::vector<T>>;

/// Standard sorter for region groups (proper way to handle multiple regions)
using StandardRegionGroupSorter = StandardSorter<Kakshya::RegionGroup>;

/// Standard sorter for region segments
using StandardSegmentSorter = StandardSorter<std::vector<Kakshya::RegionSegment>>;

/// Standard sorter for Eigen matrices
using StandardMatrixSorter = StandardSorter<Eigen::MatrixXd>;

/// Standard sorter for Eigen vectors
using StandardVectorSorterEigen = StandardSorter<Eigen::VectorXd>;

/// Standard sorter that generates indices
template <ComputeData InputType = Kakshya::DataVariant>
using StandardIndexSorter = StandardSorter<InputType, std::vector<size_t>>;

} // namespace MayaFlux::Yantra
