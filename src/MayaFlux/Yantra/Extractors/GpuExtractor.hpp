#pragma once

#include "MayaFlux/Yantra/Executors/GpuExecutionContext.hpp"
#include "UniversalExtractor.hpp"

namespace MayaFlux::Yantra {

/**
 * @class GpuExtractor
 * @brief Concrete UniversalExtractor that dispatches entirely via a
 *        GpuExecutionContext. CPU path is a hard error.
 *
 * @tparam InputType  ComputeData type accepted.
 * @tparam OutputType ComputeData type produced.
 */
template <ComputeData InputType = std::vector<Kakshya::DataVariant>,
    ComputeData OutputType = InputType>
class MAYAFLUX_API GpuExtractor : public UniversalExtractor<InputType, OutputType> {
public:
    using input_type = Datum<InputType>;
    using output_type = Datum<OutputType>;

    /**
     * @brief Construct with a configured GpuExecutionContext.
     * @param executor Configured executor. Must not be null.
     */
    explicit GpuExtractor(
        std::shared_ptr<GpuExecutionContext<InputType, OutputType>> executor)
    {
        assert(executor && "GpuExtractor: executor must not be null");
        m_executor = executor;
        this->set_gpu_backend(std::move(executor));
    }

    /**
     * @brief Returns the attached GpuExecutionContext for further configuration.
     */
    [[nodiscard]] std::shared_ptr<GpuExecutionContext<InputType, OutputType>>
    get_executor() const { return m_executor; }

    [[nodiscard]] std::string get_extractor_name() const override
    {
        return "GpuExtractor";
    }

    [[nodiscard]] ExtractionType get_extraction_type() const override
    {
        return ExtractionType::CUSTOM;
    }

    [[nodiscard]] std::vector<std::string> get_available_methods() const override
    {
        return { "gpu_extract" };
    }

protected:
    output_type extract_implementation(const input_type&) override
    {
        error<std::runtime_error>(
            Journal::Component::Yantra,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "GpuExtractor: GPU unavailable and no CPU fallback provided");
    }

private:
    std::shared_ptr<GpuExecutionContext<InputType, OutputType>> m_executor;
};

} // namespace MayaFlux::Yantra
