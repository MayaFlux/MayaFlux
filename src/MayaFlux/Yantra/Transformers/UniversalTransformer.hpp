#pragma once

#include "MayaFlux/EnumUtils.hpp"
#include "MayaFlux/Yantra/ComputeOperation.hpp"

/**
 * @brief Float Processing Guidelines
 *
 * Transformers support float data processing but with some caveats:
 *
 * 1. **Recommended**: Use double precision for maximum compatibility
 * 2. **Supported**: Float processing works in most environments
 * 3. **Warning**: Mixed float/double processing may cause memory issues
 * 4. **Best Practice**: Stick to one numeric type per transformer instance
 *
 * Example safe usage:
 * ```cpp
 * // Good: Consistent double usage
 * auto transformer = std::make_unique<MathematicalTransformer<>>();
 * std::vector<double> data = {1.0, 2.0, 3.0};
 *
 * // Okay: Consistent float usage (with warning)
 * auto float_transformer = std::make_unique<MathematicalTransformer<>>();
 * std::vector<float> float_data = {1.0f, 2.0f, 3.0f};
 *
 * // Risky: Mixed types (may cause issues)
 * transformer->process(double_data);  // First call
 * transformer->process(float_data);   // Second call - risky!
 * ```
 */

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
enum class TransformationType : u_int8_t {
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
enum class TransformationStrategy : u_int8_t {
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
enum class TransformationQuality : u_int8_t {
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
enum class TransformationScope : u_int8_t {
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
    std::string name; ///< Unique identifier for this transformation key
    std::function<double(const std::any&)> parameter_extractor; ///< Extract parameter value from data
    double intensity = 1.0; ///< Transformation intensity/amount
    double weight = 1.0; ///< Weight for multi-key transformations
    bool normalize = false; ///< Normalize parameters before transformation

    /**
     * @brief Constructs a TransformationKey with name and extractor
     * @param n Name identifier for the key
     * @param e Function to extract parameter values from input data
     */
    TransformationKey(std::string n, std::function<double(const std::any&)> e)
        : name(std::move(n))
        , parameter_extractor(std::move(e))
    {
    }
};

/**
 * @class UniversalTransformer
 * @brief Template-flexible transformer base with instance-defined I/O types
 * @tparam InputType Input data type (defaults to Kakshya::DataVariant)
 * @tparam OutputType Output data type (defaults to InputType)
 *
 * The UniversalTransformer provides a clean, concept-based foundation for all transformation
 * operations. I/O types are defined at instantiation time, providing maximum flexibility
 * while maintaining type safety through C++20 concepts.
 *
 * Unlike traditional transformers that only handle simple signal processing, this embraces the
 * digital paradigm with multi-modal transformation, cross-domain operations, and computational
 * approaches that go beyond analog metaphors.
 *
 * ## Supported Transformation Types
 * - **Temporal**: Time-domain operations (reverse, stretch, delay, fade)
 * - **Spectral**: Frequency-domain operations (pitch shift, filtering, enhancement)
 * - **Mathematical**: Pure mathematical operations (gain, polynomial, trigonometric)
 * - **Convolution**: Convolution-based operations (filtering, correlation, deconvolution)
 * - **Cross-modal**: Operations that translate between different data modalities
 * - **Generative**: AI/ML-driven transformations and procedural generation
 *
 * ## Usage Patterns
 * ```cpp
 * // Basic mathematical transformation
 * auto math_transformer = std::make_unique<MathematicalTransformer<>>();
 * math_transformer->set_parameter("operation", "gain");
 * math_transformer->set_parameter("gain_factor", 2.0);
 *
 * // Spectral processing with custom quality
 * auto spectral_transformer = std::make_unique<SpectralTransformer<>>();
 * spectral_transformer->set_quality(TransformationQuality::HIGH_QUALITY);
 * spectral_transformer->set_strategy(TransformationStrategy::BUFFERED);
 * ```
 */
template <ComputeData InputType = Kakshya::DataVariant, ComputeData OutputType = InputType>
class UniversalTransformer : public ComputeOperation<InputType, OutputType> {
public:
    using input_type = IO<InputType>;
    using output_type = IO<OutputType>;
    using base_type = ComputeOperation<InputType, OutputType>;

