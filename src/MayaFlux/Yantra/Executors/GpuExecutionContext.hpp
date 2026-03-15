#pragma once

#include "MayaFlux/Yantra/OperationSpec/ExecutionContext.hpp"
#include "MayaFlux/Yantra/OperationSpec/OperationHelper.hpp"

#include "GpuResourceManager.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Yantra {

/**
 * @class GpuExecutionContext
 * @brief Standalone GPU compute executor attachable to any ComputeOperation.
 *
 * Manages shader dispatch, resource lifecycle, and data marshalling for a
 * single compute shader. Attached to a ComputeOperation via
 * ComputeOperation::set_gpu_backend(), which delegates apply_operation_internal
 * to execute() when GPU is available.
 *
 * Subclasses may override:
 *   - declare_buffer_bindings   (descriptor layout)
 *   - on_before_gpu_dispatch    (push constants, per-dispatch config)
 *   - prepare_gpu_inputs        (custom marshalling)
 *   - collect_gpu_outputs       (custom readback)
 *   - calculate_dispatch_size   (driven by structure_info dimensions)
 *
 * @tparam InputType  ComputeData type accepted.
 * @tparam OutputType ComputeData type produced.
 */
template <ComputeData InputType = std::vector<Kakshya::DataVariant>,
    ComputeData OutputType = InputType>
class MAYAFLUX_API GpuExecutionContext {
public:
    using input_type = Datum<InputType>;
    using output_type = Datum<OutputType>;

    explicit GpuExecutionContext(GpuShaderConfig config)
        : m_gpu_config(std::move(config))
    {
    }

    ~GpuExecutionContext() = default;

    GpuExecutionContext(const GpuExecutionContext&) = delete;
    GpuExecutionContext& operator=(const GpuExecutionContext&) = delete;
    GpuExecutionContext(GpuExecutionContext&&) = delete;
    GpuExecutionContext& operator=(GpuExecutionContext&&) = delete;

    /**
     * @brief Set push constant data to be passed to the shader on dispatch.
     * @param data Pointer to raw push constant data.
     * @param bytes Size of the push constant data in bytes.
     */
    void set_push_constants(const void* data, size_t bytes)
    {
        m_push_constants.resize(bytes);
        std::memcpy(m_push_constants.data(), data, bytes);
    }

    /**
     * @brief Templated convenience for setting push constants from a struct or value.
     *        The data will be copied as raw bytes, so ensure it is trivially copyable
     *        and matches the shader's expected layout.
     *
     * @tparam T Type of the push constant data (e.g., a struct).
     * @param data The push constant data to set.
     */
    template <typename T>
    void set_push_constants(const T& data)
    {
        set_push_constants(&data, sizeof(T));
    }

    /**
     * @brief Supply pre-built typed data for a specific binding slot.
     *
     * Follows the same set_parameter pattern used across all Yantra operations.
     * When set, prepare_gpu_inputs uploads this data verbatim for the given
     * binding index, bypassing the default channel-flattening path.
     *
     * @tparam T    Trivially copyable element type.
     * @param index Binding index matching declare_buffer_bindings order.
     * @param data  Elements to upload.
     */
    template <typename T>
    void set_binding_data(size_t index, std::span<const T> data)
    {
        if (index >= m_binding_data.size()) {
            m_binding_data.resize(index + 1);
        }
        auto& slot = m_binding_data[index];
        slot.resize(data.size_bytes());
        std::memcpy(slot.data(), data.data(), data.size_bytes());
    }

    template <typename T>
    void set_binding_data(size_t index, const std::vector<T>& data)
    {
        set_binding_data(index, std::span<const T>(data));
    }

    /**
     * @brief Declare the byte capacity of an output binding independently of input data.
     *
     * Required when output size cannot be derived from the input (edge lists,
     * count buffers, histograms, etc.). Must be called before process().
     *
     * @param index     Binding index matching declare_buffer_bindings order.
     * @param byte_size Required allocation in bytes.
     */
    void set_output_size(size_t index, size_t byte_size)
    {
        if (index >= m_output_size_overrides.size()) {
            m_output_size_overrides.resize(index + 1, 0);
        }
        m_output_size_overrides[index] = byte_size;
    }

