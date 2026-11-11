#pragma once

#include "MayaFlux/Yantra/OperationSpec/OperationHelper.hpp"

#include "Analyzer.hpp"
#include <Eigen/Dense>

#include "MayaFlux/Yantra/Executors/Audio/SegmentGeometry.hpp"
#include "MayaFlux/Yantra/Executors/ShaderExecutionContext.hpp"
#include "MayaFlux/Yantra/Executors/ShaderSpecBinding.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

/**
 * @file WindowedKernelAnalyzer.hpp
 * @brief Runs a supplied compute kernel over every window of a sequence.
 *
 * The extension counterpart of the library's exact analyzers: the kernel is
 * written by the caller, in GLSL, and runs on native float.
 */

namespace MayaFlux::Yantra {

/**
 * @brief Where a windowed kernel is written against.
 *
 * The kernel runs once per window of a float sequence. Window w covers
 * window_length samples starting at offset + w * stride. It is either a GLSL
 * body, wrapped by windowed_kernel_config, or a whole .comp file that follows
 * the same layout. Both run on native float, so results are float32 and not
 * exact against a double reference.
 *
 * Layout, set 0: bindings 0 to inputs - 1 are the samples (float, read), then
 * the parameters (float, read; one zero when none are given), then the results
 * (float, written; outputs values per window, window major). With one input
 * the bindings are 0, 1 and 2. The push constants are uints: window_length,
 * stride, offset, window_count and outputs, then a stride and an offset for each
 * further input. The window length is shared by every input.
 *
 * A body sees those names, the first input as samples with sample_at(window, k),
 * the further inputs as samples2 to samples4 with sample2_at to sample4_at and
 * their stride2, offset2 and so on, window_base (the first sample of the first
 * input's window) and result_base (the first result of the window). The kernel
 * must write every result it declares.
 *
 * With one lane the workgroup is 64 and each invocation runs one window: the
 * window index is i, and windows past window_count return before the body runs.
 *
 * With lanes of 2 or more, a power of two up to 1024, one workgroup runs one
 * window and its lanes share the window's work. The body sees window, lane and
 * lanes, and these functions, which every lane of the window must call together,
 * outside any branch that some lanes skip:
 *
 *   float lane_reduce_sum(float v)   the sum of v over the lanes, in a fixed order
 *   float lane_reduce_max(float v)   the largest v
 *   float lane_reduce_min(float v)   the smallest v
 *   uint lane_first(bool condition)  the lowest lane whose condition holds, or lanes when none
 *
 * Every lane receives the result. Only one lane should write the results, for
 * example lane 0. The index of a maximum is lane_first(v == lane_reduce_max(v)).
 * A .comp file follows the same bindings and push constants, with a workgroup
 * of lanes, or 64 for one lane, and a group count of window_count for lanes of
 * 2 or more.
 */
inline constexpr uint32_t windowed_kernel_workgroup = 64;
inline constexpr uint32_t windowed_kernel_max_inputs = 4;
inline constexpr uint32_t windowed_kernel_max_lanes = 1024;

/**
 * @struct WindowedKernelShape
 * @brief How many lanes share a window and how many sequences a kernel reads.
 *
 * The default is the original layout, one invocation per window over one
 * sequence, and it never changes.
 */
struct WindowedKernelShape {
    uint32_t lanes { 1 };
    uint32_t inputs { 1 };

