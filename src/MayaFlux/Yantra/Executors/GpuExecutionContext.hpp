#pragma once

#include "GpuDispatchCore.hpp"

namespace MayaFlux::Yantra {

/**
 * @class GpuExecutionContext
 * @brief Type-parameterised shell over GpuDispatchCore.
 *
 * Handles the two type-aware boundary steps:
 *   1. Extracting input data for dispatch_core  — overridable via extract_inputs().
 *   2. Reconstructing a Datum<OutputType>       — overridable via collect_gpu_outputs().
 *
 * All resource management, buffer staging, dispatch orchestration, and
 * virtual override points live in GpuDispatchCore and are compiled once
 * in GpuDispatchCore.cpp.
 *
 * Subclasses override GpuDispatchCore virtuals (declare_buffer_bindings,
 * on_before_gpu_dispatch, prepare_gpu_inputs, calculate_dispatch_size)
 * directly;
 * Subclasses that do not operate on numeric channels (e.g. image-only shaders)
 * override extract_inputs() to return empty channels and override
 * collect_gpu_outputs() to pull from get_output_image() instead of the float
 * readback.
 *
 * @tparam InputType  ComputeData type accepted.
 * @tparam OutputType ComputeData type produced.
 */
template <ComputeData InputType = std::vector<Kakshya::DataVariant>,
    ComputeData OutputType = InputType>
class GpuExecutionContext : public GpuDispatchCore {
public:
    using input_type = Datum<InputType>;
    using output_type = Datum<OutputType>;

    explicit GpuExecutionContext(GpuComputeConfig config)
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
        if (ctx.mode == ExecutionMode::DEPENDENCY) {
            if (!ctx.execution_metadata.contains("dependency_stages")) {
                error<std::runtime_error>(Journal::Component::Yantra,
                    Journal::Context::Runtime,
                    std::source_location::current(),
                    "GpuExecutionContext: DEPENDENCY mode requires 'dependency_stages' in execution_metadata");
            }
            const auto& stages = safe_any_cast_or_throw<std::vector<DependencyStage>>(
                ctx.execution_metadata.at("dependency_stages"));
            dispatch_core_dependency(stages);
            return output_type {};
        }

        if (!ensure_gpu_ready()) {
            error<std::runtime_error>(
                Journal::Component::Yantra,
                Journal::Context::BufferProcessing,
                std::source_location::current(),
                "GpuExecutionContext: GPU initialisation failed");
        }

        auto [ch_copies, structure_info] = extract_inputs(input);

        const GpuChannelResult raw = (ctx.mode == ExecutionMode::CHAINED)
            ? dispatch_core_chained(ch_copies, structure_info, ctx)
            : dispatch_core(ch_copies, structure_info);

