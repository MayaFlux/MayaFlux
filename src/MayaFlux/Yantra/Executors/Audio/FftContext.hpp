#pragma once

#include <bit>

#include "MayaFlux/Yantra/Executors/ShaderSpecBinding.hpp"
#include "MayaFlux/Yantra/Executors/WordPairExecutionContext.hpp"

#include "SegmentGeometry.hpp"

/**
 * @file FftContext.hpp
 * @brief Batched FFT of doubles on the GPU, exactly, with no fp64 on the device.
 */

namespace MayaFlux::Yantra {

/**
 * @class FftContext
 * @brief One FFT per window, as one dispatch of the FFT template.
 *
 * Windows are read from the input at a length, stride and offset given in
 * complex elements, multiplied by a window function and zero padded to the
 * point count. The butterflies run in the template's emulated IEEE double
 * arithmetic on word pairs, with twiddle factors computed here in double, so a
 * result agrees with a host transform to rounding. An inverse context uses the
 * conjugate twiddles and scales its output by one over the point count.
 *
 * Complex output is points bins per window as interleaved re, im. Power output
 * is points / 2 + 1 bins per window of re * re + im * im, which is the
 * non-redundant half for a real signal. A projection context takes rows of
 * weights, one per power bin, and returns one value per row and window: the
 * weights summed against the power spectrum, so band energies, spectral
 * moments and filterbank outputs leave the device as a few doubles per window.
 *
 * The point count is a power of two from 2 to 2048. The input must hold at
 * least one complex element.
 */
class FftContext : public WordPairExecutionContext {
public:
    /**
     * @param points Points per window.
     * @param output Complex bins or the power spectrum.
     * @param inverse True for the inverse transform, scaled by 1 / points.
     */
    explicit FftContext(
        uint32_t points,
        Portal::Graphics::SpectrumOutput output = Portal::Graphics::SpectrumOutput::Complex,
        bool inverse = false)
        : FftContext(fft_spec(points, output), points, output, inverse)
    {
    }

    /**
     * @brief A projecting context.
     * @param points Points per window.
     * @param weights Rows of points / 2 + 1 weights, row major. The row count is weights.size() divided by that.
     */
    FftContext(uint32_t points, std::span<const double> weights)
        : FftContext(
              fft_spec(points, Portal::Graphics::SpectrumOutput::Projection, row_count(points, weights.size())),
              points, Portal::Graphics::SpectrumOutput::Projection, false, weights)
    {
    }

    [[nodiscard]] uint32_t points() const noexcept { return m_points; }

    /**
     * @brief Rows of weights of a projecting context, zero otherwise.
     */
    [[nodiscard]] uint32_t descriptors() const noexcept { return m_descriptors; }

    /**
     * @brief Doubles in the result of one window.
     */
    [[nodiscard]] size_t values_per_window() const noexcept
    {
        if (m_projection) {
            return m_descriptors;
        }
        return m_power ? static_cast<size_t>(m_points) / 2 + 1 : static_cast<size_t>(m_points) * 2;
    }

    /**
     * @brief Transform windows of a real signal.
     * @param real Samples the windows index into.
     * @param geometry Window length, stride, first offset and count, in samples.
     * @param window Coefficients applied to each window, points of them. Empty is rectangular.
     * @return values_per_window() doubles per window, empty when there are no windows.
     */
    [[nodiscard]] std::vector<double> transform(
        std::span<const double> real, const SegmentGeometry& geometry, std::span<const double> window = {})
    {
        std::vector<double> interleaved(real.size() * 2, 0.0);
        for (size_t i = 0; i < real.size(); ++i) {
            interleaved[2 * i] = real[i];
        }
        return transform_complex(interleaved, geometry, window);
    }

    /**
     * @brief Transform windows of a complex signal.
     * @param interleaved Re, im pairs the windows index into.
     * @param geometry Window length, stride, first offset and count, in complex elements.
     * @param window Coefficients applied to each window, points of them. Empty is rectangular.
     */
    [[nodiscard]] std::vector<double> transform_complex(
        std::span<const double> interleaved, const SegmentGeometry& geometry, std::span<const double> window = {})
    {
        if (geometry.count == 0) {
            return {};
        }
        stage(interleaved, geometry, window);
        execute(Datum<std::vector<Kakshya::DataVariant>> {}, ExecutionContext {});

        const auto result = primary_values();
        return { result.begin(), result.end() };
    }