    [[nodiscard]] uint32_t workgroup() const noexcept { return lanes > 1 ? lanes : windowed_kernel_workgroup; }
    [[nodiscard]] uint32_t parameters_binding() const noexcept { return inputs; }
    [[nodiscard]] uint32_t results_binding() const noexcept { return inputs + 1; }
    [[nodiscard]] uint32_t push_words() const noexcept { return 5 + 2 * (inputs - 1); }
    [[nodiscard]] uint32_t groups(uint32_t windows) const noexcept
    {
        return lanes > 1 ? windows : (windows + windowed_kernel_workgroup - 1) / windowed_kernel_workgroup;
    }
    [[nodiscard]] bool operator==(const WindowedKernelShape&) const noexcept = default;
};

/**
 * @brief Why a shape cannot be used, empty when it can.
 */
[[nodiscard]] inline std::string windowed_kernel_diagnose(const WindowedKernelShape& shape)
{
    if (shape.inputs == 0 || shape.inputs > windowed_kernel_max_inputs) {
        return "A windowed kernel reads 1 to " + std::to_string(windowed_kernel_max_inputs) + " inputs";
    }
    if (shape.lanes == 0 || shape.lanes > windowed_kernel_max_lanes || (shape.lanes & (shape.lanes - 1)) != 0) {
        return "Lanes must be 1 or a power of two up to " + std::to_string(windowed_kernel_max_lanes);
    }
    return {};
}

/**
 * @brief The binding layout of a windowed kernel.
 */
[[nodiscard]] inline std::vector<GpuBufferBinding> windowed_kernel_layout(const WindowedKernelShape& shape = {})
{
    std::vector<GpuBufferBinding> layout;
    for (uint32_t binding = 0; binding < shape.inputs; ++binding) {
        layout.push_back({ .set = 0, .binding = binding, .direction = GpuBufferBinding::Direction::INPUT, .element_type = GpuBufferBinding::ElementType::FLOAT32 });
    }
    layout.push_back({ .set = 0, .binding = shape.parameters_binding(), .direction = GpuBufferBinding::Direction::INPUT, .element_type = GpuBufferBinding::ElementType::FLOAT32 });
    layout.push_back({ .set = 0, .binding = shape.results_binding(), .direction = GpuBufferBinding::Direction::OUTPUT, .element_type = GpuBufferBinding::ElementType::FLOAT32 });
    return layout;
}

namespace detail {

    inline std::string lane_preamble(uint32_t lanes)
    {
        const std::string n = std::to_string(lanes);
        return "shared float lane_scratch[" + n + "];\nshared uint lane_scratch_u[" + n + "];";
    }