    /**
     * @brief Dispatch to GPU, selected by ExecutionContext mode.
     *
     * Called by ComputeOperation::apply_operation_internal when a GPU backend
     * is attached and ready. CHAINED mode routes to dispatch_gpu_chained for
     * multi-pass execution; all other modes use dispatch_gpu.
     *
     * @param input Input data to process.
     * @param ctx   Execution context. CHAINED mode requires pass_count and
     *              pc_updater keys in execution_metadata.
     * @throws std::runtime_error If GPU initialisation fails.
     */
    output_type execute(const input_type& input, const ExecutionContext& ctx)
    {
        if (ctx.mode == ExecutionMode::CHAINED) {
            return dispatch_gpu_chained(input, ctx);
        }
        if (ensure_gpu_ready()) {
            return dispatch_gpu(input);
        }

        error<std::runtime_error>(
            Journal::Component::Yantra,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "GpuExecutionContext: GPU initialisation failed and no CPU fallback exists at this level");
    }

    /**
     * @brief Check if GPU resources are initialised and ready for dispatch.
     * @return True if GPU is ready, false otherwise.
     */
    [[nodiscard]] bool is_gpu_ready() const { return m_resources.is_ready(); }

    /**
     * @brief Ensure GPU resources are initialised. If already ready, does nothing.
     * @return True if GPU is ready after this call, false if initialisation failed.
     */
    bool ensure_gpu_ready()
    {
        if (m_resources.is_ready()) {
            return true;
        }
        m_bindings = declare_buffer_bindings();
        return m_resources.initialise(m_gpu_config, m_bindings);
    }

protected:
    //==========================================================================
    // Subclass declaration points
    //==========================================================================

    /**
     * @brief Declare the storage buffers the shader expects.
     *
     * Default: one INPUT at (0,0), one OUTPUT at (0,1), both FLOAT32.
     */
    [[nodiscard]] virtual std::vector<GpuBufferBinding> declare_buffer_bindings() const
    {
        return {
            { .set = 0, .binding = 0, .direction = GpuBufferBinding::Direction::INPUT, .element_type = GpuBufferBinding::ElementType::FLOAT32 },
            { .set = 0, .binding = 1, .direction = GpuBufferBinding::Direction::OUTPUT, .element_type = GpuBufferBinding::ElementType::FLOAT32 },
        };
    }

    /**
     * @brief Called before dispatch. Override to write push constants.
     */
    virtual void on_before_gpu_dispatch(
        const std::vector<std::vector<double>>& channels,
        const DataStructureInfo& structure_info) { }

