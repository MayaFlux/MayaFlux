#pragma once

#include "MayaFlux/EnumUtils.hpp"
#include "MayaFlux/Yantra/ComputeOperation.hpp"

/**
 * @file UniversalTransformer.hpp
 * @brief Modern, digital-first universal transformation framework for Maya Flux
 *
 * The UniversalTransformer system provides a clean, extensible foundation for data transformation
 * in the Maya Flux ecosystem. Unlike traditional audio transformers limited to analog metaphors,
 * this embraces the digital paradigm: data-driven workflows, multi-modal transformations, and
 * computational possibilities beyond physical analog constraints.
 *
 * ## Core Philosophy
 * A transformer **modifies ComputeData** through digital-first approaches:
 * 1. **Temporal transformations:** Time-stretching, reversing, granular manipulation
 * 2. **Spectral transformations:** Frequency domain processing, spectral morphing, cross-synthesis
 * 3. **Mathematical transformations:** Polynomial mapping, matrix operations, recursive algorithms
 * 4. **Cross-modal transformations:** Audio-to-visual mapping, pattern translation between modalities
 * 5. **Generative transformations:** AI-driven, grammar-based, stochastic transformations
 * 6. **Multi-dimensional transformations:** N-dimensional data manipulation, spatial transformations
 *
 * ## Key Features
 * - **Universal input/output:** Template-based I/O types defined at instantiation
 * - **Type-safe transformation:** C++20 concepts and compile-time guarantees
 * - **Transformation strategies:** In-place, buffered, streaming, recursive
 * - **Composable operations:** Integrates with ComputeMatrix execution modes
 * - **Digital-first design:** Embraces computational possibilities beyond analog metaphors
 *
 * ## Usage Examples
 * ```cpp
 * // Transform DataVariant containing audio/video/texture data
 * auto transformer = std::make_shared<MyTransformer<Kakshya::DataVariant>>();
 *
 * // Transform signal containers with time-stretching
 * auto time_transformer = std::make_shared<MyTransformer<
 *     std::shared_ptr<Kakshya::SignalSourceContainer>,
 *     std::shared_ptr<Kakshya::SignalSourceContainer>>>();
 *
 * // Transform matrices with mathematical operations
 * auto matrix_transformer = std::make_shared<MyTransformer<
 *     Eigen::MatrixXd,
 *     Eigen::MatrixXd>>();
 * ```
 */

namespace MayaFlux::Yantra {

/**
 * @enum TransformationType
 * @brief Categories of transformation operations for discovery and organization
 */
enum class TransformationType : uint8_t {
    TEMPORAL, ///< Time-based transformations (time-stretch, reverse, delay)
    SPECTRAL, ///< Frequency domain transformations (pitch-shift, spectral filtering)
    MATHEMATICAL, ///< Mathematical transformations (polynomial mapping, matrix operations)
    CROSS_MODAL, ///< Cross-modality transformations (audio-to-visual, pattern translation)
    GENERATIVE, ///< AI/ML-driven or algorithmic generation-based transformations
    SPATIAL, ///< Multi-dimensional spatial transformations
    PATTERN_BASED, ///< Pattern recognition and transformation
    RECURSIVE, ///< Recursive/fractal transformations
    GRANULAR, ///< Granular synthesis and micro-temporal transformations
    CONVOLUTION, ///< Convolution-based transformations (impulse response, filters)
    CUSTOM ///< User-defined transformation types
};

/**
 * @enum TransformationStrategy
 * @brief Transformation execution strategies
 */
enum class TransformationStrategy : uint8_t {
    IN_PLACE, ///< Transform data in-place (modifies input)
    BUFFERED, ///< Create transformed copy (preserves input)
    STREAMING, ///< Stream-based transformation for large data
    INCREMENTAL, ///< Progressive transformation with intermediate results
    LAZY, ///< Lazy evaluation transformation (future: coroutines)
    CHUNKED, ///< Transform in chunks for efficient processing
    PARALLEL, ///< Parallel/concurrent transformation
    RECURSIVE ///< Recursive transformation with feedback
};

/**
 * @enum TransformationQuality
 * @brief Quality vs performance trade-off control
 */
enum class TransformationQuality : uint8_t {
    DRAFT, ///< Fast, low-quality transformation for previews
    STANDARD, ///< Balanced quality/performance for real-time use
    HIGH_QUALITY, ///< High-quality transformation, may be slower
    REFERENCE, ///< Maximum quality, computational cost is secondary
    ADAPTIVE ///< Quality adapts based on available computational resources
};

/**
 * @enum TransformationScope
 * @brief Scope control for transformation operations
 */
enum class TransformationScope : uint8_t {
    FULL_DATA, ///< Transform entire data set
    TARGETED_REGIONS, ///< Transform only specific regions
    SELECTIVE_BANDS, ///< Transform specific frequency/spatial bands
    CONDITIONAL ///< Transform based on dynamic conditions
};

/**
 * @struct TransformationKey
 * @brief Multi-dimensional transformation key specification for complex transformations
 */
struct TransformationKey {
    std::string name;
    std::function<double(const std::any&)> parameter_extractor; ///< Extract parameter value from data
    double intensity = 1.0; ///< Transformation intensity/amount
    double weight = 1.0; ///< Weight for multi-key transformations
    bool normalize = false; ///< Normalize parameters before transformation