    inline std::string lane_reduction(std::string_view scratch, std::string_view type, std::string_view combine)
    {
        std::string body;
        body += "    uint l = gl_LocalInvocationID.x;\n";
        body += "    " + std::string(scratch) + "[l] = v;\n";
        body += "    barrier();\n";
        body += "    for (uint s = gl_WorkGroupSize.x / 2u; s > 0u; s >>= 1u) {\n";
        body += "        if (l < s) { " + std::string(scratch) + "[l] = " + std::string(combine) + "; }\n";
        body += "        barrier();\n";
        body += "    }\n";
        body += "    " + std::string(type) + " r = " + std::string(scratch) + "[0];\n";
        body += "    barrier();\n";
        body += "    return r;";
        return body;
    }

} // namespace detail

/**
 * @brief Config of a GLSL body wrapped in the layout.
 * @param body Statements run for each window.
 * @param shape Lanes per window and inputs.
 * @return A config whose shader_id is INVALID_SHADER when the body does not compile or the shape is unusable.
 */
[[nodiscard]] inline GpuComputeConfig windowed_kernel_config(const std::string& body, const WindowedKernelShape& shape = {})
{
    using Portal::Graphics::BindingDirection;
    if (!windowed_kernel_diagnose(shape).empty()) {
        return {};
    }

    std::string text;
    if (shape.lanes > 1) {
        text += "    uint window = gl_WorkGroupID.x;\n";
        text += "    uint lane = gl_LocalInvocationID.x;\n";
        text += "    uint lanes = gl_WorkGroupSize.x;\n";
        text += "    if (window >= window_count) { return; }\n";
        text += "    uint window_base = offset + window * stride;\n";
        text += "    uint result_base = window * outputs;\n";
    } else {
        text += "    if (i >= window_count) { return; }\n";
        text += "    uint window_base = offset + i * stride;\n";
        text += "    uint result_base = i * outputs;\n";
    }
    text += body;

    auto assemble = Portal::Graphics::ShaderSpec::Assemble {};
    assemble.ssbo("samples", BindingDirection::Input, Kakshya::GpuDataFormat::FLOAT32);
    for (uint32_t input = 2; input <= shape.inputs; ++input) {
        assemble.ssbo("samples" + std::to_string(input), BindingDirection::Input, Kakshya::GpuDataFormat::FLOAT32);
    }
    assemble.ssbo("parameters", BindingDirection::Input, Kakshya::GpuDataFormat::FLOAT32)
        .ssbo("results", BindingDirection::Output, Kakshya::GpuDataFormat::FLOAT32)
        .pc("window_length", Kakshya::GpuDataFormat::UINT32)
        .pc("stride", Kakshya::GpuDataFormat::UINT32)
        .pc("offset", Kakshya::GpuDataFormat::UINT32)
        .pc("window_count", Kakshya::GpuDataFormat::UINT32)
        .pc("outputs", Kakshya::GpuDataFormat::UINT32);
    for (uint32_t input = 2; input <= shape.inputs; ++input) {
        assemble.pc("stride" + std::to_string(input), Kakshya::GpuDataFormat::UINT32)
            .pc("offset" + std::to_string(input), Kakshya::GpuDataFormat::UINT32);
    }
    assemble.function("float", "sample_at", "uint window, uint k",
        "    return samples[pc.offset + window * pc.stride + k];");
    for (uint32_t input = 2; input <= shape.inputs; ++input) {
        const std::string tag = std::to_string(input);
        assemble.function("float", "sample" + tag + "_at", "uint window, uint k",
            "    return samples" + tag + "[pc.offset" + tag + " + window * pc.stride" + tag + " + k];");
    }
    if (shape.lanes > 1) {
        assemble.preamble(detail::lane_preamble(shape.lanes))
            .function("float", "lane_reduce_sum", "float v",
                detail::lane_reduction("lane_scratch", "float", "lane_scratch[l] + lane_scratch[l + s]"))
            .function("float", "lane_reduce_max", "float v",
                detail::lane_reduction("lane_scratch", "float", "max(lane_scratch[l], lane_scratch[l + s])"))
            .function("float", "lane_reduce_min", "float v",
                detail::lane_reduction("lane_scratch", "float", "min(lane_scratch[l], lane_scratch[l + s])"))
            .function("uint", "lane_first", "bool condition",
                "    uint v = condition ? gl_LocalInvocationID.x : gl_WorkGroupSize.x;\n"
                + detail::lane_reduction("lane_scratch_u", "uint", "min(lane_scratch_u[l], lane_scratch_u[l + s])"));
    }

    return config_from_spec(assemble.kernel(Portal::Graphics::KernelSource { .body = std::move(text) })
            .workgroup(shape.workgroup())
            .build());
}

/**
 * @brief Config of a .comp file that follows the layout.
 */
[[nodiscard]] inline GpuComputeConfig windowed_kernel_file_config(std::string path, const WindowedKernelShape& shape = {})
{
    return GpuComputeConfig {
        .shader_path = std::move(path),
        .workgroup_size = { shape.workgroup(), 1, 1 },
        .push_constant_size = shape.push_words() * sizeof(uint32_t),
    };
}

/**
 * @struct WindowedInput
 * @brief One sequence a windowed kernel reads, and how its windows are laid out.
 *
 * Window w of the input starts at offset + w * stride. A resident input is not
 * staged: its binding already resolves to a buffer of this context or one
 * imported from another with import_shared.
 */
struct WindowedInput {
    std::span<const float> samples;
    uint32_t stride { 0 };
    uint32_t offset { 0 };
    bool resident { false };

