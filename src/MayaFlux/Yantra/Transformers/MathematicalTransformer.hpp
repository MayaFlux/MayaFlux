#pragma once

#include "MayaFlux/Kinesis/Discrete/Transform.hpp"
#include "MayaFlux/Nodes/Generators/Polynomial.hpp"
#include "Transformer.hpp"

namespace MayaFlux::Yantra {

namespace D = MayaFlux::Kinesis::Discrete;

/**
 * @enum MathematicalOperation
 * @brief Pointwise maps applied by MathematicalTransformer
 */
enum class MathematicalOperation : uint8_t {
    GAIN, ///< Linear gain/attenuation
    OFFSET, ///< DC offset
    POWER, ///< Power function
    LOGARITHMIC, ///< Logarithmic transform
    EXPONENTIAL, ///< Exponential transform
    TRIGONOMETRIC, ///< Trigonometric functions
    QUANTIZE, ///< Quantization/bit reduction
    NORMALIZE, ///< Normalization
    POLYNOMIAL ///< Polynomial transform
};

/**
 * @enum TrigFunction
 * @brief Trigonometric function used by the TRIGONOMETRIC operation
 */
enum class TrigFunction : uint8_t {
    SIN,
    COS,
    TAN
};

/**
 * @struct MathematicalTransformerConfig
 * @brief Settings of MathematicalTransformer, stored whole in the operation's facets.
 */
struct MathematicalTransformerConfig : TransformerConfig {
    static constexpr auto describe_facets()
    {
        return std::tuple_cat(
            TransformerConfig::describe_facets(),
            std::tuple {
                FacetMember<&MathematicalTransformerConfig::operation> {},
                FacetMember<&MathematicalTransformerConfig::gain_factor> {},
                FacetMember<&MathematicalTransformerConfig::offset_value> {},
                FacetMember<&MathematicalTransformerConfig::exponent> {},
                FacetMember<&MathematicalTransformerConfig::log_scale> {},
                FacetMember<&MathematicalTransformerConfig::log_input_scale> {},
                FacetMember<&MathematicalTransformerConfig::log_offset> {},
                FacetMember<&MathematicalTransformerConfig::log_base> {},
                FacetMember<&MathematicalTransformerConfig::exp_scale> {},
                FacetMember<&MathematicalTransformerConfig::exp_rate> {},
                FacetMember<&MathematicalTransformerConfig::exp_base> {},
                FacetMember<&MathematicalTransformerConfig::trig_function> {},
                FacetMember<&MathematicalTransformerConfig::frequency> {},
                FacetMember<&MathematicalTransformerConfig::amplitude> {},
                FacetMember<&MathematicalTransformerConfig::phase> {},
                FacetMember<&MathematicalTransformerConfig::bits> {},
                FacetMember<&MathematicalTransformerConfig::target_peak> {},
                FacetMember<&MathematicalTransformerConfig::coefficients> {},
            });
    }

    /** Pointwise map to apply. */
    Reflect::EnumByte<MathematicalOperation> operation { MathematicalOperation::GAIN };
    /** GAIN: multiplier. */
    double gain_factor { 1.0 };
    /** OFFSET: added constant. */
    double offset_value { 0.0 };
    /** POWER: exponent. */
    double exponent { 2.0 };
    /** LOGARITHMIC: output scale. */
    double log_scale { 1.0 };
    /** LOGARITHMIC: input scale applied before the offset. */
    double log_input_scale { 1.0 };
    /** LOGARITHMIC: offset added to the scaled input. */
    double log_offset { 1.0 };
    /** LOGARITHMIC: logarithm base. */
    double log_base { std::numbers::e };
    /** EXPONENTIAL: output scale. */
    double exp_scale { 1.0 };
    /** EXPONENTIAL: rate applied to the input. */
    double exp_rate { 1.0 };
    /** EXPONENTIAL: base of the exponential. */
    double exp_base { std::numbers::e };
    /** TRIGONOMETRIC: function applied. */
    Reflect::EnumByte<TrigFunction> trig_function { TrigFunction::SIN };
    /** TRIGONOMETRIC: frequency scaling of the input. */
    double frequency { 1.0 };
    /** TRIGONOMETRIC: amplitude of the output. */
    double amplitude { 1.0 };
    /** TRIGONOMETRIC: phase offset of the input. */
    double phase { 0.0 };
    /** QUANTIZE: bit depth. */
    uint8_t bits { 16 };
    /** NORMALIZE: peak of the output range, which is [-target_peak, target_peak]. */
    double target_peak { 1.0 };
    /** POLYNOMIAL: coefficients, highest order first, as Polynomial::power_sum evaluates them. */
    std::vector<double> coefficients { 0.0, 1.0 };
};

/**
 * @class MathematicalTransformer
 * @brief Applies one pointwise map to every channel, as set by its config.
 */
template <ComputeData InputType = std::vector<Kakshya::DataVariant>, ComputeData OutputType = InputType>
class MathematicalTransformer final : public Transformer<InputType, OutputType> {
public:
    using input_type = Datum<InputType>;
    using output_type = Datum<OutputType>;
    using supports = Supports<MathematicalTransformerConfig>;

