#pragma once

#include "MayaFlux/Journal/Archivist.hpp"
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
 * @brief The stage that applies the operation to count elements whose operands are already on the device.
 *
 * The operands are whatever the context's bindings resolve to: shared buffers
 * of the context, or buffers imported from another context with
 * import_shared. Nothing is staged from the host. Give the context that owns
 * the operands and this one the same recording, and record the owner's stage
 * first.
 *
 * @code
 * producer.ensure_shared_buffer(0, 2, count, GpuBufferBinding::ElementType::VEC2_U32);
 * consumer.import_shared(0, 0, producer, 0, 2, GpuBufferBinding::ElementType::VEC2_U32);
 * producer.record_stages({ word_pair_map_stage(producer, product_config, WordPairOp::Mul, a, b) });
 * consumer.record_stages({ word_pair_map_stage(consumer, root_config, WordPairOp::Sqrt, count) });
 * const auto roots = consumer.download_words(1, count);
 * @endcode
 *
 * @param context Context built with config.
 * @param config Config of the spec the context was built from.
 * @param op Operation of the spec.
 * @param count Elements to compute. The operand buffers must hold at least this many.
 * @param workgroup Workgroup size of the spec.
 */
[[nodiscard]] inline DependencyStage word_pair_map_stage(
    WordPairExecutionContext& context, const GpuComputeConfig& config, Portal::Graphics::WordPairOp op,
    size_t count, uint32_t workgroup = 256)
{
    const uint32_t result_binding = op == Portal::Graphics::WordPairOp::Sqrt ? 1U : 2U;
    const std::array<uint32_t, 1> push { static_cast<uint32_t>(count) };
    const uint32_t groups = std::max<uint32_t>(1U, static_cast<uint32_t>((count + workgroup - 1) / workgroup));

    return DependencyStage {
        .config = config,
        .stage_fn = [&context, count, result_binding, push](GpuDispatchCore&) {
            if (!context.shared_buffer_info(0, result_binding)) {
                context.set_output_size(result_binding, count * sizeof(uint64_t));
            }
            context.set_push_constants(push);
        },
        .explicit_groups = std::array<uint32_t, 3> { groups, 1U, 1U },
    };
}

/**
 * @struct WordPairOperand
 * @brief Where one operand of a WordPairMap stage comes from.
 *
 * A host sequence is staged by the stage. An operand on the device is whatever
 * its binding already resolves to, a shared buffer of the context or one
 * imported from another context.
 */
struct WordPairOperand {
    std::span<const double> host;
    bool resident { false };

    WordPairOperand() = default;
    WordPairOperand(std::span<const double> values)
        : host(values)
    {
    }

    WordPairOperand(const std::vector<double>& values)
        : host(values)
    {
    }

    /** @brief The operand is already on the device. */
    [[nodiscard]] static WordPairOperand on_device()
    {
        WordPairOperand operand;
        operand.resident = true;
        return operand;
    }
};

/**
 * @brief The stage that applies the operation to count elements, each operand from the host or the device.
 *
 * @code
 * consumer.record_stages({ word_pair_map_stage(*consumer, config, WordPairOp::Div, count,
 *     WordPairOperand::on_device(), divisors) });
 * @endcode
 *
 * @param context Context built with config.
 * @param config Config of the spec the context was built from.
 * @param op Operation of the spec.
 * @param count Elements to compute.
 * @param a First operand. A host operand must hold at least count elements.
 * @param b Second operand, ignored for Sqrt. A host operand must hold at least count elements.
 * @param workgroup Workgroup size of the spec.
 * @throws std::invalid_argument when a host operand holds fewer than count elements.
 */
[[nodiscard]] inline DependencyStage word_pair_map_stage(
    WordPairExecutionContext& context, const GpuComputeConfig& config, Portal::Graphics::WordPairOp op,
    size_t count, const WordPairOperand& a, const WordPairOperand& b, uint32_t workgroup = 256)
{
    const bool unary = op == Portal::Graphics::WordPairOp::Sqrt;
    const auto check = [count](const WordPairOperand& operand) {
        if (!operand.resident && operand.host.size() < count) {
            error<std::invalid_argument>(Journal::Component::Yantra, Journal::Context::BufferProcessing,
                std::source_location::current(),
                "word_pair_map_stage: a host operand holds {} elements, {} are required", operand.host.size(), count);
        }
    };
    check(a);
    if (!unary) {
        check(b);
    }

    auto stage = word_pair_map_stage(context, config, op, count, workgroup);
    stage.stage_fn = [&context, a, b, unary, count, resident = std::move(stage.stage_fn)](GpuDispatchCore& core) {
        if (!a.resident) {
            context.stage_words(0, a.host.first(count));
        }
        if (!unary && !b.resident) {
            context.stage_words(1, b.host.first(count));
        }
        resident(core);
    };
    return stage;
}

/**
 * @brief The stage that applies the operation to every element of two host sequences.
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

    return word_pair_map_stage(context, config, op, count, WordPairOperand(a), WordPairOperand(b), workgroup);
}

} // namespace MayaFlux::Yantra