    /** @brief An input that is already on the device. */
    [[nodiscard]] static WindowedInput on_device(uint32_t stride, uint32_t offset = 0)
    {
        return { .stride = stride, .offset = offset, .resident = true };
    }
};

/**
 * @brief Run a windowed kernel over every window of one or more sequences.
 * @param context Context built with config and windowed_kernel_layout(shape).
 * @param config The config the context was built from.
 * @param shape The shape the config was built for.
 * @param inputs One entry per input of the shape. Every window must lie inside its input.
 * @param window_length Samples in each window, the same for every input.
 * @param window_count Windows to run.
 * @param outputs Results per window.
 * @param parameters Floats the kernel reads from the parameters binding.
 * @return window_count * outputs floats, window major, empty when there is nothing to run.
 * @throws std::invalid_argument when the number of inputs does not match the shape.
 */
[[nodiscard]] inline std::vector<float> run_windowed_kernel(
    ShaderExecutionContext<>& context, const GpuComputeConfig& config, const WindowedKernelShape& shape,
    std::span<const WindowedInput> inputs, uint32_t window_length, uint32_t window_count,
    uint32_t outputs, std::span<const float> parameters)
{
    if (inputs.size() != shape.inputs) {
        error<std::invalid_argument>(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
            std::source_location::current(), "A kernel of {} inputs was given {}", shape.inputs, inputs.size());
    }
    if (window_count == 0 || outputs == 0) {
        return {};
    }

    std::vector<uint32_t> push { window_length, inputs[0].stride, inputs[0].offset, window_count, outputs };
    for (size_t input = 1; input < inputs.size(); ++input) {
        push.push_back(inputs[input].stride);
        push.push_back(inputs[input].offset);
    }
    const size_t result_bytes = static_cast<size_t>(window_count) * outputs * sizeof(float);
    const float none = 0.0F;
    const std::vector<WindowedInput> staged(inputs.begin(), inputs.end());

    context.record_stages({ DependencyStage {
        .config = config,
        .stage_fn = [staged, parameters, &none, result_bytes, push, shape](GpuDispatchCore& core) {
            for (size_t input = 0; input < staged.size(); ++input) {
                if (!staged[input].resident) {
                    core.set_binding_data(input, staged[input].samples);
                }
            }
            core.set_binding_data(shape.parameters_binding(), parameters.empty() ? std::span<const float>(&none, 1) : parameters);
            core.set_output_size(shape.results_binding(), result_bytes);
            core.set_push_constants(push.data(), push.size() * sizeof(uint32_t));
        },
        .explicit_groups = std::array<uint32_t, 3> { shape.groups(window_count), 1U, 1U },
    } });

    std::vector<float> results(static_cast<size_t>(window_count) * outputs);
    context.download_binding(shape.results_binding(), results.data(), result_bytes);
    return results;
}

/**
 * @brief Run a one input, one invocation per window kernel over every window.
 * @param context Context built with config and windowed_kernel_layout.
 * @param config The config the context was built from.
 * @param samples Floats the windows index into. Every window must lie inside it.
 * @param geometry Window length, stride, first offset and count.
 * @param outputs Results per window.
 * @param parameters Floats the kernel reads from binding 1.
 * @return geometry.count * outputs floats, window major, empty when there is nothing to run.
 */
[[nodiscard]] inline std::vector<float> run_windowed_kernel(
    ShaderExecutionContext<>& context, const GpuComputeConfig& config, std::span<const float> samples,
    const SegmentGeometry& geometry, uint32_t outputs, std::span<const float> parameters)
{
    const WindowedInput input { .samples = samples, .stride = geometry.stride, .offset = geometry.offset };
    return run_windowed_kernel(context, config, WindowedKernelShape {}, std::span<const WindowedInput>(&input, 1),
        geometry.length, geometry.count, outputs, parameters);
}

/**
 * @struct WindowedKernelConfig
 * @brief Settings of a WindowedKernelAnalyzer.
 *
 * Exactly one of kernel and shader_path is set. kernel is the body of the
 * per-window GLSL function, in the layout described above windowed_kernel_workgroup.
 * shader_path names a whole .comp file written against the same layout.
 *
 * lanes is how many invocations share a window, 1 or a power of two up to 1024.
 * inputs is how many sequences the kernel reads. With one input every channel is
 * analysed on its own. With more, the first inputs channels of the data are the
 * inputs of one kernel run, with the same window and hop, and the analysis has
 * one channel; windows run while every input has them.
 */
struct WindowedKernelConfig : AnalyzerConfig {
    static constexpr auto describe_facets()
    {
        return std::tuple_cat(
            AnalyzerConfig::describe_facets(),
            std::tuple {
                FacetMember<&WindowedKernelConfig::outputs> {},
                FacetMember<&WindowedKernelConfig::kernel> {},
                FacetMember<&WindowedKernelConfig::shader_path> {},
                FacetMember<&WindowedKernelConfig::parameters> {},
                FacetMember<&WindowedKernelConfig::lanes> {},
                FacetMember<&WindowedKernelConfig::inputs> {},
            });
    }

