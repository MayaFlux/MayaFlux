#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"
#include "MayaFlux/Kinesis/Discrete/Convolution.hpp"
#include "Transformer.hpp"

namespace MayaFlux::Yantra {

namespace D = MayaFlux::Kinesis::Discrete;

/**
 * @enum ConvolutionOperation
 * @brief Convolution-family operations applied by ConvolutionTransformer
 */
enum class ConvolutionOperation : uint8_t {
    DIRECT_CONVOLUTION, ///< Standard convolution
    CROSS_CORRELATION, ///< Cross-correlation
    MATCHED_FILTER, ///< Matched filtering
    DECONVOLUTION, ///< Deconvolution
    AUTO_CORRELATION ///< Auto-correlation
};

/**
 * @struct ConvolutionTransformerConfig
 * @brief Settings of ConvolutionTransformer, stored whole in the operation's facets.
 */
struct ConvolutionTransformerConfig : TransformerConfig {
    /** Convolution-family operation to apply. */
    Reflect::EnumByte<ConvolutionOperation> operation { ConvolutionOperation::DIRECT_CONVOLUTION };
    /** DIRECT_CONVOLUTION and DECONVOLUTION: impulse response. */
    std::vector<double> impulse_response { 1.0 };
    /** CROSS_CORRELATION: template signal. */
    std::vector<double> template_signal { 1.0 };
    /** MATCHED_FILTER: reference signal. */
    std::vector<double> reference_signal { 1.0 };
    /** CROSS_CORRELATION and AUTO_CORRELATION: normalise the result to its peak. */
    bool normalize { true };
    /** DECONVOLUTION: Tikhonov regularisation factor. */
    double regularization { 1e-6 };
};

MF_DESCRIBE(ConvolutionTransformerConfig)

/**
 * @class ConvolutionTransformer
 * @brief Applies one convolution-family operation to every channel, as set by its config.
 */
template <ComputeData InputType = std::vector<Kakshya::DataVariant>, ComputeData OutputType = InputType>
class ConvolutionTransformer final : public Transformer<InputType, OutputType> {
public:
    using input_type = Datum<InputType>;
    using output_type = Datum<OutputType>;
    using supports = Supports<ConvolutionTransformerConfig>;

    /**
     * @brief Construct a convolution transformer from its settings.
     * @param config Operation and its parameters.
     */
    explicit ConvolutionTransformer(ConvolutionTransformerConfig config = {})
    {
        this->set_config(std::move(config));
    }

    [[nodiscard]] TransformationType get_transformation_type() const override
    {
        return TransformationType::CONVOLUTION;
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
        const auto& cfg = this->template config<ConvolutionTransformerConfig>();
        switch (static_cast<ConvolutionOperation>(cfg.operation)) {
        case ConvolutionOperation::DIRECT_CONVOLUTION: {
            const auto& ir = cfg.impulse_response;
            return apply_per_channel(input, [&ir](std::span<const double> ch) {
                return D::convolve(ch, ir);
            });
        }
        case ConvolutionOperation::CROSS_CORRELATION: {
            const auto& tmpl = cfg.template_signal;
            const bool norm = cfg.normalize;
            return apply_per_channel(input, [&tmpl, norm](std::span<const double> ch) {
                return D::cross_correlate(ch, tmpl, norm);
            });
        }
        case ConvolutionOperation::MATCHED_FILTER: {
            const auto& ref = cfg.reference_signal;
            return apply_per_channel(input, [&ref](std::span<const double> ch) {
                return D::matched_filter(ch, ref);
            });
        }
        case ConvolutionOperation::DECONVOLUTION: {
            const auto& ir = cfg.impulse_response;
            const double reg = cfg.regularization;
            return apply_per_channel(input, [&ir, reg](std::span<const double> ch) {
                return D::deconvolve(ch, ir, reg);
            });
        }
        case ConvolutionOperation::AUTO_CORRELATION: {
            const bool norm = cfg.normalize;
            return apply_per_channel(input, [norm](std::span<const double> ch) {
                return D::auto_correlate(ch, norm);
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
#include "ConvolutionTransformer.describe.hpp"
