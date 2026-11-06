#pragma once

#include "MayaFlux/Transitive/Reflect/EnumReflect.hpp"
#include "MayaFlux/Yantra/ComputeOperation.hpp"

#include "MayaFlux/Yantra/OperationSpec/OperationHelper.hpp"

/**
 * @file Transformer.hpp
 * @brief Base for transformers. Settings are held in a config struct per family.
 *
 * Each family's config extends TransformerConfig and builds its facet list with
 * std::tuple_cat over the parent's describe_facets(). The config is set through
 * set_config and read through config().
 */

namespace MayaFlux::Yantra {

/**
 * @enum TransformationType
 * @brief Categories of transformation for discovery and organization
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
 * @brief How a transformer handles its input
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
 * @brief Quality level requested from a transformer
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
 * @brief Portion of the data a transformer processes
 */
enum class TransformationScope : uint8_t {
    FULL_DATA, ///< Transform entire data set
    TARGETED_REGIONS, ///< Transform only specific regions
    SELECTIVE_BANDS, ///< Transform specific frequency/spatial bands
    CONDITIONAL ///< Transform based on dynamic conditions
};

/**
 * @struct TransformerConfig
 * @brief Settings every transformer family accepts.
 *
 * Family configs extend this struct and add their own fields.
 */
struct TransformerConfig : OperationConfig {
    static constexpr auto describe_facets()
    {
        return std::tuple {
            FacetMember<&TransformerConfig::strategy> {},
            FacetMember<&TransformerConfig::quality> {},
            FacetMember<&TransformerConfig::scope> {},
            FacetMember<&TransformerConfig::intensity> {},
        };
    }

    /** How the input is handled. */
    TransformationStrategy strategy { TransformationStrategy::BUFFERED };
    /** Requested quality level. */
    TransformationQuality quality { TransformationQuality::STANDARD };
    /** Portion of the data processed. */
    TransformationScope scope { TransformationScope::FULL_DATA };
    /** Amount of transformation, 0.0 to 2.0. */
    double intensity { 1.0 };
};

/**
 * @class Transformer
 * @brief Base of every transformer family.
 *
 * Holds no settings of its own. Derived classes read their family config through config().
 */
template <ComputeData InputType = Kakshya::DataVariant, ComputeData OutputType = InputType>
class Transformer : public ComputeOperation<InputType, OutputType> {
public:
    using input_type = Datum<InputType>;
    using output_type = Datum<OutputType>;
    using base_type = ComputeOperation<InputType, OutputType>;

    virtual ~Transformer() = default;

    /**
     * @brief Gets the transformation type category for this transformer
     * @return TransformationType enum value
     */
    [[nodiscard]] virtual TransformationType get_transformation_type() const = 0;

    [[nodiscard]] std::string get_name() const override
    {
        return get_transformer_name();
    }

    [[nodiscard]] OperationType get_operation_type() const override
    {
        return OperationType::TRANSFORMER;
    }

    /**
     * @brief The stored transformer settings, empty when none were set.
     */
    [[nodiscard]] ConfigRefs configs() const override
    {
        return this->stored_configs(Supports<TransformerConfig> {});
    }

    [[nodiscard]] bool is_in_place() const
    {
        const auto stored = configs();
        return !stored.empty()
            && static_cast<const TransformerConfig&>(stored.front().get()).strategy == TransformationStrategy::IN_PLACE;
    }

protected:
    output_type operation_function(const input_type& input) override
    {
        if (!validate_input(input)) {
            MF_WARN(
                Journal::Component::Yantra,
                Journal::Context::BufferProcessing,
                "{}: input failed validation, passing it through unchanged",
                get_transformer_name());
            return create_safe_output(input);
        }
        auto raw_result = transform_implementation(const_cast<input_type&>(input));
        return apply_scope_and_quality_processing(raw_result);
    }

    virtual output_type transform_implementation(input_type& input) = 0;

    [[nodiscard]] virtual std::string get_transformer_name() const { return "Transformer"; }

    virtual output_type apply_scope_and_quality_processing(const output_type& result)
    {
        return result;
    }

    bool validate_input(const input_type& input) const override
    {
        if constexpr (RequiresContainer<InputType>) {
            if (!input.has_container()) {
                return false;
            }
            auto numeric_data = OperationHelper::extract_numeric_data(input.data, input.container.value());
            return validate_multi_channel_data(numeric_data);
        } else {
            auto numeric_data = OperationHelper::extract_numeric_data(input.data);
            return validate_multi_channel_data(numeric_data);
        }
    }

private:
    output_type create_safe_output(const input_type& input) const
    {
        output_type result;
        result.dimensions = input.dimensions;
        result.modality = input.modality;
        result.metadata = input.metadata;
        result.facets = input.facets;

        if constexpr (std::is_same_v<InputType, OutputType>) {
            result.data = input.data;
        } else {
            if constexpr (RequiresContainer<InputType>) {
                if (!input.has_container()) {
                    result.container = std::make_shared<Kakshya::SignalSourceContainer>();
                } else {
                    result.container = input.container;
                }
            }
            result.data = {};
        }
        return result;
    }

    [[nodiscard]] bool validate_multi_channel_data(const std::vector<std::span<double>>& channels) const
    {
        if (channels.empty()) {
            return false;
        }
        for (const auto& channel : channels) {
            if (channel.empty()) {
                continue;
            }
            for (double sample : channel) {
                if (std::isnan(sample)) {
                    return false;
                }
                if (std::isinf(sample)) {
                    return false;
                }
            }
        }
        return true;
    }
};

}