    [[nodiscard]] WindowedKernelShape shape() const noexcept { return { .lanes = lanes, .inputs = inputs }; }

    /** Results the kernel writes for each window. */
    uint32_t outputs { 1 };
    /** GLSL statements run for each window. */
    std::string kernel;
    /** A .comp file following the windowed kernel layout, used instead of kernel. */
    std::string shader_path;
    /** Floats the kernel reads as parameters[k]. */
    std::vector<float> parameters;
    /** Invocations that share one window. */
    uint32_t lanes { 1 };
    /** Sequences the kernel reads. */
    uint32_t inputs { 1 };
};

/**
 * @struct ChannelWindowedKernel
 * @brief Kernel results of one channel.
 */
struct MAYAFLUX_API ChannelWindowedKernel {
    /** Window major: window w, output j is values[w * outputs + j]. */
    std::vector<float> values;
    /** Results per window. */
    uint32_t outputs { 0 };
    /** First and one past the last sample of each window. */
    std::vector<std::pair<size_t, size_t>> window_positions;
};

/**
 * @struct WindowedKernelAnalysis
 * @brief Analysis result of a WindowedKernelAnalyzer.
 */
struct MAYAFLUX_API WindowedKernelAnalysis {
    static constexpr auto describe_facets()
    {
        return std::tuple {
            FacetMember<&WindowedKernelAnalysis::config> {},
            FacetMember<&WindowedKernelAnalysis::channels> {},
        };
    }

    /** Settings the analysis was produced under. */
    WindowedKernelConfig config;
    std::vector<ChannelWindowedKernel> channels;
};

/**
 * @class WindowedKernelAnalyzer
 * @brief Runs a caller's compute kernel once per window of every channel.
 *
 * Where WaveformAnalyzer and SpectralAnalyzer are fixed measurements computed
 * exactly, this is the place for a measurement the library does not have. The
 * kernel reads the window through sample_at and writes its results to the
 * result array; see the layout described above windowed_kernel_workgroup for the names it sees. Samples are
 * converted to float, the kernel runs in native float, and results are float,
 * so they agree with a double reference only to float precision.
 *
 * Nothing about the windows is specific to audio: a window is a run of
 * consecutive values of any one-dimensional sequence. It is on the vulkan
 * target, has no CPU twin, and is not registered with the default registry
 * because it has no fixed selection to resolve by. A kernel that does not
 * compile makes the analysis fail with the shader log's message.
 */
template <ComputeData InputType = std::vector<Kakshya::DataVariant>, ComputeData OutputType = Eigen::MatrixXd>
class WindowedKernelAnalyzer : public Analyzer<InputType, OutputType> {
public:
    using input_type = Datum<InputType>;
    using output_type = Datum<OutputType>;
    using base_type = Analyzer<InputType, OutputType>;
    using supports = Supports<WindowedKernelConfig>;

    static constexpr Target target = targets::vulkan;

    /**
     * @brief Construct from a kernel and its window settings.
     * @param config Settings, validated before they are stored.
     */
    explicit WindowedKernelAnalyzer(WindowedKernelConfig config)
    {
        validate_config(config);
        this->set_config(std::move(config));
    }

