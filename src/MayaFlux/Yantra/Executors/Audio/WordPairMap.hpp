#pragma once

#include "MayaFlux/Yantra/Executors/ShaderSpecBinding.hpp"
#include "MayaFlux/Yantra/Executors/WordPairExecutionContext.hpp"

/**
 * @file WordPairMap.hpp
 * @brief Element-wise IEEE double arithmetic on the GPU, exactly rounded, with no fp64 on the device.
 *
 * The WordPairMap kernel is run on a plain WordPairExecutionContext. This file
 * holds what is specific to it: the spec and the stage.
 */

namespace MayaFlux::Yantra {

/**
 * @brief The spec of a WordPairMap kernel.
 *
 * Result i is op(a[i], b[i]) in the template's emulated IEEE 754 binary64
 * arithmetic, rounded to nearest even, with the IEEE results for signed zeros,
 * infinities, NaN and subnormals. Sqrt takes only a. Bindings: a 0, b 1, result 2,
 * or a 0 and result 1 for Sqrt.
 *
 * @param op Operation applied to every element.
 * @param workgroup Invocations per group, from 1 to 1024.
 */
[[nodiscard]] inline Portal::Graphics::ShaderSpec word_pair_map_spec(
    Portal::Graphics::WordPairOp op, uint32_t workgroup = 256)
{
    using Portal::Graphics::BindingDirection;
    auto assemble = Portal::Graphics::ShaderSpec::Assemble {};
    assemble.tmpl(Portal::Graphics::KernelTemplate::WordPairMap)
        .word_op(op)
        .ssbo("a", BindingDirection::Input, Kakshya::GpuDataFormat::VEC2_U32);
    if (op != Portal::Graphics::WordPairOp::Sqrt) {
        assemble.ssbo("b", BindingDirection::Input, Kakshya::GpuDataFormat::VEC2_U32);
    }
    return assemble.ssbo("result", BindingDirection::Output, Kakshya::GpuDataFormat::VEC2_U32)
        .pc("count", Kakshya::GpuDataFormat::UINT32)
        .workgroup(workgroup)
        .build();
}

/**
 * @brief The stage that applies the operation to every element.
 *
 * The element count is the length of a, or the shorter of a and b for a binary
 * operation. The result is that many doubles.
 *
 * @code
 * const auto spec = word_pair_map_spec(WordPairOp::Div);
 * const auto config = config_from_spec(spec);
 * WordPairExecutionContext context(config, bindings_from_spec(spec), false, "WordPairMap");
 * context.record_stages({ word_pair_map_stage(context, config, WordPairOp::Div, numerators, denominators) });
 * const auto quotients = context.download_words(2, numerators.size());
 * @endcode
 *
 * @param context Context built with config.
 * @param config Config of the spec the context was built from.
 * @param op Operation of the spec.
 * @param a First operand of every element.
 * @param b Second operand of every element, ignored for Sqrt.
 * @param workgroup Workgroup size of the spec.
 */
[[nodiscard]] inline DependencyStage word_pair_map_stage(
    WordPairExecutionContext& context, const GpuComputeConfig& config, Portal::Graphics::WordPairOp op,
    std::span<const double> a, std::span<const double> b, uint32_t workgroup = 256)
{
    const bool unary = op == Portal::Graphics::WordPairOp::Sqrt;
    const size_t count = unary ? a.size() : std::min(a.size(), b.size());
    const uint32_t result_binding = unary ? 1U : 2U;
    const std::array<uint32_t, 1> push { static_cast<uint32_t>(count) };
    const uint32_t groups = std::max<uint32_t>(1U, static_cast<uint32_t>((count + workgroup - 1) / workgroup));

    return DependencyStage {
        .config = config,
        .stage_fn = [&context, a, b, unary, count, result_binding, push](GpuDispatchCore&) {
            context.stage_words(0, a.first(count));
            if (!unary) {
                context.stage_words(1, b.first(count));
            }
            context.set_output_size(result_binding, count * sizeof(uint64_t));
            context.set_push_constants(push);
        },
        .explicit_groups = std::array<uint32_t, 3> { groups, 1U, 1U },
    };
}

} // namespace MayaFlux::Yantra
