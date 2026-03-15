#pragma once

#include "GpuExecutionContext.hpp"

namespace MayaFlux::Yantra {

/**
 * @class ShaderExecutionContext
 * @brief Concrete GpuExecutionContext for a single fixed shader with fixed bindings.
 *
 * The standard path for attaching GPU dispatch to any ComputeOperation via
 * ComputeOperation::set_gpu_backend(). Bindings are declared at construction
 * or built incrementally through the fluent API. The owning ComputeOperation
 * provides category identity, parameter system, and CPU fallback.
 *
 * Construction followed by fluent configuration:
 * @code
 * auto executor = std::make_shared<ShaderExecutionContext<>>(
 *     GpuShaderConfig { "graph_build.comp", { 256, 1, 1 }, sizeof(GraphBuildPC) });
 * executor->input(positions)
 *          .input(attributes)
 *          .output(k_max_edges * 2 * sizeof(float))
 *          .output(sizeof(uint32_t), GpuBufferBinding::ElementType::UINT32)
 *          .push(pc);
 *
 * my_operation->set_gpu_backend(executor);
 * @endcode
 *
 * Or with explicit binding indices when order cannot be inferred:
 * @code
 * executor->input(0, positions)
 *          .inout(1, data)
 *          .output(2, output_bytes);
 * @endcode
 *
 * Output readback after pipeline execution:
 * @code
 * auto edges = ShaderExecutionContext<>::read_output<float>(result, 2);
 * auto count = ShaderExecutionContext<>::read_output<uint32_t>(result, 3)[0];
 * @endcode
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
     * @brief Construct with shader config and optional pre-built binding list.
     *
     * Bindings may be supplied here or added incrementally via the fluent API.
     * Mixing both is valid — fluent calls append after any pre-built bindings.
     *
     * @param config   Shader path, workgroup size, push constant size.
     * @param bindings Pre-built descriptor layout. Empty by default.
     * @param name     Executor name for logging and error messages.
     */
    explicit ShaderExecutionContext(
        GpuShaderConfig config,
        std::vector<GpuBufferBinding> bindings = {},
        std::string name = "ShaderExecutionContext")
        : GpuExecutionContext<InputType, OutputType>(std::move(config))
        , m_bindings(std::move(bindings))
        , m_name(std::move(name))
    {
    }

    //==========================================================================
    // Fluent binding API
    //==========================================================================

    /**
     * @brief Add an INPUT binding, inferring the next available binding index.
     * @tparam T     Element type of the data vector.
     * @param data   Data to upload for this binding.
     * @param type   Element type hint for the shader (default: FLOAT32).
     * @return Reference to this executor for chaining.
     */
    template <typename T>
    ShaderExecutionContext& input(const std::vector<T>& data,
        GpuBufferBinding::ElementType type = GpuBufferBinding::ElementType::FLOAT32)
    {
        const uint32_t idx = next_binding_index();
        m_bindings.push_back({ .set = 0,
            .binding = idx,
            .direction = GpuBufferBinding::Direction::INPUT,
            .element_type = type });
        this->set_binding_data(idx, data);
        return *this;
    }

    /**
     * @brief Add an INPUT binding at an explicit index.
     * @tparam T       Element type of the data vector.
     * @param binding  Binding index.
     * @param data     Data to upload for this binding.
     * @param type     Element type hint for the shader (default: FLOAT32).
     * @return Reference to this executor for chaining.
     */
    template <typename T>
    ShaderExecutionContext& input(uint32_t binding, const std::vector<T>& data,
        GpuBufferBinding::ElementType type = GpuBufferBinding::ElementType::FLOAT32)
    {
        m_bindings.push_back({ .set = 0,
            .binding = binding,
            .direction = GpuBufferBinding::Direction::INPUT,
            .element_type = type });

        this->set_binding_data(binding, data);
        return *this;
    }