    [[nodiscard]] ConfigRefs configs() const override
    {
        return this->stored_configs(supports {});
    }

    /**
     * @brief Type-safe windowed kernel analysis
     */
    WindowedKernelAnalysis analyze_windows(const input_type& data)
    {
        auto result = this->analyze_data(data);
        return safe_any_cast_or_throw<WindowedKernelAnalysis>(result);
    }

    WindowedKernelAnalysis analyze_windows(const InputType& data)
    {
        return this->analyze_windows(input_type { data });
    }

    [[nodiscard]] WindowedKernelAnalysis get_windowed_kernel_analysis() const
    {
        return safe_any_cast_or_throw<WindowedKernelAnalysis>(this->get_current_analysis());
    }

    [[nodiscard]] AnalysisType get_analysis_type() const override
    {
        return AnalysisType::CUSTOM;
    }

    [[nodiscard]] std::string get_name() const override
    {
        return "WindowedKernelAnalyzer";
    }

    /**
     * @brief One while a device is up, zero otherwise.
     */
    static float fitness(const input_type& /*input*/)
    {
        return Portal::Graphics::get_shader_foundry().is_initialized() ? 1.0F : 0.0F;
    }

protected:
    output_type analyze_implementation(const input_type& input) override
    {
        try {
            const auto& cfg = this->template config<WindowedKernelConfig>();

            auto [data_span, structure_info] = OperationHelper::extract_structured_double(
                const_cast<input_type&>(input));

            auto& context = this->context(cfg);

            WindowedKernelAnalysis analysis;
            analysis.config = cfg;

            std::vector<std::vector<double>> channel_values;

            if (cfg.inputs == 1) {
                analysis.channels.resize(data_span.size());
                channel_values.reserve(data_span.size());
                for (size_t channel = 0; channel < data_span.size(); ++channel) {
                    const std::array<std::span<const double>, 1> inputs { std::span<const double> { data_span[channel].data(), data_span[channel].size() } };
                    analyse(context, analysis.channels[channel], inputs, cfg);
                    channel_values.emplace_back(analysis.channels[channel].values.begin(), analysis.channels[channel].values.end());
                }
            } else {
                if (data_span.size() < cfg.inputs) {
                    error<std::invalid_argument>(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
                        std::source_location::current(), "A kernel of {} inputs needs that many channels and {} were given", cfg.inputs, data_span.size());
                }
                std::vector<std::span<const double>> inputs;
                for (uint32_t input = 0; input < cfg.inputs; ++input) {
                    inputs.emplace_back(data_span[input].data(), data_span[input].size());
                }
                analysis.channels.resize(1);
                analyse(context, analysis.channels[0], inputs, cfg);
                channel_values.emplace_back(analysis.channels[0].values.begin(), analysis.channels[0].values.end());
            }

            this->store_current_analysis(analysis);

            output_type output = this->convert_result(channel_values, structure_info);
            output.metadata = input.metadata;
            output.facets = input.facets;
            output.facets.set(analysis);
            return output;
        } catch (const std::exception& e) {
            MF_ERROR(Journal::Component::Yantra, Journal::Context::ComputeMatrix, "Windowed kernel analysis failed: {}", e.what());
            output_type error_result;
            error_result.metadata = input.metadata;
            error_result.facets = input.facets;
            return error_result;
        }
    }

private:
    static constexpr size_t k_chunk_windows = 16384;

    mutable std::unique_ptr<ShaderExecutionContext<>> m_context;
    mutable GpuComputeConfig m_gpu;
    mutable std::string m_key;

    static void validate_config(const WindowedKernelConfig& cfg)
    {
        const auto fail = [](std::string_view message) {
            error<std::invalid_argument>(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
                std::source_location::current(), "{}", message);
        };
        if (cfg.kernel.empty() == cfg.shader_path.empty()) {
            fail("Exactly one of kernel and shader_path must be set");
        }
        if (cfg.outputs == 0) {
            fail("A kernel must write at least one output per window");
        }
        if (cfg.window_size == 0 || cfg.hop_size == 0) {
            fail("Window and hop sizes must be greater than 0");
        }
        if (const std::string reason = windowed_kernel_diagnose(cfg.shape()); !reason.empty()) {
            fail(reason);
        }
    }