    TransformationKey(std::string n, std::function<double(const std::any&)> e)
        : name(std::move(n))
        , parameter_extractor(std::move(e))
    {
    }
};

/**
 * @class UniversalTransformer
 * @brief Template-flexible transformer base with instance-defined I/O types
 *
 * The UniversalTransformer provides a clean, concept-based foundation for all transformation
 * operations. I/O types are defined at instantiation time, providing maximum flexibility
 * while maintaining type safety through C++20 concepts.
 *
 * Unlike traditional transformers that only handle simple signal processing, this embraces the
 * digital paradigm with multi-modal transformation, cross-domain operations, and computational
 * approaches that go beyond analog metaphors.
 */
template <ComputeData InputType = Kakshya::DataVariant, ComputeData OutputType = InputType>
class UniversalTransformer : public ComputeOperation<InputType, OutputType> {
public:
    using input_type = IO<InputType>;
    using output_type = IO<OutputType>;
    using base_type = ComputeOperation<InputType, OutputType>;

    virtual ~UniversalTransformer() = default;

    /**
     * @brief Gets the transformation type category for this transformer
     * @return TransformationType enum value
     */
    [[nodiscard]] virtual TransformationType get_transformation_type() const = 0;

    /**
     * @brief Gets human-readable name for this transformer
     * @return String identifier for the transformer
     */
    [[nodiscard]] std::string get_name() const override
    {
        return get_transformer_name();
    }

    /**
     * @brief Type-safe parameter management with transformation-specific defaults
     */
    void set_parameter(const std::string& name, std::any value) override
    {
        if (name == "strategy") {
            auto strategy_result = safe_any_cast<TransformationStrategy>(value);
            if (strategy_result) {
                m_strategy = *strategy_result.value;
                return;
            }
            auto str_result = safe_any_cast<std::string>(value);
            if (str_result) {
                auto strategy_enum = Utils::string_to_enum_case_insensitive<TransformationStrategy>(*str_result.value);
                if (strategy_enum) {
                    m_strategy = *strategy_enum;
                    return;
                }
            }
        }
        if (name == "quality") {
            auto quality_result = safe_any_cast<TransformationQuality>(value);
            if (quality_result) {
                m_quality = *quality_result.value;
                return;
            }
            auto str_result = safe_any_cast<std::string>(value);
            if (str_result) {
                auto quality_enum = Utils::string_to_enum_case_insensitive<TransformationQuality>(*str_result.value);
                if (quality_enum) {
                    m_quality = *quality_enum;
                    return;
                }
            }
        }
        if (name == "scope") {
            auto scope_result = safe_any_cast<TransformationScope>(value);
            if (scope_result) {
                m_scope = *scope_result.value;
                return;
            }
            auto str_result = safe_any_cast<std::string>(value);
            if (str_result) {
                auto scope_enum = Utils::string_to_enum_case_insensitive<TransformationScope>(*str_result.value);
                if (scope_enum) {
                    m_scope = *scope_enum;
                    return;
                }
            }
        }
        set_transformation_parameter(name, std::move(value));
    }

    [[nodiscard]] std::any get_parameter(const std::string& name) const override
    {
        if (name == "strategy") {
            return m_strategy;
        }
        if (name == "quality") {
            return m_quality;
        }
        if (name == "scope") {
            return m_scope;
        }
        return get_transformation_parameter(name);
    }

    [[nodiscard]] std::map<std::string, std::any> get_all_parameters() const override
    {
        auto params = get_transformation_parameters();
        params["strategy"] = m_strategy;
        params["quality"] = m_quality;
        params["scope"] = m_scope;
        return params;
    }

    /**
     * @brief Transformation-specific parameter access
     */
    void set_strategy(TransformationStrategy strategy) { m_strategy = strategy; }
    [[nodiscard]] TransformationStrategy get_strategy() const { return m_strategy; }

    void set_quality(TransformationQuality quality) { m_quality = quality; }
    [[nodiscard]] TransformationQuality get_quality() const { return m_quality; }

    void set_scope(TransformationScope scope) { m_scope = scope; }
    [[nodiscard]] TransformationScope get_scope() const { return m_scope; }