    /**
     * @brief Marshal channel data into GPU input buffers.
     *
     * Uses structure_info.modality and per-binding element_type to
     * determine cast strategy:
     *   - FLOAT32       — double-to-float concatenation of all channels.
     *   - UINT32/INT32  — reinterpret first channel's backing bytes directly.
     *   - PASSTHROUGH   — uploads whatever was staged via stage_passthrough().
     *
     * OUTPUT-only bindings are sized to match the input byte footprint.
     */
    virtual void prepare_gpu_inputs(
        const std::vector<std::vector<double>>& channels,
        const DataStructureInfo& structure_info)
    {
        flatten_channels_to_staging(channels, structure_info);
        const size_t float_byte_size = m_staging_floats.size() * sizeof(float);

        const size_t fallback_bytes = float_byte_size > 0
            ? float_byte_size
            : Kakshya::ContainerDataStructure::get_total_elements(structure_info.dimensions) * sizeof(float);

        for (size_t i = 0; i < m_bindings.size(); ++i) {
            const auto& b = m_bindings[i];

            if (i < m_binding_data.size() && !m_binding_data[i].empty()) {
                m_resources.ensure_buffer(i, m_binding_data[i].size());
                m_resources.upload_raw(i, m_binding_data[i].data(), m_binding_data[i].size());
                continue;
            }

            if (b.direction == GpuBufferBinding::Direction::OUTPUT) {
                const size_t sz = (i < m_output_size_overrides.size() && m_output_size_overrides[i] > 0)
                    ? m_output_size_overrides[i]
                    : fallback_bytes;
                m_resources.ensure_buffer(i, sz);
                if (i < m_output_size_overrides.size() && m_output_size_overrides[i] > 0) {
                    std::vector<uint8_t> zeros(sz, 0);
                    m_resources.upload_raw(i, zeros.data(), sz);
                }
                continue;
            }

            switch (b.element_type) {
            case GpuBufferBinding::ElementType::PASSTHROUGH:
                if (i < m_passthrough_bytes.size() && !m_passthrough_bytes[i].empty()) {
                    m_resources.ensure_buffer(i, m_passthrough_bytes[i].size());
                    m_resources.upload_raw(i,
                        m_passthrough_bytes[i].data(),
                        m_passthrough_bytes[i].size());
                }
                break;

            case GpuBufferBinding::ElementType::UINT32:
            case GpuBufferBinding::ElementType::INT32:
                if (!channels.empty()) {
                    const size_t raw_bytes = channels[0].size()
                        * (b.element_type == GpuBufferBinding::ElementType::UINT32
                                ? sizeof(uint32_t)
                                : sizeof(int32_t));
                    m_resources.ensure_buffer(i, raw_bytes);
                    m_resources.upload_raw(i,
                        reinterpret_cast<const uint8_t*>(channels[0].data()),
                        raw_bytes);
                }
                break;

            case GpuBufferBinding::ElementType::FLOAT32:
            default:
                m_resources.ensure_buffer(i, float_byte_size);
                m_resources.upload(i, m_staging_floats.data(), float_byte_size);
                break;
            }
        }
    }

    /**
     * @brief Read back GPU results and reconstruct IO.
     *
     * Default: read first OUTPUT/INPUT_OUTPUT buffer, split back into
     * per-channel doubles.
     */
    virtual output_type collect_gpu_outputs(
        const std::vector<std::vector<double>>& channels,
        const DataStructureInfo& structure_info)
    {
        output_type result;

        const size_t float_count = m_staging_floats.size();
        if (float_count > 0) {
            const size_t readback_index = find_first_output_index();
            const size_t allocated = m_resources.buffer_allocated_bytes(readback_index);
            const size_t byte_size = std::min(float_count * sizeof(float), allocated);
            const size_t capped_count = byte_size / sizeof(float);
            std::vector<float> result_f(capped_count);
            m_resources.download(readback_index, result_f.data(), byte_size);
            result = reconstruct_from_floats(result_f, channels, structure_info);
        }

        for (size_t i = 0; i < m_bindings.size(); ++i) {
            if ((m_bindings[i].direction == GpuBufferBinding::Direction::OUTPUT
                    || m_bindings[i].direction == GpuBufferBinding::Direction::INPUT_OUTPUT)
                && i < m_output_size_overrides.size()
                && m_output_size_overrides[i] > 0) {
                const size_t sz = m_output_size_overrides[i];
                std::vector<uint8_t> raw(sz);
                m_resources.download(i, reinterpret_cast<float*>(raw.data()), sz);
                result.metadata["gpu_output_" + std::to_string(i)] = std::move(raw);
            }
        }

        return result;
    }

