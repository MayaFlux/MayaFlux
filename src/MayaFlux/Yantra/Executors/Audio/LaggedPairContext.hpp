#pragma once

#include <bit>

#include "MayaFlux/Yantra/Executors/ShaderSpecBinding.hpp"
#include "MayaFlux/Yantra/Executors/WordPairExecutionContext.hpp"

#include "SegmentGeometry.hpp"

/**
 * @file LaggedPairContext.hpp
 * @brief Lagged pairwise sums of doubles on the GPU, exactly, with no fp64 on the device.
 */

namespace MayaFlux::Yantra {

/**
 * @class LaggedPairContext
 * @brief One sum per window and lag, as one dispatch of the LaggedPair template.
 *
 * For window w and lag l the result is the sum over i of map(x[i], x[i + l]),
 * with i + l inside the window, where x is the window's elements. Sums run in
 * the template's emulated IEEE double arithmetic on word pairs, so a result
 * agrees with a host sum up to the order the partial sums are combined in. A
 * centred context subtracts each window's mean from both operands first; the
 * mean is the window sum times the double 1 / length, which may differ from a
 * host quotient in the last bit. A lag of the window length or more sums nothing.
 *
 * Both the lag count and the window count are dispatch dimensions, so each is
 * limited to the device's maximum workgroup count, 65535 at least.
 */
class LaggedPairContext : public WordPairExecutionContext {
public:
    /**
     * @param map Pairwise map summed over the window.
     * @param centred True to remove each window's mean before the map.
     * @param workgroup Invocations per sum. A power of two from 2 to 1024.
     */
    explicit LaggedPairContext(
        Portal::Graphics::PairMap map = Portal::Graphics::PairMap::Product,
        bool centred = false,
        uint32_t workgroup = 256)
        : LaggedPairContext(lagged_pair_spec(map, centred, workgroup))
    {
    }

    /**
     * @brief Sum every window at every lag in a range.
     * @param values Doubles the windows index into.
     * @param geometry Window length, stride, first offset and count. Every window must lie inside values.
     * @param first_lag Shortest lag.
     * @param lag_count Number of consecutive lags.
     * @return geometry.count * lag_count doubles, window major, empty when either count is zero.
     */
    [[nodiscard]] std::vector<double> sums(
        std::span<const double> values, const SegmentGeometry& geometry, uint32_t first_lag, uint32_t lag_count)
    {
        if (geometry.count == 0 || lag_count == 0) {
            return {};
        }
        stage(values, geometry, first_lag, lag_count);
        execute(Datum<std::vector<Kakshya::DataVariant>> {}, ExecutionContext {});

        const auto result = primary_values();
        return { result.begin(), result.end() };
    }

    /**
     * @brief Stage values and geometry for the next dispatch without running it.
     *
     * Follow with dispatch_async and collect_result, then decode_primary.
     */
    void stage(std::span<const double> values, const SegmentGeometry& geometry, uint32_t first_lag, uint32_t lag_count)
    {
        m_windows = geometry.count;
        m_lags = lag_count;

        stage_words(0, values);
        set_output_size(1, static_cast<size_t>(geometry.count) * lag_count * sizeof(uint64_t));

        const double inverse_length = geometry.length > 0 ? 1.0 / static_cast<double>(geometry.length) : 0.0;
        const auto scale_bits = std::bit_cast<uint64_t>(inverse_length);
        push(Push {
            .length = geometry.length,
            .stride = geometry.stride,
            .offset = geometry.offset,
            .first_lag = first_lag,
            .lag_count = lag_count,
            .scale_lo = static_cast<uint32_t>(scale_bits & 0xFFFFFFFFU),
            .scale_hi = static_cast<uint32_t>(scale_bits >> 32U) });
    }

    /**
     * @brief The spec of the template instance this context drives.
     */
    [[nodiscard]] static Portal::Graphics::ShaderSpec lagged_pair_spec(
        Portal::Graphics::PairMap map, bool centred, uint32_t workgroup)
    {
        using Portal::Graphics::BindingDirection;
        return Portal::Graphics::ShaderSpec::Assemble {}
            .tmpl(Portal::Graphics::KernelTemplate::LaggedPair)
            .pair_map(map)
            .centred(centred)
            .ssbo("values", BindingDirection::Input, Kakshya::GpuDataFormat::VEC2_U32)
            .ssbo("result", BindingDirection::Output, Kakshya::GpuDataFormat::VEC2_U32)
            .pc("length", Kakshya::GpuDataFormat::UINT32)
            .pc("stride", Kakshya::GpuDataFormat::UINT32)
            .pc("offset", Kakshya::GpuDataFormat::UINT32)
            .pc("first_lag", Kakshya::GpuDataFormat::UINT32)
            .pc("lag_count", Kakshya::GpuDataFormat::UINT32)
            .pc("scale_lo", Kakshya::GpuDataFormat::UINT32)
            .pc("scale_hi", Kakshya::GpuDataFormat::UINT32)
            .workgroup(workgroup)
            .build();
    }

protected:
    /**
     * @brief One workgroup per lag and window.
     */
    [[nodiscard]] std::array<uint32_t, 3> calculate_dispatch_size(
        size_t /*total_elements*/, const DataStructureInfo& /*structure_info*/) const override
    {
        return { m_lags, m_windows, 1U };
    }

private:
    struct Push {
        uint32_t length;
        uint32_t stride;
        uint32_t offset;
        uint32_t first_lag;
        uint32_t lag_count;
        uint32_t scale_lo;
        uint32_t scale_hi;
    };

    explicit LaggedPairContext(const Portal::Graphics::ShaderSpec& spec)
        : WordPairExecutionContext(config_from_spec(spec), bindings_from_spec(spec), false, "LaggedPairContext")
    {
    }

    uint32_t m_windows { 1 };
    uint32_t m_lags { 1 };
};

} // namespace MayaFlux::Yantra