    /**
     * @brief Add an INPUT_OUTPUT binding, inferring the next available binding index.
     * @tparam T     Element type of the data vector.
     * @param data   Data to upload for this binding.
     * @param type   Element type hint for the shader (default: FLOAT32).
     * @return Reference to this executor for chaining.
     */
    template <typename T>
    ShaderExecutionContext& in_out(const std::vector<T>& data,
        GpuBufferBinding::ElementType type = GpuBufferBinding::ElementType::FLOAT32)
    {
        const uint32_t idx = next_binding_index();
        m_bindings.push_back({ .set = 0,
            .binding = idx,
            .direction = GpuBufferBinding::Direction::INPUT_OUTPUT,
            .element_type = type });

        this->set_binding_data(idx, data);
        return *this;
    }

    /**
     * @brief Add an INPUT_OUTPUT binding at an explicit index.
     * @tparam T       Element type of the data vector.
     * @param binding  Binding index.
     * @param data     Data to upload for this binding.
     * @param type     Element type hint for the shader (default: FLOAT32).
     * @return Reference to this executor for chaining.
     */
    template <typename T>
    ShaderExecutionContext& in_out(uint32_t binding, const std::vector<T>& data,
        GpuBufferBinding::ElementType type = GpuBufferBinding::ElementType::FLOAT32)
    {
        m_bindings.push_back({ .set = 0,
            .binding = binding,
            .direction = GpuBufferBinding::Direction::INPUT_OUTPUT,
            .element_type = type });

        this->set_binding_data(binding, data);
        return *this;
    }

    /**
     * @brief Declare an INPUT_OUTPUT binding without pre-staging data.
     *
     * The binding direction is registered but no data is uploaded at
     * configuration time. Dispatch stages data from the input Datum
     * automatically. Use when the shader reads and writes the same buffer
     * and the input arrives via apply_operation rather than set_binding_data.
     *
     * @param type  Element type hint for the shader (default: FLOAT32).
     * @return Reference to this executor for chaining.
     */
    ShaderExecutionContext& in_out(uint32_t binding,
        GpuBufferBinding::ElementType type = GpuBufferBinding::ElementType::FLOAT32)
    {
        m_bindings.push_back({ .set = 0,
            .binding = binding,
            .direction = GpuBufferBinding::Direction::INPUT_OUTPUT,
            .element_type = type });

        return *this;
    }

    /**
     * @brief Declare an INPUT_OUTPUT binding at an explicit index without
     *        pre-staging data.
     *
     * @param binding Binding index.
     * @param type    Element type hint for the shader (default: FLOAT32).
     * @return Reference to this executor for chaining.
     */
    ShaderExecutionContext& in_out(
        GpuBufferBinding::ElementType type = GpuBufferBinding::ElementType::FLOAT32)
    {
        const uint32_t idx = next_binding_index();
        m_bindings.push_back({ .set = 0,
            .binding = idx,
            .direction = GpuBufferBinding::Direction::INPUT_OUTPUT,
            .element_type = type });

        return *this;
    }

    /**
     * @brief Add an OUTPUT binding, inferring the next available binding index.
     * @param byte_size Allocation size in bytes.
     * @param type      Element type hint for the shader (default: FLOAT32).
     * @return Reference to this executor for chaining.
     */
    ShaderExecutionContext& output(size_t byte_size,
        GpuBufferBinding::ElementType type = GpuBufferBinding::ElementType::FLOAT32)
    {
        const uint32_t idx = next_binding_index();
        m_bindings.push_back({ .set = 0,
            .binding = idx,
            .direction = GpuBufferBinding::Direction::OUTPUT,
            .element_type = type });

        this->set_output_size(idx, byte_size);
        return *this;
    }

    /**
     * @brief Add an OUTPUT binding at an explicit index.
     * @param binding   Binding index.
     * @param byte_size Allocation size in bytes.
     * @param type      Element type hint for the shader (default: FLOAT32).
     * @return Reference to this executor for chaining.
     */
    ShaderExecutionContext& output(uint32_t binding, size_t byte_size,
        GpuBufferBinding::ElementType type = GpuBufferBinding::ElementType::FLOAT32)
    {
        m_bindings.push_back({ .set = 0,
            .binding = binding,
            .direction = GpuBufferBinding::Direction::OUTPUT,
            .element_type = type });

        this->set_output_size(binding, byte_size);
        return *this;
    }

