#pragma once

#include "GpuComputeOperation.hpp"
#include "OperationSpec/GrammarHelper.hpp"

namespace MayaFlux::Yantra {

/**
 * @class ShaderOperation
 * @brief Concrete GpuComputeOperation configured entirely at construction time.
 *
 * Covers the common case — fixed shader, fixed bindings, optional CPU fallback —
 * without requiring a subclass. Also participates in the grammar/context system
 * when an ExecutionContext prototype is supplied.
 *
 * Three construction paths:
 *
 *   1. Minimal — shader + bindings only. GPU dispatches, CPU path errors.
 *
 *   2. With CPU fallback — adds a callable for when GPU is unavailable.
 *
 *   3. With context prototype — copies metadata, hooks, and mode from the
 *      prototype into every incoming ExecutionContext before dispatch, so
 *      ShaderOperation participates in ComputationGrammar rules, ComputeMatrix
 *      context configurators, and any opinionated pipeline that sets context
 *      expectations on the incoming context.
 *
 * The context prototype is applied additively: keys already present in the
 * incoming context are not overwritten. This preserves caller intent while
 * letting the operation declare its own defaults.
 *
 * @tparam InputType  ComputeData type accepted.
 * @tparam OutputType ComputeData type produced.
 */
template <ComputeData InputType = std::vector<Kakshya::DataVariant>,
    ComputeData OutputType = InputType>
class MAYAFLUX_API ShaderOperation : public GpuComputeOperation<InputType, OutputType> {
public:
    using input_type = typename GpuComputeOperation<InputType, OutputType>::input_type;
    using output_type = typename GpuComputeOperation<InputType, OutputType>::output_type;
    using CpuFallback = std::function<output_type(const input_type&)>;

    /**
     * @brief Construct with shader config, bindings, optional CPU fallback, and
     *        optional ExecutionContext prototype.
     *
     * @param config           Shader path, workgroup size, push constant size.
     * @param bindings         Descriptor layout the shader expects.
     * @param cpu_fallback     Called when GPU is unavailable. Null errors on CPU path.
     * @param ctx_prototype    Context whose metadata, hooks, and mode are merged
     *                         into every incoming ExecutionContext before dispatch.
     *                         Pass a default-constructed ExecutionContext to opt out.
     * @param name             Operation name for introspection.
     */
    ShaderOperation(
        GpuShaderConfig config,
        std::vector<GpuBufferBinding> bindings,
        CpuFallback cpu_fallback = nullptr,
        ExecutionContext ctx_prototype = {},
        std::string name = "ShaderOperation")
        : GpuComputeOperation<InputType, OutputType>(std::move(config))
        , m_bindings(std::move(bindings))
        , m_cpu_fallback(std::move(cpu_fallback))
        , m_ctx_prototype(std::move(ctx_prototype))
        , m_name(std::move(name))
    {
    }

    void set_parameter(const std::string& name, std::any value) override
    {
        m_params[name] = std::move(value);
    }

    [[nodiscard]] std::any get_parameter(const std::string& name) const override
    {
        auto it = m_params.find(name);
        return it != m_params.end() ? it->second : std::any {};
    }

    [[nodiscard]] std::map<std::string, std::any> get_all_parameters() const override
    {
        return { m_params.begin(), m_params.end() };
    }

    [[nodiscard]] std::string get_name() const override { return m_name; }

protected:
    [[nodiscard]] std::vector<GpuBufferBinding> declare_buffer_bindings() const override
    {
        return m_bindings;
    }

    /**
     * @brief Merges the stored prototype into the incoming context then dispatches.
     *
     * Prototype metadata keys are copied only when absent from the incoming context,
     * preserving any ComputationContext or hints already set by a grammar rule or
     * ComputeMatrix configurator. Mode and hooks from the prototype are applied only
     * when the incoming context carries their default/null values.
     */
    output_type apply_operation_internal(
        const input_type& input,
        const ExecutionContext& ctx) override
    {
        ExecutionContext merged = ctx;
        apply_prototype(merged);
        return GpuComputeOperation<InputType, OutputType>::apply_operation_internal(input, merged);
    }

    output_type operation_function(const input_type& input) override
    {
        if (m_cpu_fallback) {
            return m_cpu_fallback(input);
        }
        error<std::runtime_error>(
            Journal::Component::Yantra,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "ShaderOperation '{}': GPU unavailable and no CPU fallback provided",
            m_name);
    }

private:
    std::vector<GpuBufferBinding> m_bindings;
    CpuFallback m_cpu_fallback;
    ExecutionContext m_ctx_prototype;
    std::string m_name;
    std::unordered_map<std::string, std::any> m_params;

    void apply_prototype(ExecutionContext& ctx) const
    {
        for (const auto& [k, v] : m_ctx_prototype.execution_metadata) {
            ctx.execution_metadata.try_emplace(k, v);
        }

        if (ctx.mode == ExecutionMode::SYNC
            && m_ctx_prototype.mode != ExecutionMode::SYNC) {
            ctx.mode = m_ctx_prototype.mode;
        }

        if (!ctx.pre_execution_hook && m_ctx_prototype.pre_execution_hook) {
            ctx.pre_execution_hook = m_ctx_prototype.pre_execution_hook;
        }

        if (!ctx.post_execution_hook && m_ctx_prototype.post_execution_hook) {
            ctx.post_execution_hook = m_ctx_prototype.post_execution_hook;
        }
    }
};

// =============================================================================
// Factory helpers
// =============================================================================

/**
 * @brief Create a ShaderOperation with a ComputationContext baked into its prototype.
 *
 * Convenience for grammar-integrated use where the operation should declare its
 * own computational domain without the caller constructing an ExecutionContext manually.
 *
 * @code
 * auto op = make_shader_operation<>(
 *     { "shaders/spectral_blur.spv" },
 *     { { 0, 0, GpuBufferBinding::Direction::INPUT },
 *       { 0, 1, GpuBufferBinding::Direction::OUTPUT } },
 *     ComputationContext::SPECTRAL,
 *     fallback_fn,
 *     "spectral_blur"
 * );
 * @endcode
 */
template <ComputeData InputType = std::vector<Kakshya::DataVariant>,
    ComputeData OutputType = InputType>
std::shared_ptr<ShaderOperation<InputType, OutputType>>
make_shader_operation(
    GpuShaderConfig config,
    std::vector<GpuBufferBinding> bindings,
    ComputationContext computation_context,
    typename ShaderOperation<InputType, OutputType>::CpuFallback cpu_fallback = nullptr,
    std::string name = "ShaderOperation")
{
    ExecutionContext proto;
    proto.execution_metadata["computation_context"] = computation_context;

    return std::make_shared<ShaderOperation<InputType, OutputType>>(
        std::move(config),
        std::move(bindings),
        std::move(cpu_fallback),
        std::move(proto),
        std::move(name));
}

} // namespace MayaFlux::Yantra