    /**
     * @brief Set transformation intensity (0.0 = no transformation, 1.0 = full transformation)
     */
    void set_intensity(double intensity)
    {
        m_intensity = std::clamp(intensity, 0.0, 2.0); // Allow > 1.0 for extreme transformations
    }
    [[nodiscard]] double get_intensity() const { return m_intensity; }

    /**
     * @brief Add transformation key for multi-dimensional transformations
     */
    void add_transformation_key(const TransformationKey& key)
    {
        m_transformation_keys.push_back(key);
    }

    /**
     * @brief Clear all transformation keys
     */
    void clear_transformation_keys()
    {
        m_transformation_keys.clear();
    }

    /**
     * @brief Get all transformation keys
     */
    [[nodiscard]] const std::vector<TransformationKey>& get_transformation_keys() const
    {
        return m_transformation_keys;
    }

    /**
     * @brief Set a custom transformation function for mathematical transformations
     */
    template <typename T>
    void set_custom_function(std::function<T(const T&)> func)
    {
        m_custom_function = [func](const std::any& a) -> std::any {
            auto val_result = safe_any_cast<T>(a);
            if (val_result) {
                return func(*val_result);
            }
            return a; // Return unchanged if cast fails
        };
    }

    /**
     * @brief Indicates whether the transformation modifies the input data directly
     * @return True if the operation modifies input in-place, false if it creates a new output
     */
    [[nodiscard]] virtual bool is_in_place() const { return m_strategy == TransformationStrategy::IN_PLACE; }

    /**
     * @brief Reports the current progress of a long-running transformation
     * @return Progress value between 0.0 (not started) and 1.0 (completed)
     */
    [[nodiscard]] virtual double get_transformation_progress() const { return 1.0; }

    /**
     * @brief Estimates the computational cost of the transformation
     * @return Relative cost factor (1.0 = standard, >1.0 = more expensive, <1.0 = cheaper)
     */
    [[nodiscard]] virtual double estimate_computational_cost() const { return 1.0; }

protected:
    /**
     * @brief Core operation implementation - called by ComputeOperation interface
     * @param input Input data with metadata
     * @return Output data with metadata
     */
    output_type operation_function(const input_type& input) override
    {
        auto raw_result = transform_implementation(const_cast<input_type&>(input));
        return apply_scope_and_quality_processing(raw_result);
    }

    /**
     * @brief Pure virtual transformation implementation - derived classes implement this
     * @param input Input data with metadata
     * @return Raw transformation output before scope/quality processing
     */
    virtual output_type transform_implementation(input_type& input) = 0;

    /**
     * @brief Get transformer-specific name (derived classes override this)
     * @return Transformer name string
     */
    [[nodiscard]] virtual std::string get_transformer_name() const { return "UniversalTransformer"; }

    /**
     * @brief Transformation-specific parameter handling (override for custom parameters)
     */
    virtual void set_transformation_parameter(const std::string& name, std::any value)
    {
        m_parameters[name] = std::move(value);
    }

    [[nodiscard]] virtual std::any get_transformation_parameter(const std::string& name) const
    {
        auto it = m_parameters.find(name);
        return (it != m_parameters.end()) ? it->second : std::any {};
    }

    [[nodiscard]] virtual std::map<std::string, std::any> get_transformation_parameters() const
    {
        return m_parameters;
    }

    /**
     * @brief Apply scope and quality filtering to the transformation result
     */
    virtual output_type apply_scope_and_quality_processing(const output_type& result)
    {
        // Base implementation just returns the result
        // Derived classes can override for specific scope/quality processing
        return result;
    }

    /**
     * @brief Helper method to apply transformation keys to extract parameters
     */
    std::vector<double> extract_transformation_parameters(const input_type& input) const
    {
        std::vector<double> parameters;
        parameters.reserve(m_transformation_keys.size());

        for (const auto& key : m_transformation_keys) {
            try {
                double param = key.parameter_extractor(input.data);
                if (key.normalize) {
                    param = std::clamp(param, 0.0, 1.0);
                }
                parameters.push_back(param * key.intensity * key.weight);
            } catch (...) {
                parameters.push_back(0.0); // Default on extraction failure
            }
        }

        return parameters;
    }

private:
    /** @brief Core transformation configuration */
    TransformationStrategy m_strategy = TransformationStrategy::BUFFERED;
    TransformationQuality m_quality = TransformationQuality::STANDARD;
    TransformationScope m_scope = TransformationScope::FULL_DATA;
    double m_intensity = 1.0;

    /** @brief Multi-dimensional transformation keys */
    std::vector<TransformationKey> m_transformation_keys;

    /** @brief Custom transformation function for mathematical operations */
    std::function<std::any(const std::any&)> m_custom_function;

    /** @brief Generic parameter storage for transformer-specific settings */
    std::map<std::string, std::any> m_parameters;
};

} // namespace MayaFlux::Yantra