    /**
     * @brief Construct a mathematical transformer from its settings.
     * @param config Operation and its parameters.
     */
    explicit MathematicalTransformer(MathematicalTransformerConfig config = {})
    {
        this->set_config(std::move(config));
    }

    [[nodiscard]] TransformationType get_transformation_type() const override
    {
        return TransformationType::MATHEMATICAL;
    }

    /**
     * @brief The stored settings, or defaults when none were set.
     */
    [[nodiscard]] ConfigRefs configs() const override
    {
        return this->stored_configs(supports {});
    }

protected:
    output_type transform_implementation(input_type& input) override
    {
        const auto& cfg = this->template config<MathematicalTransformerConfig>();
        switch (static_cast<MathematicalOperation>(cfg.operation)) {
        case MathematicalOperation::GAIN: {
            const auto a = cfg.gain_factor;
            return apply_per_channel(input, [a](std::span<double> ch) {
                D::linear(ch, a, 0.0);
            });
        }
        case MathematicalOperation::OFFSET: {
            const auto b = cfg.offset_value;
            return apply_per_channel(input, [b](std::span<double> ch) {
                D::linear(ch, 1.0, b);
            });
        }
        case MathematicalOperation::POWER: {
            const auto exp = cfg.exponent;
            return apply_per_channel(input, [exp](std::span<double> ch) {
                D::power(ch, exp);
            });
        }
        case MathematicalOperation::LOGARITHMIC: {
            const auto a = cfg.log_scale;
            const auto b = cfg.log_input_scale;
            const auto c = cfg.log_offset;
            const auto base = cfg.log_base;
            return apply_per_channel(input, [a, b, c, base](std::span<double> ch) {
                D::logarithmic(ch, a, b, c, base);
            });
        }
        case MathematicalOperation::EXPONENTIAL: {
            const auto a = cfg.exp_scale;
            const auto b = cfg.exp_rate;
            const auto base = cfg.exp_base;
            return apply_per_channel(input, [a, b, base](std::span<double> ch) {
                D::exponential(ch, a, b, base);
            });
        }
        case MathematicalOperation::TRIGONOMETRIC: {
            const auto freq = cfg.frequency;
            const auto amp = cfg.amplitude;
            const auto ph = cfg.phase;
            switch (static_cast<TrigFunction>(cfg.trig_function)) {
            case TrigFunction::COS:
                return apply_per_channel(input, [freq, amp, ph](std::span<double> ch) {
                    D::apply_trig(ch, [](double x) { return std::cos(x); }, freq, amp, ph);
                });
            case TrigFunction::TAN:
                return apply_per_channel(input, [freq, amp, ph](std::span<double> ch) {
                    D::apply_trig(ch, [](double x) { return std::tan(x); }, freq, amp, ph);
                });
            case TrigFunction::SIN:
                break;
            }
            return apply_per_channel(input, [freq, amp, ph](std::span<double> ch) {
                D::apply_trig(ch, [](double x) { return std::sin(x); }, freq, amp, ph);
            });
        }
        case MathematicalOperation::QUANTIZE: {
            const auto bits = cfg.bits;
            return apply_per_channel(input, [bits](std::span<double> ch) {
                D::quantize(ch, bits);
            });
        }
        case MathematicalOperation::NORMALIZE: {
            const auto peak = cfg.target_peak;
            return apply_per_channel(input, [peak](std::span<double> ch) {
                D::normalize(ch, -peak, peak);
            });
        }
        case MathematicalOperation::POLYNOMIAL: {
            auto poly = std::make_shared<Nodes::Generator::Polynomial>(cfg.coefficients);
            return apply_per_channel(input, [&poly](std::span<double> ch) {
                std::ranges::transform(ch, ch.begin(),
                    [&poly](double x) { return poly->process_sample(x); });
            });
        }
        default:
            return create_output(input);
        }
    }

private:
    mutable std::vector<std::vector<double>> m_working_buffer;

    template <typename Func>
    output_type apply_per_channel(input_type& input, Func&& func)
    {
        auto [channels, structure_info] = OperationHelper::extract_structured_double(input);
        m_working_buffer.resize(channels.size());
        for (size_t i = 0; i < channels.size(); ++i) {
            m_working_buffer[i].assign(channels[i].begin(), channels[i].end());
            func(std::span<double> { m_working_buffer[i] });
        }
        if (this->is_in_place())
            for (size_t i = 0; i < channels.size(); ++i)
                std::ranges::copy(m_working_buffer[i], channels[i].begin());
        return create_output(
            OperationHelper::reconstruct_from_double<InputType>(m_working_buffer, structure_info));
    }

    output_type create_output(const input_type& input)
    {
        if constexpr (std::is_same_v<InputType, OutputType>) {
            return input;
        } else {
            auto [result_data, metadata] = OperationHelper::extract_structured_double(input);
            return this->convert_result(result_data, metadata);
        }
    }
};

}