    /**
     * @brief Virtual destructor for proper inheritance cleanup
     */
    virtual ~UniversalTransformer() = default;

    /**
     * @brief Gets the transformation type category for this transformer
     * @return TransformationType enum value indicating the category of transformation
     *
     * This is used for transformer discovery, organization, and determining
     * compatibility with different processing pipelines.
     */
    [[nodiscard]] virtual TransformationType get_transformation_type() const = 0;

    /**
     * @brief Gets human-readable name for this transformer
     * @return String identifier for the transformer
     *
     * Implementation delegates to get_transformer_name() which derived classes override.
     * Used for debugging, logging, and user interface display.
     */
    [[nodiscard]] std::string get_name() const override
    {
        return get_transformer_name();
    }

    /**
     * @brief Type-safe parameter management with transformation-specific defaults
     * @param name Parameter name
     * @param value Parameter value (type-erased)
     *
     * Handles core transformer parameters (strategy, quality, scope) and delegates
     * transformer-specific parameters to set_transformation_parameter().
     *
     * Core parameters:
     * - "strategy": TransformationStrategy enum or string
     * - "quality": TransformationQuality enum or string
     * - "scope": TransformationScope enum or string
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

    /**
     * @brief Gets a parameter value by name
     * @param name Parameter name
     * @return Parameter value or empty std::any if not found
     *
     * Handles core transformer parameters and delegates to get_transformation_parameter()
     * for transformer-specific parameters.
     */
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

    /**
     * @brief Gets all parameters as a map
     * @return Map of parameter names to values
     *
     * Combines core transformer parameters with transformer-specific parameters.
     * Useful for serialization, debugging, and parameter inspection.
     */
    [[nodiscard]] std::map<std::string, std::any> get_all_parameters() const override
    {
        auto params = get_transformation_parameters();
        params["strategy"] = m_strategy;
        params["quality"] = m_quality;
        params["scope"] = m_scope;
        return params;
    }

    /**
     * @brief Sets the transformation strategy
     * @param strategy How the transformation should be executed
     *
     * Strategies control memory usage, performance, and behavior:
     * - IN_PLACE: Modify input data directly (memory efficient)
     * - BUFFERED: Create copy for output (preserves input)
     * - STREAMING: Process in chunks for large data
     * - PARALLEL: Use multiple threads where possible
     */
    void set_strategy(TransformationStrategy strategy) { m_strategy = strategy; }

    /**
     * @brief Gets the current transformation strategy
     * @return Current strategy setting
     */
    [[nodiscard]] TransformationStrategy get_strategy() const { return m_strategy; }

    /**
     * @brief Sets the transformation quality level
     * @param quality Quality vs performance trade-off setting
     *
     * Quality levels control the computational cost vs result quality:
     * - DRAFT: Fast processing for previews
     * - STANDARD: Balanced for real-time use
     * - HIGH_QUALITY: Better results, may be slower
     * - REFERENCE: Maximum quality regardless of cost
     */
    void set_quality(TransformationQuality quality) { m_quality = quality; }

    /**
     * @brief Gets the current transformation quality level
     * @return Current quality setting
     */
    [[nodiscard]] TransformationQuality get_quality() const { return m_quality; }

    /**
     * @brief Sets the transformation scope
     * @param scope What portion of the data to transform
     *
     * Scope controls which parts of the input data are processed:
     * - FULL_DATA: Process entire input
     * - TARGETED_REGIONS: Process specific regions only
     * - SELECTIVE_BANDS: Process specific frequency/spatial bands
     * - CONDITIONAL: Process based on dynamic conditions
     */
    void set_scope(TransformationScope scope) { m_scope = scope; }

    /**
     * @brief Gets the current transformation scope
     * @return Current scope setting
     */
    [[nodiscard]] TransformationScope get_scope() const { return m_scope; }