        return collect_gpu_outputs(raw, ch_copies, structure_info);
    }

protected:
    /**
     * @brief Extract channel data and structure metadata from the input Datum.
     *
     * Calls extract_structured_native first to obtain native-typed spans without
     * any conversion, then resolves the native element type from the first span's
     * active DataSpanVariant alternative (which is already determined by visiting
     * the source variants -- no separate original_type inspection needed).
     *
     * - float / uint8_t / uint16_t / uint32_t: stages raw bytes natively via
     *   flatten_native_variants_to_staging. Returns ({}, structure_info).
     * - double / complex / anything else: converts spans to vector<vector<double>>
     *   for the standard double staging path. Unchanged from previous behaviour.
     *
     * Override to return ({}, {}) when channel extraction is not needed
     * (e.g. image-only shaders that stage via on_before_gpu_dispatch).
     */
    virtual std::pair<std::vector<std::vector<double>>, DataStructureInfo>
    extract_inputs(const input_type& input)
    {
        auto info = OperationHelper::get_structure_info(const_cast<input_type&>(input));

        const std::type_index native_type = [&]() -> std::type_index {
            if constexpr (std::is_same_v<
                              std::decay_t<decltype(input.data)>,
                              std::shared_ptr<Kakshya::SignalSourceContainer>>) {
                if (input.data)
                    return input.data->value_element_type();
            } else if constexpr (std::is_same_v<
                                     std::decay_t<decltype(input.data)>,
                                     std::vector<Kakshya::DataVariant>>) {
                if (!input.data.empty())
                    return Kakshya::FrameView(OperationHelper::extract_native_data(input.data[0])).element_type();
            }
            return typeid(double);
        }();

        const bool is_native_non_double = native_type == typeid(float) || native_type == typeid(uint8_t) || native_type == typeid(uint16_t) || native_type == typeid(uint32_t);

        if (is_native_non_double) {
            if constexpr (std::is_same_v<
                              std::decay_t<decltype(input.data)>,
                              std::vector<Kakshya::DataVariant>>) {
                flatten_native_variants_to_staging(input.data, info);
            } else if constexpr (std::is_same_v<
                                     std::decay_t<decltype(input.data)>,
                                     std::shared_ptr<Kakshya::SignalSourceContainer>>) {
                flatten_native_variants_to_staging(input.data->get_data(), info);
            }
            return { {}, std::move(info) };
        }

        auto [spans, double_info] = OperationHelper::extract_structured_double(
            const_cast<input_type&>(input));

        std::vector<std::vector<double>> channels(spans.size());
        for (size_t c = 0; c < spans.size(); ++c)
            channels[c].assign(spans[c].begin(), spans[c].end());
        return { std::move(channels), std::move(double_info) };
    }

    /**
     * @brief Reconstruct Datum<OutputType> from a GpuChannelResult.
     *
     * Branches on structure_info.original_type:
     *
     * - Non-double native types (float, uint8_t, uint16_t, uint32_t):
     *   reads raw bytes directly from the first OUTPUT binding via download_binding
     *   rather than the float readback path, then packs them into a DataVariant
     *   in the correct native type. No float reinterpretation of pixel bytes.
     *
     * - Double / complex / other: reads raw.primary as float, widens to double
     *   per channel, delegates to reconstruct_from_double. Unchanged from
     *   previous behaviour.
     *
     * Override to perform custom readback interpretation (e.g. image containers).
     */
    virtual output_type collect_gpu_outputs(
        const GpuChannelResult& raw,
        const std::vector<std::vector<double>>& channels,
        const DataStructureInfo& structure_info)
    {
        output_type result;

        const auto& ot = structure_info.original_type;
        const bool is_native_non_double = ot == std::type_index(typeid(std::vector<float>)) || ot == std::type_index(typeid(std::vector<uint8_t>)) || ot == std::type_index(typeid(std::vector<uint16_t>)) || ot == std::type_index(typeid(std::vector<uint32_t>));

        if (is_native_non_double) {
            const size_t out_idx = find_first_output_index();
            const size_t allocated = m_resources.buffer_allocated_bytes(dispatch_key(), out_idx);
            if (allocated > 0) {
                std::vector<uint8_t> raw_bytes(allocated);
                download_binding(out_idx, raw_bytes.data(), allocated);

                auto native_variant = OperationHelper::reconstruct_from_double<Kakshya::DataVariant>(
                    { std::vector<double>(allocated / sizeof(double), 0.0) },
                    structure_info);
                std::visit([&](auto& vec) {
                    using V = typename std::decay_t<decltype(vec)>::value_type;
                    vec.resize(allocated / sizeof(V));
                    std::memcpy(vec.data(), raw_bytes.data(), allocated);
                },
                    native_variant);

                if constexpr (std::is_same_v<OutputType, std::vector<Kakshya::DataVariant>>) {
                    result.data = { std::move(native_variant) };
                } else if constexpr (std::is_same_v<OutputType, Kakshya::DataVariant>) {
                    result.data = std::move(native_variant);
                }
            }
            for (const auto& [idx, bytes] : raw.aux)
                result.metadata["gpu_output_" + std::to_string(idx)] = bytes;
            return result;
        }

        const size_t total = std::accumulate(channels.begin(), channels.end(), size_t { 0 },
            [](size_t s, const auto& ch) { return s + ch.size(); });
        if (!raw.primary.empty() && !channels.empty() && raw.primary.size() >= total) {
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
