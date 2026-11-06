#pragma once

#include "MayaFlux/Yantra/Executors/GpuExecutionContext.hpp"
#include "Transformer.hpp"

namespace MayaFlux::Yantra {

/**
 * @class GpuTransformer
 * @brief Concrete Transformer that dispatches entirely via a
 *        GpuExecutionContext. CPU path is a hard error.
 *
 * Use to run a shader as an operation. Hand it a configured
 * GpuExecutionContext at construction. It declares no target and is not in the
 * default registry; an operation with both a CPU and a GPU implementation is two
 * classes, each a registered candidate on its own target.
 *
 * @code
 * auto op = std::make_shared<GpuTransformer<>>(
 *     std::make_shared<GpuExecutionContext<>>(
 *         GpuComputeConfig { "my_shader.comp", { 256, 1, 1 }, sizeof(MyPC) }));
 * op->get_executor()->input(data).output(output_bytes).push(pc);
 * pipeline->add_operation(op, "my_shader");
 * @endcode
 *
 * @tparam InputType  ComputeData type accepted.
 * @tparam OutputType ComputeData type produced.
 */
template <ComputeData InputType = std::vector<Kakshya::DataVariant>,
    ComputeData OutputType = InputType>
class GpuTransformer : public Transformer<InputType, OutputType> {
public:
    using input_type = Datum<InputType>;
    using output_type = Datum<OutputType>;

    /**
     * @brief Construct with a configured GpuExecutionContext.
     * @param executor Configured executor. Must not be null.
     */
    explicit GpuTransformer(
        std::shared_ptr<GpuExecutionContext<InputType, OutputType>> executor)
    {
        assert(executor && "GpuTransformer: executor must not be null");
        m_executor = std::move(executor);
    }

    /**
     * @brief Returns the attached GpuExecutionContext for further configuration.
     */
    [[nodiscard]] std::shared_ptr<GpuExecutionContext<InputType, OutputType>>
    get_executor() const { return m_executor; }

    [[nodiscard]] TransformationType get_transformation_type() const override
    {
        return TransformationType::CUSTOM;
    }


protected:
    /**
     * @brief Dispatches through the executor when the GPU is ready. Otherwise the
     *        base path runs and reaches the hard error below: there is no CPU fallback.
     */
    output_type apply_operation_internal(const input_type& input, const ExecutionContext& context) override
    {
        if (m_executor->ensure_gpu_ready()) {
            return m_executor->execute(input, context);
        }
        return Transformer<InputType, OutputType>::apply_operation_internal(input, context);
    }

    output_type transform_implementation(input_type&) override
    {
        error<std::runtime_error>(
            Journal::Component::Yantra,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "GpuTransformer: GPU unavailable and no CPU fallback provided");
    }

private:
    std::shared_ptr<GpuExecutionContext<InputType, OutputType>> m_executor;
};

} // namespace MayaFlux::Yantra
