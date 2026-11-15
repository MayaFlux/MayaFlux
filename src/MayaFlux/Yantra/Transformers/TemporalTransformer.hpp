#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"
#include "MayaFlux/Kinesis/Discrete/Transform.hpp"
#include "Transformer.hpp"

namespace MayaFlux::Yantra {

namespace D = MayaFlux::Kinesis::Discrete;

/**
 * @enum TemporalOperation
 * @brief Time-domain operations applied by TemporalTransformer
 */
enum class TemporalOperation : uint8_t {
    TIME_REVERSE, ///< Reverse temporal order
    TIME_STRETCH, ///< Change playback speed
    DELAY, ///< Add temporal delay
    FADE_IN_OUT, ///< Apply fade envelope
    SLICE, ///< Extract temporal slice
    INTERPOLATE ///< Temporal interpolation
};

/**
 * @struct TemporalTransformerConfig
 * @brief Settings of TemporalTransformer, stored whole in the operation's facets.
 */
struct TemporalTransformerConfig : TransformerConfig {
    /** Temporal operation to apply. */
    Reflect::EnumByte<TemporalOperation> operation { TemporalOperation::TIME_REVERSE };
    /** TIME_STRETCH: length multiplier, above 1 is slower. */
    double stretch_factor { 1.0 };
    /** DELAY: number of samples prepended. */
    uint32_t delay_samples { 1000 };
    /** DELAY: value used for the prepended samples. */
    double fill_value { 0.0 };
    /** FADE_IN_OUT: fraction of the length used for fade-in. */
    double fade_in_ratio { 0.1 };
    /** FADE_IN_OUT: fraction of the length used for fade-out. */
    double fade_out_ratio { 0.1 };
    /** SLICE: normalised start position. */
    double start_ratio { 0.0 };
    /** SLICE: normalised end position. */
    double end_ratio { 1.0 };
    /** INTERPOLATE: output length in samples. Zero leaves the input unchanged. */
    size_t target_size { 0 };
    /** INTERPOLATE: use cubic rather than linear interpolation. */
    bool use_cubic { false };
};

MF_DESCRIBE(TemporalTransformerConfig)

/**
 * @class TemporalTransformer
 * @brief Applies one time-domain operation to every channel, as set by its config.
 */
template <ComputeData InputType = std::vector<Kakshya::DataVariant>, ComputeData OutputType = InputType>
class TemporalTransformer final : public Transformer<InputType, OutputType> {
public:
    using input_type = Datum<InputType>;
    using output_type = Datum<OutputType>;
    using supports = Supports<TemporalTransformerConfig>;

    /**
     * @brief Construct a temporal transformer from its settings.
     * @param config Operation and its parameters.
     */
    explicit TemporalTransformer(TemporalTransformerConfig config = {})
    {
        this->set_config(std::move(config));
    }

    [[nodiscard]] TransformationType get_transformation_type() const override
    {
        return TransformationType::TEMPORAL;
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
        const auto& cfg = this->template config<TemporalTransformerConfig>();
        switch (static_cast<TemporalOperation>(cfg.operation)) {
        case TemporalOperation::TIME_REVERSE:
            return apply_per_channel(input, [](std::span<double> ch) {
                D::reverse(ch);
            });
        case TemporalOperation::FADE_IN_OUT: {
            const double fi = cfg.fade_in_ratio;
            const double fo = cfg.fade_out_ratio;
            return apply_per_channel(input, [fi, fo](std::span<double> ch) {
                D::fade(ch, fi, fo);
            });
        }
        case TemporalOperation::TIME_STRETCH: {
            const double factor = cfg.stretch_factor;
            return apply_per_channel(input, [factor](std::span<const double> ch) {
                return D::time_stretch(ch, factor);
            });
        }
        case TemporalOperation::DELAY: {
            const uint32_t samples = cfg.delay_samples;
            const double fill = cfg.fill_value;
            return apply_per_channel(input, [samples, fill](std::span<const double> ch) {
                return D::delay(ch, samples, fill);
            });
        }
        case TemporalOperation::SLICE: {
            const double start = cfg.start_ratio;
            const double end = cfg.end_ratio;
            return apply_per_channel(input, [start, end](std::span<const double> ch) {
                return D::slice(ch, start, end);
            });
        }
        case TemporalOperation::INTERPOLATE: {
            const size_t target = cfg.target_size;
            const bool cubic = cfg.use_cubic;
            if (target == 0)
                return create_output(input);
            return apply_per_channel(input, [target, cubic](std::span<const double> ch) {
                std::vector<double> out(target);
                if (cubic)
                    D::interpolate_cubic(ch, out);
                else
                    D::interpolate_linear(ch, out);
                return out;
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
            if constexpr (std::is_void_v<std::invoke_result_t<Func, std::span<double>>>) {
                m_working_buffer[i].assign(channels[i].begin(), channels[i].end());
                func(std::span<double> { m_working_buffer[i] });
            } else {
                m_working_buffer[i] = func(std::span<const double> { channels[i] });
            }
        }
        if (this->is_in_place())
            for (size_t i = 0; i < channels.size(); ++i)
                std::ranges::copy(m_working_buffer[i], channels[i].begin());
        return create_output(
            OperationHelper::reconstruct_from_double<input_type>(m_working_buffer, structure_info));
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
#include "TemporalTransformer.describe.hpp"