    /**
     * @brief Set push constants from a trivially copyable struct or value.
     *
     * Fluent alias for GpuExecutionContext::set_push_constants<T>.
     *
     * @tparam T   Push constant type. Must match shader layout exactly.
     * @param data Push constant data.
     * @return Reference to this executor for chaining.
     */
    template <typename T>
    ShaderExecutionContext& push(const T& data)
    {
        this->set_push_constants(data);
        return *this;
    }

    /**
     * @brief Configure multi-pass (CHAINED) dispatch.
     *
     * Stores pass count and push constant updater so the caller never
     * touches ExecutionContext::execution_metadata by string key.
     * The CHAINED mode is activated automatically in execute() when
     * a multipass configuration is present.
     *
     * @param pass_count  Total number of passes to dispatch.
     * @param pc_updater  Called before each pass with (pass_index, push_constant_ptr).
     * @return Reference to this executor for chaining.
     */
    ShaderExecutionContext& set_multipass(
        uint32_t pass_count,
        std::function<void(uint32_t, void*)> pc_updater)
    {
        m_multipass_count = pass_count;
        m_multipass_updater = std::move(pc_updater);
        return *this;
    }

    //==========================================================================
    // Output readback
    //==========================================================================

    /**
     * @brief Read a typed output buffer from a pipeline result Datum.
     *
     * Replaces the manual any_cast + reinterpret_cast pattern at call sites.
     * Copies the raw bytes from metadata into a typed vector.
     *
     * @tparam T            Element type to interpret the buffer as.
     * @param result        Datum returned by ComputationPipeline::process or
     *                      ComputeOperation::apply_operation.
     * @param binding_index Binding index matching the OUTPUT or INPUT_OUTPUT
     *                      declaration.
     * @return Vector of T with element count derived from raw byte size.
     *
     * @code
     * auto edges = ShaderExecutionContext<>::read_output<float>(result, 2);
     * auto count = ShaderExecutionContext<>::read_output<uint32_t>(result, 3)[0];
     * @endcode
     */
    template <typename T>
    static std::vector<T> read_output(
        const Datum<std::vector<Kakshya::DataVariant>>& result,
        size_t binding_index)
    {
        const auto key = "gpu_output_" + std::to_string(binding_index);
        const auto& raw = std::any_cast<const std::vector<uint8_t>&>(
            result.metadata.at(key));
        const size_t count = raw.size() / sizeof(T);
        std::vector<T> out(count);
        std::memcpy(out.data(), raw.data(), count * sizeof(T));
        return out;
    }

protected:
    /**
     * @brief Returns the binding list declared via constructor or fluent API.
     */
    [[nodiscard]] std::vector<GpuBufferBinding> declare_buffer_bindings() const override
    {
        return m_bindings;
    }

    /**
     * @brief Injects multipass configuration into the context before dispatch
     *        when set_multipass() has been called.
     */
    output_type execute(const input_type& input, const ExecutionContext& ctx)
    {
        if (m_multipass_count > 0 && m_multipass_updater) {
            ExecutionContext chained = ctx;
            chained.mode = ExecutionMode::CHAINED;
            chained.execution_metadata["pass_count"] = m_multipass_count;
            chained.execution_metadata["pc_updater"] = m_multipass_updater;
            return GpuExecutionContext<InputType, OutputType>::execute(input, chained);
        }
        return GpuExecutionContext<InputType, OutputType>::execute(input, ctx);
    }

private:
    std::vector<GpuBufferBinding> m_bindings;
    std::string m_name;
    uint32_t m_multipass_count { 0 };
    std::function<void(uint32_t, void*)> m_multipass_updater;

    /**
     * @brief Returns one past the highest binding index currently registered.
     *
     * Used by the no-index fluent overloads to append sequentially after
     * any existing bindings, including those set by explicit-index calls.
     */
    [[nodiscard]] uint32_t next_binding_index() const
    {
        if (m_bindings.empty())
            return 0;
        return std::ranges::max(
                   m_bindings | std::views::transform([](const GpuBufferBinding& b) { return b.binding; }))
            + 1;
    }
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
