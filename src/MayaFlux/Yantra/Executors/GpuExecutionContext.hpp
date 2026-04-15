#pragma once

#include "GpuDispatchCore.hpp"

namespace MayaFlux::Yantra {

/**
 * @class GpuExecutionContext
 * @brief Type-parameterised shell over GpuDispatchCore.
 *
 * Handles only the two steps that must be aware of InputType/OutputType:
 *   1. Extracting double channels from a Datum<InputType>.
 *   2. Reconstructing a Datum<OutputType> from the float readback.
 *
 * All resource management, buffer staging, dispatch orchestration, and
 * virtual override points live in GpuDispatchCore and are compiled once
 * in GpuDispatchCore.cpp.
 *
 * Subclasses override GpuDispatchCore virtuals (declare_buffer_bindings,
 * on_before_gpu_dispatch, prepare_gpu_inputs, calculate_dispatch_size)
 * directly; no additional template surface is required.
 *
 * @tparam InputType  ComputeData type accepted.
 * @tparam OutputType ComputeData type produced.
 */
template <ComputeData InputType = std::vector<Kakshya::DataVariant>,
    ComputeData OutputType = InputType>
class MAYAFLUX_API GpuExecutionContext : public GpuDispatchCore {
public:
    using input_type = Datum<InputType>;
    using output_type = Datum<OutputType>;

    explicit GpuExecutionContext(GpuShaderConfig config)
        : GpuDispatchCore(std::move(config))
    {
    }

    ~GpuExecutionContext() override = default;

    GpuExecutionContext(const GpuExecutionContext&) = delete;
    GpuExecutionContext& operator=(const GpuExecutionContext&) = delete;
    GpuExecutionContext(GpuExecutionContext&&) = delete;
    GpuExecutionContext& operator=(GpuExecutionContext&&) = delete;

    /**
     * @brief Dispatch to GPU and reconstruct a typed output Datum.
     *
     * Routes to dispatch_core_chained for CHAINED mode; all other modes
     * use dispatch_core.  Both paths are defined in GpuDispatchCore.cpp.
     *
     * @param input Input Datum to process.
     * @param ctx   ExecutionContext; CHAINED mode requires 'pass_count' and
     *              'pc_updater' in execution_metadata.
     * @throws std::runtime_error If GPU initialisation fails.
     */
    virtual output_type execute(const input_type& input, const ExecutionContext& ctx)
    {
        if (!ensure_gpu_ready()) {
            error<std::runtime_error>(
                Journal::Component::Yantra,
                Journal::Context::BufferProcessing,
                std::source_location::current(),
                "GpuExecutionContext: GPU initialisation failed");
        }

        auto [channels, structure_info] = OperationHelper::extract_structured_double(
            const_cast<input_type&>(input));

        std::vector<std::vector<double>> ch_copies(channels.size());
        for (size_t c = 0; c < channels.size(); ++c)
            ch_copies[c].assign(channels[c].begin(), channels[c].end());

        const GpuChannelResult raw = (ctx.mode == ExecutionMode::CHAINED)
            ? dispatch_core_chained(ch_copies, structure_info, ctx)
            : dispatch_core(ch_copies, structure_info);

        return collect_gpu_outputs(raw, ch_copies, structure_info);
    }

protected:
    /**
     * @brief Reconstruct Datum<OutputType> from a GpuChannelResult.
     *
     * Reads the primary float readback into per-channel doubles and
     * delegates to OperationHelper::reconstruct_from_double.  Aux entries
     * are forwarded into result.metadata keyed as "gpu_output_N".
     *
     * Override to perform custom readback interpretation, e.g. when the
     * output buffer layout does not match the input channel structure.
     *
     * @param raw            Readback produced by dispatch_core or dispatch_core_chained.
     * @param channels       Double channels extracted from the input Datum.
     * @param structure_info Dimension/modality metadata.
     */
    virtual output_type collect_gpu_outputs(
        const GpuChannelResult& raw,
        const std::vector<std::vector<double>>& channels,
        const DataStructureInfo& structure_info)
    {
        output_type result;

        const size_t total_ch_elements = std::accumulate(
            channels.begin(), channels.end(), size_t { 0 },
            [](size_t s, const auto& ch) { return s + ch.size(); });

        if (!raw.primary.empty() && !channels.empty()
            && raw.primary.size() >= total_ch_elements) {
            size_t offset = 0;
            std::vector<std::vector<double>> result_ch(channels.size());
            for (size_t c = 0; c < channels.size(); ++c) {
                result_ch[c].resize(channels[c].size());
                for (size_t i = 0; i < channels[c].size(); ++i)
                    result_ch[c][i] = static_cast<double>(raw.primary[offset++]);
            }
            result = Datum<OutputType>(
                OperationHelper::reconstruct_from_double<OutputType>(result_ch, structure_info));
        }

        for (const auto& [idx, bytes] : raw.aux)
            result.metadata["gpu_output_" + std::to_string(idx)] = bytes;

        return result;
    }
};

} // namespace MayaFlux::Yantra