    /**
     * @brief Stage input, window and geometry for the next dispatch without running it.
     *
     * Follow with dispatch_async and collect_result, then decode_primary.
     */
    void stage(std::span<const double> interleaved, const SegmentGeometry& geometry, std::span<const double> window = {})
    {
        m_windows = geometry.count;

        std::vector<double> coefficients(m_points, window.empty() ? 1.0 : 0.0);
        std::copy_n(window.begin(), std::min<size_t>(window.size(), m_points), coefficients.begin());

        stage_words(0, interleaved);
        stage_words(1, m_twiddle);
        stage_words(2, coefficients);
        if (m_projection) {
            stage_words(4, m_weights);
        }
        set_output_size(3, static_cast<size_t>(geometry.count) * values_per_window() * sizeof(uint64_t));

        const auto scale_bits = std::bit_cast<uint64_t>(m_scale);
        push(Push {
            .length = geometry.length,
            .stride = geometry.stride,
            .offset = geometry.offset,
            .scale_lo = static_cast<uint32_t>(scale_bits & 0xFFFFFFFFU),
            .scale_hi = static_cast<uint32_t>(scale_bits >> 32U) });
    }

    /**
     * @brief The spec of the template instance this context drives.
     */
    [[nodiscard]] static Portal::Graphics::ShaderSpec fft_spec(
        uint32_t points, Portal::Graphics::SpectrumOutput output, uint32_t descriptors = 0)
    {
        using Portal::Graphics::BindingDirection;
        auto assemble = Portal::Graphics::ShaderSpec::Assemble {};
        assemble.tmpl(Portal::Graphics::KernelTemplate::FFT)
            .points(points)
            .spectrum(output)
            .descriptors(descriptors)
            .ssbo("values", BindingDirection::Input, Kakshya::GpuDataFormat::VEC2_U32)
            .ssbo("twiddle", BindingDirection::Input, Kakshya::GpuDataFormat::VEC2_U32)
            .ssbo("window", BindingDirection::Input, Kakshya::GpuDataFormat::VEC2_U32)
            .ssbo("result", BindingDirection::Output, Kakshya::GpuDataFormat::VEC2_U32);
        if (output == Portal::Graphics::SpectrumOutput::Projection) {
            assemble.ssbo("weights", BindingDirection::Input, Kakshya::GpuDataFormat::VEC2_U32);
        }
        return assemble
            .pc("length", Kakshya::GpuDataFormat::UINT32)
            .pc("stride", Kakshya::GpuDataFormat::UINT32)
            .pc("offset", Kakshya::GpuDataFormat::UINT32)
            .pc("scale_lo", Kakshya::GpuDataFormat::UINT32)
            .pc("scale_hi", Kakshya::GpuDataFormat::UINT32)
            .workgroup(std::max<uint32_t>(std::min<uint32_t>(points / 2, 256U), 1U))
            .build();
    }

protected:
    /**
     * @brief One workgroup per window.
     */
    [[nodiscard]] std::array<uint32_t, 3> calculate_dispatch_size(
        size_t /*total_elements*/, const DataStructureInfo& /*structure_info*/) const override
    {
        return { m_windows, 1U, 1U };
    }

private:
    struct Push {
        uint32_t length;
        uint32_t stride;
        uint32_t offset;
        uint32_t scale_lo;
        uint32_t scale_hi;
    };

    static uint32_t row_count(uint32_t points, size_t weights) noexcept
    {
        const size_t bins = static_cast<size_t>(points) / 2 + 1;
        return weights != 0 && weights % bins == 0 ? static_cast<uint32_t>(weights / bins) : 0U;
    }

    FftContext(const Portal::Graphics::ShaderSpec& spec, uint32_t points,
        Portal::Graphics::SpectrumOutput output, bool inverse, std::span<const double> weights = {})
        : WordPairExecutionContext(config_from_spec(spec), bindings_from_spec(spec), false, "FftContext")
        , m_points(points)
        , m_power(output == Portal::Graphics::SpectrumOutput::Power)
        , m_projection(output == Portal::Graphics::SpectrumOutput::Projection)
        , m_descriptors(spec.descriptors)
        , m_scale(inverse && points > 0 ? 1.0 / static_cast<double>(points) : 1.0)
        , m_weights(weights.begin(), weights.end())
    {
        const uint32_t half = points / 2;
        m_twiddle.reserve(static_cast<size_t>(half) * 2);
        for (uint32_t k = 0; k < half; ++k) {
            const double angle = 2.0 * std::numbers::pi * static_cast<double>(k) / static_cast<double>(points);
            m_twiddle.push_back(std::cos(angle));
            m_twiddle.push_back(inverse ? std::sin(angle) : -std::sin(angle));
        }
    }

    uint32_t m_points;
    bool m_power;
    bool m_projection;
    uint32_t m_descriptors;
    double m_scale;
    std::vector<double> m_weights;
    uint32_t m_windows { 1 };
    std::vector<double> m_twiddle;
};

} // namespace MayaFlux::Yantra
