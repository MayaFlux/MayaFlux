#pragma once

#include <bit>

#include "MayaFlux/Yantra/Executors/ShaderSpecBinding.hpp"
#include "MayaFlux/Yantra/Executors/WordPairExecutionContext.hpp"

#include "SegmentGeometry.hpp"

/**
 * @file LaggedPair.hpp
 * @brief Lagged pairwise sums of doubles on the GPU, exactly, with no fp64 on the device.
 *
 * The LaggedPair kernel is run on a plain WordPairExecutionContext. This file
 * holds what is specific to it: the spec and the stage.
 */

namespace MayaFlux::Yantra {

/**
 * @brief The spec of a LaggedPair kernel.
 *
 * For window w and lag l the kernel sums map(x[i], x[i + l]) over the window,
 * with i + l inside it, in the template's emulated IEEE double arithmetic on
 * word pairs. A centred kernel subtracts each window's mean from both operands
 * first; the mean is the window sum divided by the window length, exactly
 * rounded like a host quotient. A lag of the window length or more sums
 * nothing. Bindings: values 0, result 1.
 *
 * @param map Pairwise map summed over the window.
 * @param centred True to remove each window's mean before the map.
 * @param workgroup Invocations per sum. A power of two from 2 to 1024.
 */
[[nodiscard]] inline Portal::Graphics::ShaderSpec lagged_pair_spec(
    Portal::Graphics::PairMap map, bool centred, uint32_t workgroup = 256)
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
        .pc("length_lo", Kakshya::GpuDataFormat::UINT32)
        .pc("length_hi", Kakshya::GpuDataFormat::UINT32)
        .workgroup(workgroup)
        .build();
}

/**
 * @brief The stage that sums every window at every lag in a range, over values already on the device.
 *
 * The values are whatever binding 0 resolves to: a shared buffer of the
 * context, or one imported from another context with import_shared. Nothing is
 * staged from the host. Many passes over one input then share a single upload.
 *
 * @param context Context built with config.
 * @param config Config of the spec the context was built from.
 * @param geometry Window length, stride, first offset and count. Every window must lie inside the values.
 * @param first_lag Shortest lag.
 * @param lag_count Number of consecutive lags.
 */
[[nodiscard]] inline DependencyStage lagged_pair_stage(
    WordPairExecutionContext& context, const GpuComputeConfig& config,
    const SegmentGeometry& geometry, uint32_t first_lag, uint32_t lag_count)
{
    const auto length_bits = std::bit_cast<uint64_t>(static_cast<double>(geometry.length));
    const std::array<uint32_t, 7> push {
        geometry.length, geometry.stride, geometry.offset, first_lag, lag_count,
        static_cast<uint32_t>(length_bits & 0xFFFFFFFFU), static_cast<uint32_t>(length_bits >> 32U)
    };
    const size_t result_bytes = static_cast<size_t>(geometry.count) * lag_count * sizeof(uint64_t);

    return DependencyStage {
        .config = config,
        .stage_fn = [&context, result_bytes, push](GpuDispatchCore&) {
            if (!context.shared_buffer_info(0, 1)) {
                context.set_output_size(1, result_bytes);
            }
            context.set_push_constants(push);
        },
        .explicit_groups = std::array<uint32_t, 3> { lag_count, geometry.count, 1U },
    };
}

/**
 * @brief The stage that sums every window at every lag in a range.
 *
 * One workgroup runs per lag and window, so the lag count and the window count
 * are each limited to the device's maximum workgroup count, 65535 at least.
 * The result is geometry.count * lag_count doubles, window major.
 *
 * @code
 * const auto spec = lagged_pair_spec(PairMap::Product, true);
 * const auto config = config_from_spec(spec);
 * WordPairExecutionContext context(config, bindings_from_spec(spec), false, "LaggedPair");
 * context.record_stages({ lagged_pair_stage(context, config, values, geometry, 0, 64) });
 * const auto sums = context.download_words(1, geometry.count * 64);
 * @endcode
 *
 * @param context Context built with config.
 * @param config Config of the spec the context was built from.
 * @param values Doubles the windows index into. Every window must lie inside it.
 * @param geometry Window length, stride, first offset and count.
 * @param first_lag Shortest lag.
 * @param lag_count Number of consecutive lags.
 */
[[nodiscard]] inline DependencyStage lagged_pair_stage(
    WordPairExecutionContext& context, const GpuComputeConfig& config, std::span<const double> values,
    const SegmentGeometry& geometry, uint32_t first_lag, uint32_t lag_count)
{
    auto stage = lagged_pair_stage(context, config, geometry, first_lag, lag_count);
    stage.stage_fn = [&context, values, resident = std::move(stage.stage_fn)](GpuDispatchCore& core) {
        context.stage_words(0, values);
        resident(core);
    };
    return stage;
}

} // namespace MayaFlux::Yantra
