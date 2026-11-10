#pragma once

#include "ShaderExecutionContext.hpp"
#include "ShaderSpecBinding.hpp"

#include "Audio/SegmentGeometry.hpp"

/**
 * @file WindowedKernelContext.hpp
 * @brief Runs a supplied compute kernel once per window of a float sequence.
 */

namespace MayaFlux::Yantra {

/**
 * @class WindowedKernelContext
 * @brief One invocation per window, with a fixed binding layout the kernel writes against.
 *
 * Window w covers window_length samples starting at offset + w * stride. The
 * kernel is either a GLSL body, wrapped by kernel_config, or a whole .comp
 * file that follows the same layout. Both run on native float, so results are
 * float32 and not exact against a double reference.
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
class WindowedKernelContext : public ShaderExecutionContext<> {
public:
    /** Invocations per workgroup, which a .comp file must declare as local_size_x. */
    static constexpr uint32_t workgroup = 64;

    /**
     * @param config Shader to run. Its workgroup size and push constant size are the layout's own.
     */
    explicit WindowedKernelContext(GpuComputeConfig config)
        : ShaderExecutionContext<>(std::move(config), layout(), "WindowedKernelContext")
    {
    }

    /**
     * @brief Config of a GLSL body wrapped in the layout.
     * @param body Statements run for each window.
     * @return A config whose shader_id is INVALID_SHADER when the body does not compile.
     */
    [[nodiscard]] static GpuComputeConfig kernel_config(const std::string& body)
    {
        return config_from_spec(kernel_spec(body));
    }

    /**
     * @brief Config of a .comp file that follows the layout.
     */
    [[nodiscard]] static GpuComputeConfig file_config(std::string path)
    {
        return GpuComputeConfig {
            .shader_path = std::move(path),
            .workgroup_size = { workgroup, 1, 1 },
            .push_constant_size = sizeof(Push),
        };
    }

    /**
     * @brief Run the kernel over every window.
     * @param samples Floats the windows index into.
     * @param geometry Window length, stride, first offset and count. Every window must lie inside samples.
     * @param outputs Results per window.
     * @param parameters Floats the kernel reads from binding 1.
     * @return geometry.count * outputs floats, window major, empty when there is nothing to run.
     */
    [[nodiscard]] std::vector<float> run(std::span<const float> samples, const SegmentGeometry& geometry,
        uint32_t outputs, std::span<const float> parameters)
    {
        if (geometry.count == 0 || outputs == 0) {
            return {};
        }
        m_windows = geometry.count;

        set_binding_data(0, samples);
        const float none = 0.0F;
        set_binding_data(1, parameters.empty() ? std::span<const float>(&none, 1) : parameters);
        set_output_size(2, static_cast<size_t>(geometry.count) * outputs * sizeof(float));
        set_push_constants(Push {
            .window_length = geometry.length,
            .stride = geometry.stride,
            .offset = geometry.offset,
            .window_count = geometry.count,
            .outputs = outputs });

        const auto result = execute(Datum<std::vector<Kakshya::DataVariant>> {}, ExecutionContext {});
        return read_output<float>(result, 2);
    }

protected:
    /**
     * @brief One invocation per window.
     */
    [[nodiscard]] std::array<uint32_t, 3> calculate_dispatch_size(
        size_t /*total_elements*/, const DataStructureInfo& /*structure_info*/) const override
    {
        return { (m_windows + workgroup - 1) / workgroup, 1U, 1U };
    }

private:
    struct Push {
        uint32_t window_length;
        uint32_t stride;
        uint32_t offset;
        uint32_t window_count;
        uint32_t outputs;
    };

    static std::vector<GpuBufferBinding> layout()
    {
        return {
            { .set = 0, .binding = 0, .direction = GpuBufferBinding::Direction::INPUT, .element_type = GpuBufferBinding::ElementType::FLOAT32 },
            { .set = 0, .binding = 1, .direction = GpuBufferBinding::Direction::INPUT, .element_type = GpuBufferBinding::ElementType::FLOAT32 },
            { .set = 0, .binding = 2, .direction = GpuBufferBinding::Direction::OUTPUT, .element_type = GpuBufferBinding::ElementType::FLOAT32 },
        };
    }

    static Portal::Graphics::ShaderSpec kernel_spec(const std::string& body)
    {
        using Portal::Graphics::BindingDirection;

        std::string text;
        text += "    if (i >= window_count) { return; }\n";
        text += "    uint window_base = offset + i * stride;\n";
        text += "    uint result_base = i * outputs;\n";
        text += body;

        return Portal::Graphics::ShaderSpec::Assemble {}
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
            .workgroup(workgroup)
            .build();
    }

    uint32_t m_windows { 1 };
};

} // namespace MayaFlux::Yantra