    /**
     * @brief Set transformation intensity (0.0 = no transformation, 1.0 = full transformation)
     * @param intensity Intensity value (clamped to 0.0-2.0 range)
     *
     * Intensity controls how strongly the transformation is applied:
     * - 0.0: No transformation (passthrough)
     * - 1.0: Full transformation as configured
     * - >1.0: Extreme/exaggerated transformation (up to 2.0)
     */
    void set_intensity(double intensity)
    {
        m_intensity = std::clamp(intensity, 0.0, 2.0); // Allow > 1.0 for extreme transformations
    }

    /**
     * @brief Gets the current transformation intensity
     * @return Current intensity value (0.0-2.0)
     */
    [[nodiscard]] double get_intensity() const { return m_intensity; }

    /**
     * @brief Add transformation key for multi-dimensional transformations
     * @param key Transformation key specification
     *
     * Transformation keys enable complex, multi-parameter transformations where
     * different aspects of the input data control different transformation parameters.
     * Useful for data-driven and adaptive transformations.
     */
    void add_transformation_key(const TransformationKey& key)
    {
        m_transformation_keys.push_back(key);
    }

    /**
     * @brief Clear all transformation keys
     *
     * Removes all transformation keys, returning to simple parameter-based operation.
     */
    void clear_transformation_keys()
    {
        m_transformation_keys.clear();
    }

    /**
     * @brief Get all transformation keys
     * @return Vector of current transformation keys
     */
    [[nodiscard]] const std::vector<TransformationKey>& get_transformation_keys() const
    {
        return m_transformation_keys;
    }

    /**
     * @brief Set a custom transformation function for mathematical transformations
     * @tparam T Data type for the custom function
     * @param func Custom transformation function
     *
     * Allows injection of custom transformation logic for specialized use cases.
     * The function will be called for each data element during transformation.
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
     *
     * This is determined by the current transformation strategy. IN_PLACE strategy
     * modifies input directly, while other strategies preserve the input.
     */
    [[nodiscard]] virtual bool is_in_place() const { return m_strategy == TransformationStrategy::IN_PLACE; }

    /**
     * @brief Reports the current progress of a long-running transformation
     * @return Progress value between 0.0 (not started) and 1.0 (completed)
     *
     * Base implementation returns 1.0 (completed). Derived classes can override
     * to provide actual progress reporting for long-running operations.
     */
    [[nodiscard]] virtual double get_transformation_progress() const { return 1.0; }

    /**
     * @brief Estimates the computational cost of the transformation
     * @return Relative cost factor (1.0 = standard, >1.0 = more expensive, <1.0 = cheaper)
     *
     * Base implementation returns 1.0. Derived classes should override to provide
     * realistic cost estimates for scheduling and resource allocation.
     */
    [[nodiscard]] virtual double estimate_computational_cost() const { return 1.0; }

protected:
    /**
     * @brief Core operation implementation - called by ComputeOperation interface
     * @param input Input data with metadata
     * @return Output data with metadata
     *
     * This is the main entry point called by the ComputeOperation framework.
     * It validates input, delegates to transform_implementation(), and applies
     * scope/quality processing to the result.
     */
    output_type operation_function(const input_type& input) override
    {
        if (!validate_input(input)) {
            return create_safe_output(input);
        }
        auto raw_result = transform_implementation(const_cast<input_type&>(input));
        return apply_scope_and_quality_processing(raw_result);
    }

    /**
     * @brief Pure virtual transformation implementation - derived classes implement this
     * @param input Input data with metadata (may be modified for in-place operations)
     * @return Raw transformation output before scope/quality processing
     *
     * This is where derived transformers implement their core transformation logic.
     * The input may be modified for in-place operations. The result will be
     * post-processed based on scope and quality settings.
     */
    virtual output_type transform_implementation(input_type& input) = 0;

    /**
     * @brief Get transformer-specific name (derived classes override this)
     * @return Transformer name string
     *
     * Derived classes should override this to provide meaningful names like
     * "MathematicalTransformer_GAIN" or "SpectralTransformer_PITCH_SHIFT".
     */
    [[nodiscard]] virtual std::string get_transformer_name() const { return "UniversalTransformer"; }