    /**
     * @brief Calculate dispatch group counts from structure_info dimensions.
     *
     * Reads SPATIAL_X/Y/Z roles from structure_info.dimensions when present,
     * giving correct 2D/3D group counts for image and volume shaders without
     * any subclass override. Falls back to 1D element-count dispatch when no
     * spatial dimensions exist (audio, spectral, etc.).
     *
     * total_elements is provided for the fallback and for subclasses that
     * prefer to ignore structure_info.
     */
    [[nodiscard]] virtual std::array<uint32_t, 3> calculate_dispatch_size(
        size_t total_elements,
        const DataStructureInfo& structure_info) const
    {
        uint64_t sz_x = 0, sz_y = 0, sz_z = 0;
        for (const auto& dim : structure_info.dimensions) {
            switch (dim.role) {
            case Kakshya::DataDimension::Role::SPATIAL_X:
                sz_x = dim.size;
                break;
            case Kakshya::DataDimension::Role::SPATIAL_Y:
                sz_y = dim.size;
                break;
            case Kakshya::DataDimension::Role::SPATIAL_Z:
                sz_z = dim.size;
                break;
            default:
                break;
            }
        }

        if (sz_x > 0) {
            return {
                static_cast<uint32_t>((sz_x + m_gpu_config.workgroup_size[0] - 1) / m_gpu_config.workgroup_size[0]),
                sz_y > 0 ? static_cast<uint32_t>((sz_y + m_gpu_config.workgroup_size[1] - 1) / m_gpu_config.workgroup_size[1]) : 1U,
                sz_z > 0 ? static_cast<uint32_t>((sz_z + m_gpu_config.workgroup_size[2] - 1) / m_gpu_config.workgroup_size[2]) : 1U,
            };
        }

        const auto gx = static_cast<uint32_t>(
            (total_elements + m_gpu_config.workgroup_size[0] - 1) / m_gpu_config.workgroup_size[0]);
        return { gx, 1, 1 };
    }

    /**
     * @brief Stage raw bytes for a PASSTHROUGH binding prior to dispatch.
     * @param binding_index Index matching declare_buffer_bindings order.
     * @param data          Raw byte pointer.
     * @param byte_size     Size in bytes.
     */
    void stage_passthrough(size_t binding_index, const void* data, size_t byte_size)
    {
        if (binding_index >= m_passthrough_bytes.size()) {
            m_passthrough_bytes.resize(binding_index + 1);
        }
        auto& slot = m_passthrough_bytes[binding_index];
        slot.resize(byte_size);
        std::memcpy(slot.data(), data, byte_size);
    }

    [[nodiscard]] const GpuShaderConfig& gpu_config() const { return m_gpu_config; }

private:
    GpuShaderConfig m_gpu_config;
    GpuResourceManager m_resources;
    std::vector<GpuBufferBinding> m_bindings;
    std::vector<float> m_staging_floats;
    std::vector<uint8_t> m_push_constants;
    std::vector<size_t> m_output_size_overrides;
    std::vector<std::vector<uint8_t>> m_passthrough_bytes;
    std::vector<std::vector<uint8_t>> m_binding_data;

    //==========================================================================
    // Internal helpers
    //==========================================================================

    /**
     * @brief Flatten planar double channels to a float staging buffer.
     *
     * Skipped entirely for structured modalities (glm::vec3 etc.) since
     * those are handled per-binding in prepare_gpu_inputs via PASSTHROUGH
     * or UINT32/INT32 paths — there is no meaningful double representation
     * to flatten from.
     */
    void flatten_channels_to_staging(
        const std::vector<std::vector<double>>& channels,
        const DataStructureInfo& structure_info)
    {
        m_staging_floats.clear();

        if (Kakshya::is_structured_modality(structure_info.modality)) {
            return;
        }

        const auto& bindings = m_bindings;
        bool all_inputs_staged = !bindings.empty();
        for (size_t i = 0; i < bindings.size(); ++i) {
            const auto dir = bindings[i].direction;
            if (dir == GpuBufferBinding::Direction::OUTPUT)
                continue;
            if (i >= m_binding_data.size() || m_binding_data[i].empty()) {
                all_inputs_staged = false;
                break;
            }
        }

        if (all_inputs_staged)
            return;

        size_t total = 0;
        for (const auto& ch : channels) {
            total += ch.size();
        }

        m_staging_floats.reserve(total);

        for (const auto& ch : channels) {
            for (double v : ch)
                m_staging_floats.push_back(static_cast<float>(v));
        }
    }

    [[nodiscard]] size_t find_first_output_index() const
    {
        size_t first_input_output = SIZE_MAX;
        for (size_t i = 0; i < m_bindings.size(); ++i) {
            if (m_bindings[i].direction == GpuBufferBinding::Direction::OUTPUT) {
                return i;
            }
            if (m_bindings[i].direction == GpuBufferBinding::Direction::INPUT_OUTPUT
                && first_input_output == SIZE_MAX) {
                first_input_output = i;
            }
        }
        if (first_input_output != SIZE_MAX) {
            return first_input_output;
        }
        error<std::runtime_error>(
            Journal::Component::Yantra,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "GpuExecutionContext: no output buffer declared");
    }

