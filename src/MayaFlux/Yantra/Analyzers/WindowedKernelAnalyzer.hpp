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
 * Layout, set 0: binding 0 is the samples (float, read), binding 1 the
 * parameters (float, read; one zero when none are given), binding 2 the
 * results (float, written; outputs values per window, window major). The push
 * constants are five uints: window_length, stride, offset, window_count and
 * outputs. The workgroup size is 64 in x.
 *
 * A body sees those names, the window index i, window_base (the first sample
 * of the window), result_base (the first result of the window) and
 * sample_at(window, k). Windows past window_count return before the body runs.
 * The kernel must write every result it declares.
 */
inline constexpr uint32_t windowed_kernel_workgroup = 64;

/**
 * @brief The binding layout of a windowed kernel.
 */
[[nodiscard]] inline std::vector<GpuBufferBinding> windowed_kernel_layout()
{
    return {
        { .set = 0, .binding = 0, .direction = GpuBufferBinding::Direction::INPUT, .element_type = GpuBufferBinding::ElementType::FLOAT32 },
        { .set = 0, .binding = 1, .direction = GpuBufferBinding::Direction::INPUT, .element_type = GpuBufferBinding::ElementType::FLOAT32 },
        { .set = 0, .binding = 2, .direction = GpuBufferBinding::Direction::OUTPUT, .element_type = GpuBufferBinding::ElementType::FLOAT32 },
    };
}

/**
 * @brief Config of a GLSL body wrapped in the layout.
 * @param body Statements run for each window.
 * @return A config whose shader_id is INVALID_SHADER when the body does not compile.
 */
[[nodiscard]] inline GpuComputeConfig windowed_kernel_config(const std::string& body)
{
    using Portal::Graphics::BindingDirection;

    std::string text;
    text += "    if (i >= window_count) { return; }\n";
    text += "    uint window_base = offset + i * stride;\n";
    text += "    uint result_base = i * outputs;\n";
    text += body;

    return config_from_spec(Portal::Graphics::ShaderSpec::Assemble {}
            .ssbo("samples", BindingDirection::Input, Kakshya::GpuDataFormat::FLOAT32)
            .ssbo("parameters", BindingDirection::Input, Kakshya::GpuDataFormat::FLOAT32)
            .ssbo("results", BindingDirection::Output, Kakshya::GpuDataFormat::FLOAT32)
            .pc("window_length", Kakshya::GpuDataFormat::UINT32)
            .pc("stride", Kakshya::GpuDataFormat::UINT32)
            .pc("offset", Kakshya::GpuDataFormat::UINT32)
            .pc("window_count", Kakshya::GpuDataFormat::UINT32)
            .pc("outputs", Kakshya::GpuDataFormat::UINT32)
            .function("float", "sample_at", "uint window, uint k",
                "    return samples[pc.offset + window * pc.stride + k];")
            .kernel(Portal::Graphics::KernelSource { .body = std::move(text) })
            .workgroup(windowed_kernel_workgroup)
            .build());
}

/**
 * @brief Config of a .comp file that follows the layout.
 */
[[nodiscard]] inline GpuComputeConfig windowed_kernel_file_config(std::string path)
{
    return GpuComputeConfig {
        .shader_path = std::move(path),
        .workgroup_size = { windowed_kernel_workgroup, 1, 1 },
        .push_constant_size = 5 * sizeof(uint32_t),
    };
}

/**
 * @brief Run a windowed kernel over every window.
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
    if (geometry.count == 0 || outputs == 0) {
        return {};
    }
    const std::array<uint32_t, 5> push { geometry.length, geometry.stride, geometry.offset, geometry.count, outputs };
    const size_t result_bytes = static_cast<size_t>(geometry.count) * outputs * sizeof(float);
    const float none = 0.0F;

    context.record_stages({ DependencyStage {
        .config = config,
        .stage_fn = [samples, parameters, &none, result_bytes, push](GpuDispatchCore& core) {
            core.set_binding_data(0, samples);
            core.set_binding_data(1, parameters.empty() ? std::span<const float>(&none, 1) : parameters);
            core.set_output_size(2, result_bytes);
            core.set_push_constants(push);
        },
        .explicit_groups = std::array<uint32_t, 3> { (geometry.count + windowed_kernel_workgroup - 1) / windowed_kernel_workgroup, 1U, 1U },
    } });

    std::vector<float> results(static_cast<size_t>(geometry.count) * outputs);
    context.download_binding(2, results.data(), result_bytes);
    return results;
}

/**
 * @struct WindowedKernelConfig
 * @brief Settings of a WindowedKernelAnalyzer.
 *
 * Exactly one of kernel and shader_path is set. kernel is the body of the
 * per-window GLSL function, in the layout described above windowed_kernel_workgroup.
 * shader_path names a whole .comp file written against the same layout.
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
            });
    }

    /** Results the kernel writes for each window. */
    uint32_t outputs { 1 };
    /** GLSL statements run for each window. */
    std::string kernel;
    /** A .comp file following the windowed kernel layout, used instead of kernel. */
    std::string shader_path;
    /** Floats the kernel reads as parameters[k]. */
    std::vector<float> parameters;
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
            analysis.channels.resize(data_span.size());

            std::vector<std::vector<double>> channel_values;
            channel_values.reserve(data_span.size());

            for (size_t channel = 0; channel < data_span.size(); ++channel) {
                const std::span<const double> samples { data_span[channel].data(), data_span[channel].size() };
                const size_t windows = samples.size() < cfg.window_size
                    ? 0
                    : (samples.size() - cfg.window_size) / cfg.hop_size + 1;

                auto& result = analysis.channels[channel];
                result.outputs = cfg.outputs;
                for (size_t first = 0; first < windows; first += k_chunk_windows) {
                    const size_t count = std::min(k_chunk_windows, windows - first);
                    run_chunk(context, result, samples, first, count, cfg);
                }
                channel_values.emplace_back(result.values.begin(), result.values.end());
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
    }

    ShaderExecutionContext<>& context(const WindowedKernelConfig& cfg) const
    {
        const std::string key = cfg.kernel.empty() ? "file:" + cfg.shader_path : "body:" + cfg.kernel;
        if (!m_context || m_key != key) {
            GpuComputeConfig gpu = cfg.kernel.empty()
                ? windowed_kernel_file_config(cfg.shader_path)
                : windowed_kernel_config(cfg.kernel);
            if (gpu.shader_id == Portal::Graphics::INVALID_SHADER && gpu.shader_path.empty()) {
                error<std::runtime_error>(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
                    std::source_location::current(), "The kernel did not compile; the shader log has the compiler's message");
            }
            m_context = std::make_unique<ShaderExecutionContext<>>(gpu, windowed_kernel_layout(), "WindowedKernel");
            m_gpu = std::move(gpu);
            m_key = key;
        }
        return *m_context;
    }

    void run_chunk(ShaderExecutionContext<>& context, ChannelWindowedKernel& out, std::span<const double> samples,
        size_t first, size_t count, const WindowedKernelConfig& cfg) const
    {
        const size_t begin = first * cfg.hop_size;
        const size_t end = (first + count - 1) * cfg.hop_size + cfg.window_size;
        const std::vector<float> chunk(samples.begin() + static_cast<std::ptrdiff_t>(begin),
            samples.begin() + static_cast<std::ptrdiff_t>(end));
        const SegmentGeometry geometry { .length = cfg.window_size, .stride = cfg.hop_size, .offset = 0, .count = static_cast<uint32_t>(count) };

        const auto values = run_windowed_kernel(context, m_gpu, chunk, geometry, cfg.outputs, cfg.parameters);
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
