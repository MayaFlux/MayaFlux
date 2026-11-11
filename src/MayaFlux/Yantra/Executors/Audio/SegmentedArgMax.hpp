#pragma once

#include "MayaFlux/Yantra/Executors/ShaderSpecBinding.hpp"
#include "MayaFlux/Yantra/Executors/WordPairExecutionContext.hpp"

#include "SegmentGeometry.hpp"

/**
 * @file SegmentedArgMax.hpp
 * @brief The largest value of each segment of doubles and where it is, on the GPU, with no fp64 on the device.
 *
 * The SegmentedArgMax kernel is run on a plain WordPairExecutionContext. This
 * file holds what is specific to it: the spec, the stage and the decoding of
 * its result.
 */

namespace MayaFlux::Yantra {

/** @brief The index reported for a segment that holds no value to select. */
inline constexpr uint32_t no_argmax_index = 0xFFFFFFFFU;

/**
 * @struct ArgMaxResult
 * @brief The selected value of one segment and its position inside the segment.
 */
struct ArgMaxResult {
    /** The largest value, NaN when the segment holds none. */
    double value { std::numeric_limits<double>::quiet_NaN() };
    /** Position of the first occurrence of the value, or no_argmax_index. */
    uint32_t index { no_argmax_index };

    [[nodiscard]] bool found() const noexcept { return index != no_argmax_index; }
};

/**
 * @brief The spec of a SegmentedArgMax kernel.
 *
 * Segment s covers length elements from offset + s * stride. Values compare by
 * the library order key, where -0.0 equals +0.0, with every NaN smaller than
 * any other value, so a NaN is never selected. Of equal values the lowest
 * index wins. Bindings: values 0, result 1.
 *
 * @param workgroup Invocations per segment. A power of two from 2 to 1024.
 */
[[nodiscard]] inline Portal::Graphics::ShaderSpec segmented_argmax_spec(uint32_t workgroup = 256)
{
    using Portal::Graphics::BindingDirection;
    return Portal::Graphics::ShaderSpec::Assemble {}
        .tmpl(Portal::Graphics::KernelTemplate::SegmentedArgMax)
        .ssbo("values", BindingDirection::Input, Kakshya::GpuDataFormat::VEC2_U32)
        .ssbo("result", BindingDirection::Output, Kakshya::GpuDataFormat::VEC2_U32)
        .pc("length", Kakshya::GpuDataFormat::UINT32)
        .pc("stride", Kakshya::GpuDataFormat::UINT32)
        .pc("offset", Kakshya::GpuDataFormat::UINT32)
        .workgroup(workgroup)
        .build();
}

/**
 * @brief The stage that selects the largest value of every segment of values already on the device.
 *
 * The values are whatever binding 0 resolves to: a shared buffer of the
 * context, or one imported from another context with import_shared, for
 * instance the result of a LaggedPair pass recorded earlier on the same
 * recording. Nothing is staged from the host.
 *
 * @code
 * const auto spec = segmented_argmax_spec();
 * const auto config = config_from_spec(spec);
 * WordPairExecutionContext select(config, bindings_from_spec(spec), false, "SegmentedArgMax");
 * select.set_recording(recording);
 * select.import_shared(0, 0, correlation, 0, 1, GpuBufferBinding::ElementType::VEC2_U32);
 * select.record_stages({ segmented_argmax_stage(select, config, geometry) });
 * const auto best = read_argmax(select, geometry.count);
 * @endcode
 *
 * @param context Context built with config.
 * @param config Config of the spec the context was built from.
 * @param geometry Segment length, stride, first offset and count. Every segment must lie inside the values.
 */
[[nodiscard]] inline DependencyStage segmented_argmax_stage(
    WordPairExecutionContext& context, const GpuComputeConfig& config, const SegmentGeometry& geometry)
{
    const std::array<uint32_t, 3> push { geometry.length, geometry.stride, geometry.offset };
    const size_t result_bytes = static_cast<size_t>(geometry.count) * 2U * sizeof(uint64_t);

    return DependencyStage {
        .config = config,
        .stage_fn = [&context, result_bytes, push](GpuDispatchCore&) {
            if (!context.shared_buffer_info(0, 1)) {
                context.set_output_size(1, result_bytes);
            }
            context.set_push_constants(push);
        },
        .explicit_groups = std::array<uint32_t, 3> { geometry.count, 1U, 1U },
    };
}

/**
 * @brief The stage that selects the largest value of every segment of a host sequence.
 *
 * @param context Context built with config.
 * @param config Config of the spec the context was built from.
 * @param values Doubles the segments index into.
 * @param geometry Segment length, stride, first offset and count. Every segment must lie inside values.
 */
[[nodiscard]] inline DependencyStage segmented_argmax_stage(
    WordPairExecutionContext& context, const GpuComputeConfig& config, std::span<const double> values,
    const SegmentGeometry& geometry)
{
    auto stage = segmented_argmax_stage(context, config, geometry);
    stage.stage_fn = [&context, values, resident = std::move(stage.stage_fn)](GpuDispatchCore& core) {
        context.stage_words(0, values);
        resident(core);
    };
    return stage;
}

/**
 * @brief Read the selections of the segments of the last recorded stage.
 * @param context The context the stage was recorded on.
 * @param segments Number of segments of that stage.
 */
[[nodiscard]] inline std::vector<ArgMaxResult> read_argmax(WordPairExecutionContext& context, size_t segments)
{
    const auto words = context.download_words(1, segments * 2U);
    std::vector<ArgMaxResult> out;
    out.reserve(segments);
    for (size_t s = 0; s < segments && 2U * s + 1U < words.size(); ++s) {
        out.push_back({ .value = words[2U * s],
            .index = static_cast<uint32_t>(std::bit_cast<uint64_t>(words[2U * s + 1U]) & 0xFFFFFFFFULL) });
    }
    return out;
}

} // namespace MayaFlux::Yantra