    /**
     * @brief Transformation-specific parameter handling (override for custom parameters)
     * @param name Parameter name
     * @param value Parameter value
     *
     * Base implementation stores parameters in a map. Derived classes should override
     * to handle transformer-specific parameters with proper type checking and validation.
     */
    virtual void set_transformation_parameter(const std::string& name, std::any value)
    {
        m_parameters[name] = std::move(value);
    }

    /**
     * @brief Gets a transformation-specific parameter value
     * @param name Parameter name
     * @return Parameter value or empty std::any if not found
     */
    [[nodiscard]] virtual std::any get_transformation_parameter(const std::string& name) const
    {
        auto it = m_parameters.find(name);
        return (it != m_parameters.end()) ? it->second : std::any {};
    }

    /**
     * @brief Gets all transformation-specific parameters
     * @return Map of transformer-specific parameter names to values
     */
    [[nodiscard]] virtual std::map<std::string, std::any> get_transformation_parameters() const
    {
        return m_parameters;
    }

    /**
     * @brief Apply scope and quality filtering to the transformation result
     * @param result Raw transformation result
     * @return Processed result with scope and quality adjustments applied
     *
     * Base implementation returns the result unchanged. Derived classes can override
     * to implement scope-specific processing (e.g., regional transforms) and
     * quality adjustments (e.g., interpolation quality, precision control).
     */
    virtual output_type apply_scope_and_quality_processing(const output_type& result)
    {
        return result;
    }

    /**
     * @brief Helper method to apply transformation keys to extract parameters
     * @param input Input data to extract parameters from
     * @return Vector of extracted parameter values
     *
     * Processes all transformation keys to extract parameter values from the input data.
     * Applies normalization, intensity, and weight adjustments as configured.
     * Used for data-driven and adaptive transformations.
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
                parameters.push_back(0.0);
            }
        }

        return parameters;
    }

    /**
     * @brief Basic input validation that derived classes can override
     * @param input Input data to validate
     * @return true if input is valid for processing, false otherwise
     *
     * Base implementation checks for:
     * - Non-empty data
     * - Basic data type validity
     * - Finite values (no NaN/infinity in sample data)
     *
     * Derived transformers can override to add specific requirements
     * (e.g., minimum size for spectral operations, specific data structure requirements).
     */
    bool validate_input(const input_type& input) const override
    {
        try {
            auto size = get_input_data_size(input.data);

            if (size == 0) {
                return false;
            }

            if (!is_data_valid(input.data)) {
                return false;
            }

            // TODO: concrete class overrides for
            // - Minimum size for spectral operations
            // - NaN/infinity detection for mathematical operations
            // - etc.

            return true;
        } catch (...) {
            return false;
        }
    }

private:
    /**
     * @brief Creates a safe fallback output when input validation fails
     * @param input Original input (for type/structure reference)
     * @return Safe output that won't cause further processing errors
     *
     * The behavior is:
     * - For same input/output types: return input unchanged
     * - For type conversion: create appropriate empty/minimal output
     * - Always preserves metadata structure
     * - Adds metadata indicating validation failure
     */
    output_type create_safe_output(const input_type& input) const
    {
        output_type result;

        result.dimensions = input.dimensions;
        result.modality = input.modality;
        result.metadata = input.metadata;
        result.metadata["validation_failed"] = true;
        result.metadata["fallback_reason"] = "Input validation failed";

        if constexpr (std::is_same_v<InputType, OutputType>) {
            result.data = input.data;
        } else {
            result.data = create_safe_empty_output<OutputType>();
        }

        return result;
    }

    /**
     * @brief Get the size of input data for validation
     * @param data Input data variant
     * @return Number of elements in the data
     *
     * Handles different data types (variants, containers, scalars) to determine
     * the data size for validation purposes.
     */
    size_t get_input_data_size(const InputType& data) const
    {
        if constexpr (std::is_same_v<InputType, Kakshya::DataVariant>) {
            return std::visit([](const auto& container) -> size_t {
                if constexpr (requires { container.size(); }) {
                    return container.size();
                } else {
                    return 1;
                }
            },
                data);
        } else if constexpr (requires { data.size(); }) {
            return data.size();
        } else {
            return 1;
        }
    }

