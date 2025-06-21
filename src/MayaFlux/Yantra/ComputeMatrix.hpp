#pragma once

#include "ComputeOperation.hpp"
#include "Extractors/ExtractionHelper.hpp"
#include "Sorters/SorterHelpers.hpp"

/**
 * @namespace MayaFlux::Yantra
 * @brief Computational framework for data processing and transformation operations
 */
namespace MayaFlux::Yantra {

/**
 * @brief Universal input/output types that encompass all operation types
 * This replaces std::any with a proper variant system
 */
using UniversalInput = std::variant<
    AnalyzerInput,
    SorterInput,
    ExtractorInput>;

using UniversalOutput = std::variant<
    AnalyzerOutput,
    SorterOutput,
    ExtractorOutput>;

/**
 * @class VariantOperationWrapper
 * @brief Wraps specific operation types to work with universal variant system
 * This replaces TypeErasedOperationWrapper with a cleaner approach
 */
template <typename SpecificInput, typename SpecificOutput>
class VariantOperationWrapper : public ComputeOperation<UniversalInput, UniversalOutput> {
private:
    std::shared_ptr<ComputeOperation<SpecificInput, SpecificOutput>> m_wrapped;

public:
    explicit VariantOperationWrapper(std::shared_ptr<ComputeOperation<SpecificInput, SpecificOutput>> wrapped)
        : m_wrapped(std::move(wrapped))
    {
    }

    UniversalOutput apply_operation(UniversalInput input) override
    {
        try {
            auto specific_input = std::get<SpecificInput>(input);
            auto result = m_wrapped->apply_operation(specific_input);
            return UniversalOutput { result };
        } catch (const std::bad_variant_access& e) {
            throw std::runtime_error(
                std::string("Variant type mismatch: expected ") + typeid(SpecificInput).name() + ", got variant index " + std::to_string(input.index()));
        }
    }

    void set_parameter(const std::string& name, std::any value) override
    {
        m_wrapped->set_parameter(name, value);
    }

    std::any get_parameter(const std::string& name) const override
    {
        return m_wrapped->get_parameter(name);
    }

    std::map<std::string, std::any> get_all_parameters() const override
    {
        return m_wrapped->get_all_parameters();
    }

    bool validate_input(const UniversalInput& input) const override
    {
        try {
            auto specific_input = std::get<SpecificInput>(input);
            return m_wrapped->validate_input(specific_input);
        } catch (const std::bad_variant_access&) {
            return false;
        }
    }

    std::shared_ptr<ComputeOperation<SpecificInput, SpecificOutput>> get_wrapped() const
    {
        return m_wrapped;
    }
};

/**
 * @class ComputeMatrix
 * @brief Central registry and factory for computational operations
 *
 * Focused on operation registration, instantiation, and cross-type pipelines.
 * Chain building is handled by dedicated pipeline classes to avoid circular dependencies.
 */
class ComputeMatrix : public std::enable_shared_from_this<ComputeMatrix> {
public:
    /**
     * @brief Operation type enumeration for proper registration
     */
    enum class OperationType {
        ANALYZER,
        SORTER,
        EXTRACTOR,
        TRANSFORMER
    };

    /**
     * @brief Register an analyzer operation
     */
    template <typename T>
    void register_analyzer(const std::string& name)
    {
        static_assert(std::is_base_of_v<ComputeOperation<AnalyzerInput, AnalyzerOutput>, T>,
            "T must inherit from ComputeOperation<AnalyzerInput, AnalyzerOutput>");

        m_operation_factories[name] = []() -> std::shared_ptr<ComputeOperation<UniversalInput, UniversalOutput>> {
            auto operation = std::make_shared<T>();
            return std::make_shared<VariantOperationWrapper<AnalyzerInput, AnalyzerOutput>>(operation);
        };
        m_operation_types[name] = OperationType::ANALYZER;
    }

    /**
     * @brief Register a sorter operation
     */
    template <typename T>
    void register_sorter(const std::string& name)
    {
        static_assert(std::is_base_of_v<ComputeOperation<SorterInput, SorterOutput>, T>,
            "T must inherit from ComputeOperation<SorterInput, SorterOutput>");

        m_operation_factories[name] = []() -> std::shared_ptr<ComputeOperation<UniversalInput, UniversalOutput>> {
            auto operation = std::make_shared<T>();
            return std::make_shared<VariantOperationWrapper<SorterInput, SorterOutput>>(operation);
        };
        m_operation_types[name] = OperationType::SORTER;
    }

    /**
     * @brief Register an extractor operation
     */
    template <typename T>
    void register_extractor(const std::string& name)
    {
        static_assert(std::is_base_of_v<ComputeOperation<ExtractorInput, ExtractorOutput>, T>,
            "T must inherit from ComputeOperation<ExtractorInput, ExtractorOutput>");

        m_operation_factories[name] = []() -> std::shared_ptr<ComputeOperation<UniversalInput, UniversalOutput>> {
            auto operation = std::make_shared<T>();
            return std::make_shared<VariantOperationWrapper<ExtractorInput, ExtractorOutput>>(operation);
        };
        m_operation_types[name] = OperationType::EXTRACTOR;
    }