    ShaderExecutionContext<>& context(const WindowedKernelConfig& cfg) const
    {
        const auto shape = cfg.shape();
        const std::string key = (cfg.kernel.empty() ? "file:" + cfg.shader_path : "body:" + cfg.kernel)
            + "#" + std::to_string(shape.lanes) + "x" + std::to_string(shape.inputs);
        if (!m_context || m_key != key) {
            GpuComputeConfig gpu = cfg.kernel.empty()
                ? windowed_kernel_file_config(cfg.shader_path, shape)
                : windowed_kernel_config(cfg.kernel, shape);
            if (gpu.shader_id == Portal::Graphics::INVALID_SHADER && gpu.shader_path.empty()) {
                error<std::runtime_error>(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
                    std::source_location::current(), "The kernel did not compile; the shader log has the compiler's message");
            }
            m_context = std::make_unique<ShaderExecutionContext<>>(gpu, windowed_kernel_layout(shape), "WindowedKernel");
            m_gpu = std::move(gpu);
            m_key = key;
        }
        return *m_context;
    }

    void analyse(ShaderExecutionContext<>& context, ChannelWindowedKernel& result,
        std::span<const std::span<const double>> inputs, const WindowedKernelConfig& cfg) const
    {
        size_t shortest = inputs.front().size();
        for (const auto& input : inputs) {
            shortest = std::min(shortest, input.size());
        }
        const size_t windows = shortest < cfg.window_size ? 0 : (shortest - cfg.window_size) / cfg.hop_size + 1;

        result.outputs = cfg.outputs;
        for (size_t first = 0; first < windows; first += k_chunk_windows) {
            const size_t count = std::min(k_chunk_windows, windows - first);
            run_chunk(context, result, inputs, first, count, cfg);
        }
    }

    void run_chunk(ShaderExecutionContext<>& context, ChannelWindowedKernel& out,
        std::span<const std::span<const double>> inputs, size_t first, size_t count, const WindowedKernelConfig& cfg) const
    {
        const size_t begin = first * cfg.hop_size;
        const size_t end = (first + count - 1) * cfg.hop_size + cfg.window_size;

        std::vector<std::vector<float>> chunks;
        chunks.reserve(inputs.size());
        std::vector<WindowedInput> bound;
        bound.reserve(inputs.size());
        for (const auto& samples : inputs) {
            chunks.emplace_back(samples.begin() + static_cast<std::ptrdiff_t>(begin), samples.begin() + static_cast<std::ptrdiff_t>(end));
            bound.push_back({ .samples = chunks.back(), .stride = cfg.hop_size, .offset = 0 });
        }

        const auto values = run_windowed_kernel(context, m_gpu, cfg.shape(), bound, cfg.window_size,
            static_cast<uint32_t>(count), cfg.outputs, cfg.parameters);
        out.values.insert(out.values.end(), values.begin(), values.end());
        for (size_t w = 0; w < count; ++w) {
            const size_t start = (first + w) * cfg.hop_size;
            out.window_positions.emplace_back(start, start + cfg.window_size);
        }
    }
};

/// Standard windowed kernel analyzer: DataVariant -> MatrixXd
using StandardWindowedKernelAnalyzer = WindowedKernelAnalyzer<std::vector<Kakshya::DataVariant>, Eigen::MatrixXd>;

/// Container windowed kernel analyzer: SignalContainer -> MatrixXd
using ContainerWindowedKernelAnalyzer = WindowedKernelAnalyzer<std::shared_ptr<Kakshya::SignalSourceContainer>, Eigen::MatrixXd>;

} // namespace MayaFlux::Yantra