    /**
     * @brief Check if data contains valid values (not all NaN/inf)
     * @param data Input data to check
     * @return true if data is valid for processing
     *
     * Performs statistical validation to ensure the data contains mostly finite values.
     * Samples a portion of the data for efficiency on large datasets.
     * Handles different numeric types including complex numbers.
     */
    bool is_data_valid(const InputType& data) const
    {
        if constexpr (std::is_same_v<InputType, Kakshya::DataVariant>) {
            return std::visit([](const auto& container) -> bool {
                using ContainerType = std::decay_t<decltype(container)>;
                using ValueType = typename ContainerType::value_type;

                if constexpr (std::is_arithmetic_v<ValueType>) {
                    if (container.empty())
                        return false;

                    size_t valid_count = 0;
                    size_t check_limit = std::min(container.size(), size_t(100));

                    for (size_t i = 0; i < check_limit; ++i) {
                        if (std::isfinite(static_cast<double>(container[i]))) {
                            valid_count++;
                        }
                    }

                    return (valid_count * 2) > check_limit;
                } else if constexpr (std::is_same_v<ValueType, std::complex<float>> || std::is_same_v<ValueType, std::complex<double>>) {
                    if (container.empty())
                        return false;

                    size_t valid_count = 0;
                    size_t check_limit = std::min(container.size(), size_t(100));

                    for (size_t i = 0; i < check_limit; ++i) {
                        double magnitude = std::abs(container[i]);
                        if (std::isfinite(magnitude)) {
                            valid_count++;
                        }
                    }

                    return (valid_count * 2) > check_limit;
                } else {
                    return !container.empty();
                }
            },
                data);
        } else {
            if constexpr (requires { data.size(); }) {
                return data.size() > 0;
            } else {
                return true;
            }
        }
    }

    /**
     * @brief Create safe empty output for type conversion cases
     * @tparam T Output type
     * @return Minimal valid output of type T
     *
     * Creates appropriate empty/default instances for different output types
     * when input validation fails and type conversion is needed.
     */
    template <typename T>
    T create_safe_empty_output() const
    {
        if constexpr (std::is_same_v<T, Kakshya::DataVariant>) {
            return Kakshya::DataVariant { std::vector<double> {} };
        } else if constexpr (std::is_same_v<T, std::vector<double>>) {
            return std::vector<double> {};
        } else if constexpr (std::is_same_v<T, std::vector<float>>) {
            return std::vector<float> {};
        } else if constexpr (std::is_same_v<T, Eigen::VectorXd>) {
            return Eigen::VectorXd {};
        } else if constexpr (std::is_same_v<T, Eigen::MatrixXd>) {
            return Eigen::MatrixXd {};
        } else if constexpr (requires { T {}; }) {
            return T {};
        } else {
            static_assert(std::is_default_constructible_v<T>,
                "Output type must be default constructible for safe fallback");
            return T {};
        }
    }

    /** @brief Core transformation configuration */
    TransformationStrategy m_strategy = TransformationStrategy::BUFFERED; ///< Current execution strategy
    TransformationQuality m_quality = TransformationQuality::STANDARD; ///< Current quality level
    TransformationScope m_scope = TransformationScope::FULL_DATA; ///< Current processing scope
    double m_intensity = 1.0; ///< Transformation intensity (0.0-2.0)

    /** @brief Multi-dimensional transformation keys */
    std::vector<TransformationKey> m_transformation_keys; ///< Keys for data-driven transformations

    /** @brief Custom transformation function for mathematical operations */
    std::function<std::any(const std::any&)> m_custom_function; ///< User-defined transformation function

    /** @brief Generic parameter storage for transformer-specific settings */
    std::map<std::string, std::any> m_parameters; ///< Transformer-specific parameter storage
};

} // namespace MayaFlux::Yantra