    /**
     * @brief Creates an instance of a registered operation type with proper type recovery
     * @tparam T Expected operation class type
     * @param name Identifier of the registered operation type
     * @return Shared pointer to the created operation instance
     */
    template <typename T>
    std::shared_ptr<T> create_operation(const std::string& name)
    {
        auto it = m_operation_factories.find(name);
        if (it == m_operation_factories.end()) {
            return nullptr;
        }

        auto type_it = m_operation_types.find(name);
        if (type_it == m_operation_types.end()) {
            return nullptr;
        }

        auto wrapper = it->second();

        switch (type_it->second) {
        case OperationType::ANALYZER: {
            if constexpr (std::is_base_of_v<ComputeOperation<AnalyzerInput, AnalyzerOutput>, T>) {
                auto analyzer_wrapper = std::dynamic_pointer_cast<VariantOperationWrapper<AnalyzerInput, AnalyzerOutput>>(wrapper);
                if (analyzer_wrapper) {
                    return std::dynamic_pointer_cast<T>(analyzer_wrapper->get_wrapped());
                }
            }
            break;
        }
        case OperationType::SORTER: {
            if constexpr (std::is_base_of_v<ComputeOperation<SorterInput, SorterOutput>, T>) {
                auto sorter_wrapper = std::dynamic_pointer_cast<VariantOperationWrapper<SorterInput, SorterOutput>>(wrapper);
                if (sorter_wrapper) {
                    return std::dynamic_pointer_cast<T>(sorter_wrapper->get_wrapped());
                }
            }
            break;
        }
        case OperationType::EXTRACTOR: {
            if constexpr (std::is_base_of_v<ComputeOperation<ExtractorInput, ExtractorOutput>, T>) {
                auto extractor_wrapper = std::dynamic_pointer_cast<VariantOperationWrapper<ExtractorInput, ExtractorOutput>>(wrapper);
                if (extractor_wrapper) {
                    return std::dynamic_pointer_cast<T>(extractor_wrapper->get_wrapped());
                }
            }
            break;
        }
        }

        return nullptr;
    }

    /**
     * @brief Apply an operation with automatic type routing
     */
    UniversalOutput apply_operation(const std::string& name, UniversalInput input)
    {
        auto it = m_operation_factories.find(name);
        if (it == m_operation_factories.end()) {
            throw std::runtime_error("Operation not found: " + name);
        }

        auto operation = it->second();
        return operation->apply_operation(input);
    }

    /**
     * @brief Create two-stage cross-type pipeline with automatic conversion
     * This is the main pipeline creation method - handles cross-type compatibility
     */
    template <typename FirstOp, typename SecondOp>
    auto create_cross_type_pipeline(const std::string& first_name, const std::string& second_name)
    {
        auto first = create_operation<FirstOp>(first_name);
        auto second = create_operation<SecondOp>(second_name);

        if (!first || !second) {
            throw std::runtime_error("Failed to create pipeline operations");
        }

        return [first, second, this](auto input) {
            auto intermediate = first->apply_operation(input);
            auto converted_input = convert_between_types(intermediate);
            return second->apply_operation(converted_input);
        };
    }

    /**
     * @brief Create three-stage pipeline
     */
    template <typename FirstOp, typename SecondOp, typename ThirdOp>
    auto create_triple_pipeline(const std::string& first_name,
        const std::string& second_name,
        const std::string& third_name)
    {
        auto first = create_operation<FirstOp>(first_name);
        auto second = create_operation<SecondOp>(second_name);
        auto third = create_operation<ThirdOp>(third_name);

        if (!first || !second || !third) {
            throw std::runtime_error("Failed to create pipeline operations");
        }

        return [first, second, third, this](auto input) {
            auto intermediate1 = first->apply_operation(input);
            auto converted1 = convert_between_types(intermediate1);

            auto intermediate2 = second->apply_operation(converted1);
            auto converted2 = convert_between_types(intermediate2);

            return third->apply_operation(converted2);
        };
    }

    /**
     * @brief Processes a batch of inputs through the same operation
     * @tparam InputType Type of the operation inputs
     * @tparam OutputType Type of the operation outputs
     * @param operation_id Identifier for the operation
     * @param inputs Collection of input data to process
     * @return Collection of corresponding output results
     */
    template <typename InputType, typename OutputType>
    std::vector<OutputType> process_batch(const std::string& operation_id,
        const std::vector<InputType>& inputs)
    {
        std::vector<OutputType> results;
        results.reserve(inputs.size());

        auto it = m_operation_factories.find(operation_id);
        if (it == m_operation_factories.end()) {
            throw std::runtime_error("Operation not found: " + operation_id);
        }

        auto operation = it->second();

        for (const auto& input : inputs) {
            UniversalInput universal_input = convert_to_universal_input(input);
            auto universal_output = operation->apply_operation(universal_input);
            auto specific_output = extract_specific_output<OutputType>(universal_output);
            results.push_back(specific_output);
        }

        return results;
    }

