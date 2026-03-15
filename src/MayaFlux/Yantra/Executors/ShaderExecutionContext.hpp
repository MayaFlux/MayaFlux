#pragma once

#include "GpuExecutionContext.hpp"

namespace MayaFlux::Yantra {

/**
 * @class ShaderExecutionContext
 * @brief Concrete GpuExecutionContext for a single fixed shader with fixed bindings.
 *
 * The common case for GPU dispatch: shader path, workgroup size, and buffer
 * layout are fixed at construction. Attach to any ComputeOperation via
 * ComputeOperation::set_gpu_backend() to enable GPU dispatch with the owning
 * operation's CPU implementation as automatic fallback.
 *
 * @tparam InputType  ComputeData type accepted.
 * @tparam OutputType ComputeData type produced.
 */
template <ComputeData InputType = std::vector<Kakshya::DataVariant>,
    ComputeData OutputType = InputType>
class MAYAFLUX_API ShaderExecutionContext : public GpuExecutionContext<InputType, OutputType> {
public:
    using input_type = typename GpuExecutionContext<InputType, OutputType>::input_type;
    using output_type = typename GpuExecutionContext<InputType, OutputType>::output_type;

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
    ShaderExecutionContext(
        GpuShaderConfig config,
        std::vector<GpuBufferBinding> bindings,
        std::string name = "ShaderExecutionContext")
        : GpuExecutionContext<InputType, OutputType>(std::move(config))
        , m_bindings(std::move(bindings))
        , m_name(std::move(name))
    {
    }

protected:
    [[nodiscard]] std::vector<GpuBufferBinding> declare_buffer_bindings() const override
    {
        return m_bindings;
    }

private:
    std::vector<GpuBufferBinding> m_bindings;
    ExecutionContext m_ctx_prototype;
    std::string m_name;
};

// =============================================================================
// Factory helpers
// =============================================================================

/**
 * @brief Convenience factory for ShaderExecutionContext.
 *
 * @code
 * auto executor = make_shader_executor(
 *     { "shaders/spectral_blur.comp", { 256, 1, 1 }, sizeof(SpectralBlurPC) },
 *     { GpuBufferBinding::input(0, 0),
 *       GpuBufferBinding::output(0, 1) },
 *     "spectral_blur"
 * );
 * my_operation->set_gpu_backend(executor);
 * @endcode
 */
template <ComputeData InputType = std::vector<Kakshya::DataVariant>,
    ComputeData OutputType = InputType>
std::shared_ptr<ShaderExecutionContext<InputType, OutputType>>
make_shader_executor(
    GpuShaderConfig config,
    std::vector<GpuBufferBinding> bindings,
    std::string name = "ShaderExecutionContext")
{
    return std::make_shared<ShaderExecutionContext<InputType, OutputType>>(
        std::move(config),
        std::move(bindings),
        std::move(name));
}

} // namespace MayaFlux::Yantra