    [[nodiscard]] size_t largest_binding_data_element_count() const
    {
        size_t max_bytes = 0;
        for (size_t i = 0; i < m_bindings.size(); ++i) {
            if (m_bindings[i].direction == GpuBufferBinding::Direction::OUTPUT)
                continue;
            if (i < m_binding_data.size() && !m_binding_data[i].empty()) {
                max_bytes = std::max(max_bytes, m_binding_data[i].size());
            }
        }
        const size_t element_bytes = sizeof(float);
        return element_bytes > 0 ? max_bytes / element_bytes : 0;
    }

    static output_type reconstruct_from_floats(
        const std::vector<float>& result_f,
        const std::vector<std::vector<double>>& channels,
        const DataStructureInfo& structure_info)
    {
        size_t offset = 0;
        std::vector<std::vector<double>> result_channels(channels.size());
        for (size_t c = 0; c < channels.size(); ++c) {
            result_channels[c].resize(channels[c].size());
            for (size_t i = 0; i < channels[c].size(); ++i) {
                result_channels[c][i] = static_cast<double>(result_f[offset++]);
            }
        }
        return Datum<OutputType>(
            OperationHelper::reconstruct_from_double<OutputType>(
                result_channels, structure_info));
    }

    output_type dispatch_gpu(const input_type& input)
    {
        auto [channels, structure_info] = OperationHelper::extract_structured_double(
            const_cast<input_type&>(input));

        std::vector<std::vector<double>> channel_copies(channels.size());
        for (size_t c = 0; c < channels.size(); ++c) {
            channel_copies[c].assign(channels[c].begin(), channels[c].end());
        }

        prepare_gpu_inputs(channel_copies, structure_info);

        for (size_t i = 0; i < m_bindings.size(); ++i) {
            m_resources.bind_descriptor(i, m_bindings[i]);
        }

        on_before_gpu_dispatch(channel_copies, structure_info);

        const size_t effective_elements = m_staging_floats.empty()
            ? largest_binding_data_element_count()
            : m_staging_floats.size();
        const auto groups = calculate_dispatch_size(effective_elements, structure_info);

        m_resources.dispatch(
            groups, m_bindings,
            m_push_constants.empty() ? nullptr : m_push_constants.data(),
            m_push_constants.size());

        return collect_gpu_outputs(channel_copies, structure_info);
    }

    output_type dispatch_gpu_chained(const input_type& input, const ExecutionContext& ctx)
    {
        ensure_gpu_ready();

        auto [channels, structure_info] = OperationHelper::extract_structured_double(
            const_cast<input_type&>(input));

        std::vector<std::vector<double>> channel_copies(channels.size());
        for (size_t c = 0; c < channels.size(); ++c)
            channel_copies[c].assign(channels[c].begin(), channels[c].end());

        flatten_channels_to_staging(channel_copies, structure_info);
        prepare_gpu_inputs(channel_copies, structure_info);

        for (size_t i = 0; i < m_bindings.size(); ++i)
            m_resources.bind_descriptor(i, m_bindings[i]);

        const size_t effective = m_staging_floats.empty()
            ? largest_binding_data_element_count()
            : m_staging_floats.size();
        const auto groups = calculate_dispatch_size(effective, structure_info);

        const auto pass_count = std::any_cast<uint32_t>(
            ctx.execution_metadata.at("pass_count"));

        const auto& pc_updater = std::any_cast<std::function<void(uint32_t, void*)>>(ctx.execution_metadata.at("pc_updater"));

        m_resources.dispatch_batched(
            pass_count, groups, m_bindings,
            [&](uint32_t pass, std::vector<uint8_t>& pc_data) {
                pc_updater(pass, pc_data.data());
            },
            m_gpu_config.push_constant_size,
            ctx.execution_metadata);

        return collect_gpu_outputs(channel_copies, structure_info);
    }
};

} // namespace MayaFlux::Yantra
