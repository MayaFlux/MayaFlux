#pragma once

#include "MayaFlux/Yantra/Executors/GpuExecutionContext.hpp"
#include "UniversalAnalyzer.hpp"

namespace MayaFlux::Yantra {

/**
 * @class GpuAnalyzer
 * @brief Concrete UniversalAnalyzer that dispatches entirely via a
 *        GpuExecutionContext. CPU path is a hard error.
 *
 * @tparam InputType  ComputeData type accepted.
 * @tparam OutputType ComputeData type produced.
 */
template <ComputeData InputType = std::vector<Kakshya::DataVariant>,
    ComputeData OutputType = InputType>
class MAYAFLUX_API GpuAnalyzer : public UniversalAnalyzer<InputType, OutputType> {
public:
    using input_type = Datum<InputType>;
    using output_type = Datum<OutputType>;

    /**
     * @brief Construct with a configured GpuExecutionContext.
     * @param executor Configured executor. Must not be null.
     */
    explicit GpuAnalyzer(
        std::shared_ptr<GpuExecutionContext<InputType, OutputType>> executor)
    {
        assert(executor && "GpuAnalyzer: executor must not be null");
        m_executor = executor;
        this->set_gpu_backend(std::move(executor));
    }

    /**
     * @brief Returns the attached GpuExecutionContext for further configuration.
     */
    [[nodiscard]] std::shared_ptr<GpuExecutionContext<InputType, OutputType>>

    get_executor() const
    {
        return m_executor;
    }

    [[nodiscard]] std::string get_analyzer_name() const override
    {
        return "GpuAnalyzer";
    }

    [[nodiscard]] AnalysisType get_analysis_type() const override
    {
        return AnalysisType::CUSTOM;
    }

protected:
    output_type analyze_implementation(const input_type&) override
    {
        error<std::runtime_error>(
            Journal::Component::Yantra,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "GpuAnalyzer: GPU unavailable and no CPU fallback provided");
    }

private:
    std::shared_ptr<GpuExecutionContext<InputType, OutputType>> m_executor;
};

} // namespace MayaFlux::Yantra
