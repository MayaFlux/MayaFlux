#pragma once

#include "MayaFlux/Yantra/Executors/ShaderExecutionContext.hpp"
#include "UniversalTransformer.hpp"

namespace MayaFlux::Yantra {

/**
 * @class GpuTransformer
 * @brief Concrete UniversalTransformer that dispatches entirely via a
 *        ShaderExecutionContext. CPU path is a hard error.
 *
 * Use when no CPU fallback exists or is needed. Attach a configured
 * ShaderExecutionContext at construction. For operations where a CPU
 * fallback is available, prefer attaching a ShaderExecutionContext to
 * an existing concrete transformer via set_gpu_backend() instead.
 *
 * @code
 * auto op = std::make_shared<GpuTransformer<>>(
 *     std::make_shared<ShaderExecutionContext<>>(
 *         GpuShaderConfig { "my_shader.comp", { 256, 1, 1 }, sizeof(MyPC) }));
 * op->get_executor()->input(data).output(output_bytes).push(pc);
 * pipeline->add_operation(op, "my_shader");
 * @endcode
 *
 * @tparam InputType  ComputeData type accepted.
 * @tparam OutputType ComputeData type produced.
 */
template <ComputeData InputType = std::vector<Kakshya::DataVariant>,
    ComputeData OutputType = InputType>
class MAYAFLUX_API GpuTransformer : public UniversalTransformer<InputType, OutputType> {
public:
    using input_type = Datum<InputType>;
    using output_type = Datum<OutputType>;

    /**
     * @brief Construct with a configured ShaderExecutionContext.
     * @param executor Configured executor. Must not be null.
     */
    explicit GpuTransformer(
        std::shared_ptr<ShaderExecutionContext<InputType, OutputType>> executor)
    {
        assert(executor && "GpuTransformer: executor must not be null");
        m_executor = executor;
        this->set_gpu_backend(std::move(executor));
    }

    /**
     * @brief Returns the attached ShaderExecutionContext for further configuration.
     */
    [[nodiscard]] std::shared_ptr<ShaderExecutionContext<InputType, OutputType>>
    get_executor() const { return m_executor; }

    [[nodiscard]] TransformationType get_transformation_type() const override
    {
        return TransformationType::CUSTOM;
    }

    [[nodiscard]] std::string get_transformer_name() const override
    {
        return "GpuTransformer";
    }

protected:
    output_type transform_implementation(input_type&) override
    {
        error<std::runtime_error>(
            Journal::Component::Yantra,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "GpuTransformer: GPU unavailable and no CPU fallback provided");
    }

private:
    std::shared_ptr<ShaderExecutionContext<InputType, OutputType>> m_executor;
};

} // namespace MayaFlux::Yantra