    /**
     * @brief Cached operation execution
     */
    template <typename InputType, typename OutputType>
    OutputType get_cached_result(const std::string& operation_id, const InputType& input)
    {
        std::string cache_key = operation_id + "_" + std::to_string(std::hash<InputType> {}(input));

        auto cached_result = find_in_cache<OutputType>(cache_key);
        if (cached_result) {
            return *cached_result;
        }

        // Use the universal system for execution
        UniversalInput universal_input = convert_to_universal_input(input);
        auto universal_output = apply_operation(operation_id, universal_input);
        auto result = extract_specific_output<OutputType>(universal_output);

        cache_result(cache_key, result);
        return result;
    }

    /**
     * @brief Get singleton instance
     */
    static std::shared_ptr<ComputeMatrix> get_instance()
    {
        static std::shared_ptr<ComputeMatrix> instance(new ComputeMatrix());
        return instance;
    }

private:
    std::map<std::string, std::function<std::shared_ptr<ComputeOperation<UniversalInput, UniversalOutput>>()>> m_operation_factories;
    std::map<std::string, OperationType> m_operation_types;

    std::unordered_map<std::string, std::any> m_result_cache;

    /**
     * @brief Type conversion helpers
     */
    template <typename InputType>
    UniversalInput convert_to_universal_input(const InputType& input)
    {
        if constexpr (std::same_as<InputType, AnalyzerInput>) {
            return UniversalInput { input };
        } else if constexpr (std::same_as<InputType, SorterInput>) {
            return UniversalInput { input };
        } else if constexpr (std::same_as<InputType, ExtractorInput>) {
            return UniversalInput { input };
        } else {
            static_assert(std::same_as<InputType, void>, "Unsupported input type");
        }
    }

    template <typename OutputType>
    OutputType extract_specific_output(const UniversalOutput& output)
    {
        if constexpr (std::same_as<OutputType, AnalyzerOutput>) {
            return std::get<AnalyzerOutput>(output);
        } else if constexpr (std::same_as<OutputType, SorterOutput>) {
            return std::get<SorterOutput>(output);
        } else if constexpr (std::same_as<OutputType, ExtractorOutput>) {
            return std::get<ExtractorOutput>(output);
        } else {
            static_assert(std::same_as<OutputType, void>, "Unsupported output type");
        }
    }

    /**
     * @brief Cross-type conversion logic
     * Fixed: Handle multiple AnalyzerOutput conversion cases properly
     */
    template <typename OutputType>
    auto convert_between_types(const OutputType& output)
    {
        if constexpr (std::same_as<OutputType, AnalyzerOutput>) {
            // For now, assume we want ExtractorInput - you can add logic to determine this
            return ExtractorInput { output };
        } else if constexpr (std::same_as<OutputType, ExtractorOutput>) {
            // Convert ExtractorOutput to SorterInput by extracting the base output
            return SorterInput { output.base_output }; // Assuming SorterInput can take BaseExtractorOutput
        } else {
            return output; // No conversion needed
        }
    }

    /** @brief Mutex for thread-safe cache access */
    std::mutex m_cache_mutex;

    /**
     * @brief Attempts to find a cached result
     * @tparam T Expected type of the cached result
     * @param key Cache lookup key
     * @return Optional containing the cached value if found
     */
    template <typename T>
    std::optional<T> find_in_cache(const std::string& key)
    {
        std::lock_guard<std::mutex> lock(m_cache_mutex);
        auto it = m_result_cache.find(key);
        if (it != m_result_cache.end()) {
            try {
                return std::any_cast<T>(it->second);
            } catch (const std::bad_any_cast&) {
                return std::nullopt;
            }
        }
        return std::nullopt;
    }

    /**
     * @brief Stores a result in the cache
     * @tparam T Type of the result to cache
     * @param key Cache storage key
     * @param result Value to cache
     */
    template <typename T>
    void cache_result(const std::string& key, const T& result)
    {
        std::lock_guard<std::mutex> lock(m_cache_mutex);
        m_result_cache[key] = result;
    }
};

// Convenience macros
#define REGISTER_ANALYZER(Matrix, Name, Type) \
    Matrix->register_analyzer<Type>(Name)

#define REGISTER_SORTER(Matrix, Name, Type) \
    Matrix->register_sorter<Type>(Name)

#define REGISTER_EXTRACTOR(Matrix, Name, Type) \
    Matrix->register_extractor<Type>(Name)

} // namespace MayaFlux::Yantra
