#pragma once

#include "ShaderSpecBinding.hpp"

#include "MayaFlux/Yantra/OperationSpec/ExecutionContext.hpp"

#include "MayaFlux/Yantra/OperationSpec/OperationHelper.hpp"

namespace MayaFlux::Core {
class VKImage;
}

namespace MayaFlux::Yantra {

class GpuDispatchCore;

/**
 * @struct GpuChannelResult
 * @brief Erased output of a GPU dispatch: reconstructed float data plus
 *        any raw auxiliary outputs keyed by binding index.
 */
struct GpuChannelResult {
    std::vector<float> primary;
    std::unordered_map<size_t, std::vector<uint8_t>> aux;
};

/**
 * @struct IndirectGroupsSource
 * @brief Location of GPU-resident workgroup counts for a DependencyStage.
 *
 * Names a shared buffer of the dispatching context, allocated with
 * BufferUsageHint::INDIRECT, that holds {x, y, z} as three uint32 values at
 * offset_bytes. An earlier stage of the same call writes it, so the dispatch
 * size never reaches the host.
 */
struct IndirectGroupsSource {
    uint32_t set {};
    size_t binding {};
    uint64_t offset_bytes { 0 };
};

/**
 * @struct DependencyStage
 * @brief One stage in a record_stages call.
 *
 * stage_fn is invoked with this context as the active target, exactly as
 * if the caller had just called swap_shader(config) and were about to call
 * dispatch_core — stage_image_at, set_binding_data, stage_passthrough,
 * set_push_constants all apply normally inside stage_fn. Works identically
 * for image-bearing and buffer-only shaders; nothing about this struct is
 * image-specific. Every stage is followed by a barrier, so a stage sees the
 * writes of every earlier stage in the same call.
 */
struct DependencyStage {
    GpuComputeConfig config;
    std::function<void(GpuDispatchCore&)> stage_fn;
    std::optional<std::array<uint32_t, 3>> explicit_groups;

    /**
     * @brief Shared buffer holding this stage's workgroup counts, when they
     *        come from GPU data. explicit_groups is then ignored.
     */
    std::optional<IndirectGroupsSource> indirect_groups;
};

/**
 * @class GpuDispatchCore
 * @brief Non-template base that owns all type-independent GPU dispatch logic.
 *
 * Separates resource management, buffer staging, and dispatch orchestration
 * from the type-parameterised boundary in GpuExecutionContext.  All virtual
 * override points that do not reference InputType/OutputType live here so
 * that implementations can be placed in a .cpp file.
 *
 * Subclasses (including GpuExecutionContext) implement the two remaining
 * type-dependent steps -- channel extraction and output reconstruction --
 * without duplicating anything that is type-independent.
 */
class MAYAFLUX_API GpuDispatchCore {
public:
    explicit GpuDispatchCore(GpuComputeConfig config, bool compute_isolated = true);
    virtual ~GpuDispatchCore() = default;

    GpuDispatchCore(const GpuDispatchCore&) = delete;
    GpuDispatchCore& operator=(const GpuDispatchCore&) = delete;
    GpuDispatchCore(GpuDispatchCore&&) = delete;
    GpuDispatchCore& operator=(GpuDispatchCore&&) = delete;

    /**
     * @brief Set push constant data from a raw byte pointer.
     * @param data  Pointer to trivially-copyable push constant struct.
     * @param bytes Size in bytes.
     */
    void set_push_constants(const void* data, size_t bytes);

    /**
     * @brief Typed convenience wrapper for set_push_constants(const void*, size_t).
     * @tparam T Trivially copyable type matching shader push constant layout.
     */
    template <typename T>
    void set_push_constants(const T& data)
    {
        set_push_constants(&data, sizeof(T));
    }

    /**
     * @brief Pre-stage typed data for a specific binding slot, bypassing
     *        the default channel-flattening path in prepare_gpu_inputs.
     * @tparam T  Trivially copyable element type.
     * @param index Binding index matching declare_buffer_bindings order.
     * @param data  Elements to upload.
     */
    template <typename T>
    void set_binding_data(size_t index, std::span<const T> data)
    {
        if (index >= m_binding_data.size())
            m_binding_data.resize(index + 1);
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
     * @brief Ensure a shared storage buffer of at least element_count elements
     *        exists at (set, binding_index).
     *
     * The default hint allocates host cached memory, suited to buffers the
     * host reads back. DEVICE and COMPUTE request device local, host visible
     * memory instead, so buffers only shaders touch avoid crossing the bus;
     * the request falls back to host cached memory when none is available.
     * Host reads of device local memory are uncached, so keep bulk readback
     * buffers on the default hint.
     */
    void ensure_shared_buffer(uint32_t set, size_t binding_index, size_t element_count,
        GpuBufferBinding::ElementType element_type,
        Portal::Graphics::BufferUsageHint usage_hint = Portal::Graphics::BufferUsageHint::COMPUTE_STORAGE)
    {
        m_resources.ensure_shared_buffer(set, binding_index, element_count, element_type, usage_hint);
    }

    void upload_shared_raw(uint32_t set, size_t binding_index, const uint8_t* data, size_t byte_size)
    {
        m_resources.upload_shared_raw(set, binding_index, data, byte_size);
    }

    void download_shared(uint32_t set, size_t binding_index, void* dest, size_t byte_size)
    {
        m_resources.download_shared(set, binding_index, dest, byte_size);
    }

    /**
     * @brief Fill the whole shared buffer at (set, binding_index) with one
     *        32-bit value, in order with the recorded dispatches.
     */
    void fill_shared(uint32_t set, size_t binding_index, uint32_t value)
    {
        m_resources.fill_shared(set, binding_index, value);
    }

    /**
     * @brief Append this context's work to a recording shared with other
     *        contexts. See GpuResourceManager::set_recording.
     */
    void set_recording(std::shared_ptr<GpuDispatchRecorder> recording)
    {
        m_resources.set_recording(std::move(recording));
    }

    /**
     * @brief Stage and record each stage in order, one dispatch per stage.
     *
     * Each stage is staged and recorded in turn, so its uploads and fills land
     * between the dispatches before and after it. With a recording attached
     * nothing is submitted; without one,
     * each stage is submitted and awaited on its own. The context's own
     * shader config, bindings and staged data are restored before this
     * returns.
     *
     * @param stages Ordered stage descriptions.
     */
    void record_stages(const std::vector<DependencyStage>& stages);

    /**
     * @brief Declare the byte capacity of an output binding independently
     *        of input data.  Required for edge lists, histograms, count
     *        buffers, and any output whose size cannot be derived from input.
     * @param index     Binding index.
     * @param byte_size Required allocation in bytes.
     */
    void set_output_size(size_t index, size_t byte_size);

    /**
     * @brief Ensure GPU resources are initialised.  Safe to call repeatedly.
     * @return True if GPU is ready after this call.
     */
    bool ensure_gpu_ready();

    /**
     * @brief Query GPU readiness without attempting initialisation.
     */
    [[nodiscard]] bool is_gpu_ready() const;

    /**
     * @brief Return the image registered at an IMAGE_STORAGE output binding.
     *
     * Valid after dispatch_core completes (dispatch is synchronous via
     * submit_and_wait).  Callers may then bind it directly to a render pass
     * or read it back via TextureLoom.
     *
     * @param binding_index Index of the IMAGE_STORAGE binding.
     * @return Shared pointer to the VKImage, or nullptr if not registered.
     */
    [[nodiscard]] std::shared_ptr<Core::VKImage> get_output_image(size_t binding_index) const;

    /**
     * @brief Read back a specific binding into a caller-provided destination.
     *
     * @param index     Binding index to read back.
     * @param dest      Pointer to caller-allocated memory for the data.
     * @param byte_size Size in bytes to read back (must not exceed allocated size).
     */
    void download_binding(size_t index, void* dest, size_t byte_size);

    /**
     * @brief Switch which shader subsequent dispatch_core calls target.
     *
     * No longer destroys any GpuResourceManager state: each shader path
     * is a persistent key in GpuResourceManager, created once and reused.
     * Switching back to a previously-used shader is free. Preserves all
     * staged image and buffer bindings, same as before.
     *
     * Push constant data belongs to the shader that was bound when it was
     * set. When the shader changes, the staged push data is dropped so the
     * next dispatch cannot carry bytes from the previous shader into a
     * pipeline whose push range is different (or absent). Setting the push
     * data after the swap is the expected order.
     *
     * Use to drive a multi-op sequence through one context: stage the input
     * image, call swap_shader + dispatch_core per op, pipe get_output_image(0)
     * back in via stage_image between steps.
     *
     * @param config Shader config for the next dispatch.
     */
    void swap_shader(GpuComputeConfig config)
    {
        const bool shader_changed = config.shader_id != m_gpu_config.shader_id
            || config.shader_path != m_gpu_config.shader_path;
        m_gpu_config = std::move(config);
        if (shader_changed)
            m_push_constants.clear();
        update_dispatch_key_cache();
        m_bindings = declared_bindings();
    }

    /**
     * @brief Register a VKImage at an explicit binding index.
     *
     * Dispatches to the storage or sampled path based on kind. The image
     * will be transitioned to eGeneral (STORAGE) or eShaderReadOnlyOptimal
     * (SAMPLED) if not already there.
     *
     * @param binding_index Index matching the declared image binding.
     * @param image         Initialised VKImage.
     * @param kind           IMAGE_STORAGE or IMAGE_SAMPLED.
     * @param sampler        Vulkan sampler handle. Required for IMAGE_SAMPLED,
     *                       ignored for IMAGE_STORAGE.
     */
    void stage_image_at(size_t binding_index,
        std::shared_ptr<Core::VKImage> image,
        GpuBufferBinding::ElementType kind,
        vk::Sampler sampler = nullptr);

    /**
     * @brief Non-owning view of the shared buffer at (set, binding_index).
     *
     * Lets a consumer read a context's GPU output in place instead of through
     * a host readback. The context keeps ownership, so the view is only valid
     * while the context lives and until the buffer is grown.
     */
    [[nodiscard]] Portal::Graphics::GpuBufferHandle shared_buffer_handle(uint32_t set, size_t binding_index) const
    {
        return m_resources.shared_buffer_handle(set, binding_index);
    }

    /** @brief Device address of the shared buffer at (set, binding_index), or zero when absent. */
    [[nodiscard]] uint64_t shared_buffer_address(uint32_t set, size_t binding_index) const
    {
        return m_resources.shared_buffer_address(set, binding_index);
    }

    /**
     * @brief The key used for this context's GpuResourceManager unit.
     *
     * shader_path when non-empty (named .comp files). Falls back to the
     * numeric shader_id when shader_path is empty, which is always the case
     * for assembled ShaderSpec configs (config_from_spec never sets
     * shader_path — only shader_id, via ShaderFoundry's content-hash cache).
     * Without this fallback, every assembled shader collapses onto the same
     * empty-string key and silently overwrites the previous assembled
     * shader's pipeline/bindings.
     */
    [[nodiscard]] const std::string& dispatch_key() const { return m_cached_dispatch_key; }

protected:
    /**
     * @brief Declare the storage buffers the shader expects.
     *
     * Default: INPUT at (0,0) FLOAT32, OUTPUT at (0,1) FLOAT32.
     */
    [[nodiscard]] virtual std::vector<GpuBufferBinding> declare_buffer_bindings() const;

    /**
     * @brief Called immediately before dispatch.  Override to write push
     *        constants or perform any per-dispatch reconfiguration.
     */
    virtual void on_before_gpu_dispatch(
        const std::vector<std::vector<double>>& channels,
        const DataStructureInfo& structure_info);

    /**
     * @brief Marshal channel data into GPU input buffers.
     *
     * Handles FLOAT32, UINT32, INT32, PASSTHROUGH, IMAGE_STORAGE, and
     * IMAGE_SAMPLED binding kinds.  Called after flatten_channels_to_staging.
     */
    virtual void prepare_gpu_inputs(
        const std::vector<std::vector<double>>& channels,
        const DataStructureInfo& structure_info);

    /**
     * @brief Calculate workgroup dispatch counts from structure dimensions.
     *
     * Reads SPATIAL_X/Y/Z roles for 2D/3D shaders; falls back to 1D
     * element-count dispatch when no spatial dimensions exist.
     *
     * @param total_elements Flat element count for the 1D fallback.
     * @param structure_info Dimension metadata.
     */
    [[nodiscard]] virtual std::array<uint32_t, 3> calculate_dispatch_size(
        size_t total_elements,
        const DataStructureInfo& structure_info) const;

    /**
     * @brief Stage raw bytes for a PASSTHROUGH binding before dispatch.
     * @param binding_index Index matching declare_buffer_bindings order.
     * @param data          Raw byte pointer.
     * @param byte_size     Size in bytes.
     */
    void stage_passthrough(size_t binding_index, const void* data, size_t byte_size);

    /**
     * @brief Stage raw input bytes of one element type for the next dispatch.
     *
     * The staging belongs to the next dispatch only: prepare_gpu_inputs
     * consumes it, whether that dispatch succeeds or throws. It is uploaded
     * raw, without conversion, to every INPUT FLOAT32 binding when element is
     * FLOAT32, or every INPUT UINT32 binding when element is UINT32. Such a
     * binding must have no binding data of its own. When native input is
     * staged, it takes the place of channel input, and its element count sets
     * the dispatch size.
     *
     * @param format    Element format of data, a Kakshya::GpuDataFormat.
     * @param data      Raw bytes in that format.
     * @param byte_size Total byte count, a multiple of gpu_data_format_bytes(format).
     */
    void stage_native_bytes(Kakshya::GpuDataFormat format, const void* data, size_t byte_size);

    [[nodiscard]] const GpuComputeConfig& gpu_config() const;

    /**
     * @brief Full single-pass dispatch.  Drives prepare_gpu_inputs,
     *        on_before_gpu_dispatch, bind_all_descriptors, and GpuResourceManager::dispatch.
     *
     * @param channels       Extracted double channels from the input Datum.
     * @param structure_info Dimension/modality metadata from OperationHelper.
     * @return GpuChannelResult containing primary float readback and aux buffers.
     */
    GpuChannelResult dispatch_core(
        const std::vector<std::vector<double>>& channels,
        const DataStructureInfo& structure_info);

    /**
     * @brief Multi-pass (chained) dispatch.  Calls dispatch_batched on
     *        GpuResourceManager and reads back once after all passes.
     *
     * @param channels       Extracted double channels.
     * @param structure_info Dimension/modality metadata.
     * @param ctx            ExecutionContext carrying pass_count and pc_updater.
     * @return GpuChannelResult containing primary float readback and aux
     *         buffers; empty with a recording attached, which reads nothing back.
     */
    GpuChannelResult dispatch_core_chained(
        const std::vector<std::vector<double>>& channels,
        const DataStructureInfo& structure_info,
        const ExecutionContext& ctx);

    /**
     * @brief Multi-pass dispatch where a GPU-resident indirect buffer gates
     *        each pass's workgroup count instead of a fixed pass_count.
     *
     * Calls dispatch_batched_indirect on GpuResourceManager and reads back
     * once after all passes.
     *
     * @param channels       Extracted double channels.
     * @param structure_info Dimension/modality metadata.
     * @param ctx            ExecutionContext carrying pass_count, pc_updater,
     *                        and indirect_dispatch_binding.
     * @return GpuChannelResult containing primary float readback and aux
     *         buffers; empty with a recording attached, which reads nothing back.
     */
    GpuChannelResult dispatch_core_chained_indirect(
        const std::vector<std::vector<double>>& channels,
        const DataStructureInfo& structure_info,
        const ExecutionContext& ctx);

    /**
     * @brief Non-blocking variant of dispatch_core.
     *
     * Performs the full setup (on_before_gpu_dispatch, prepare_gpu_inputs,
     * bind_all_descriptors) then calls GpuResourceManager::dispatch_async.
     * Returns immediately with a FenceID. The caller must poll
     * ShaderFoundry::is_fence_signaled on the returned ID, and once
     * signaled call readback_primary / readback_aux to collect results.
     *
     * @param channels       Extracted double channels from the input Datum.
     * @param structure_info Dimension/modality metadata from OperationHelper.
     * @return FenceID to poll. INVALID_FENCE if dispatch fails, and always
     *         with a recording attached, which holds the dispatch until it
     *         is submitted.
     */
    [[nodiscard]] Portal::Graphics::FenceID dispatch_core_async(
        const std::vector<std::vector<double>>& channels,
        const DataStructureInfo& structure_info);

    /**
     * @brief Effective element count used by the last dispatch_core or
     *        dispatch_core_async call.
     *
     * Cached after each dispatch so callers can pass the correct count to
     * readback_primary without re-deriving it.
     */
    [[nodiscard]] size_t last_effective_element_count() const
    {
        return m_last_effective_element_count;
    }

    /**
     * @brief Read back the primary output buffer into a float vector.
     *
     * Selects the first OUTPUT or INPUT_OUTPUT binding.  Caps readback to
     * the lesser of the requested element count, at the binding's element
     * width, and the allocated buffer size.  The bytes are returned as is,
     * so a wider element fills several floats.
     *
     * @param float_count Number of elements of the primary binding to attempt to read.
     * @return Float vector of length min(float_count * element width, allocated) / sizeof(float).
     */
    [[nodiscard]] std::vector<float> readback_primary(size_t float_count);

    /**
     * @brief Read back all OUTPUT bindings that have explicit size overrides
     *        into the aux map of a GpuChannelResult.
     *
     * @param result GpuChannelResult to write aux entries into.
     */
    void readback_aux(GpuChannelResult& result);

    /**
     * @brief Flatten planar double channels into m_staging_floats.
     *
     * Skipped for structured modalities (glm::vec3 etc.) since those are
     * handled per-binding via PASSTHROUGH or integer paths.
     */
    void flatten_channels_to_staging(
        const std::vector<std::vector<double>>& channels,
        const DataStructureInfo& structure_info);

    /**
     * @brief Flatten native-typed DataVariant channels into m_native_staging_bytes
     *        without any conversion.
     *
     * Called by GpuExecutionContext::extract_inputs for float and uint32 input.
     * Copies each variant's bytes unchanged and records their element type. All
     * variants must share one element type, or the call throws.
     *
     * No-ops when variants is empty or the modality is structured.
     *
     * @param variants      Per-channel DataVariants from the container.
     * @param structure_info Dimension/modality metadata.
     */
    void flatten_native_variants_to_staging(
        const std::vector<Kakshya::DataVariant>& variants,
        const DataStructureInfo& structure_info);

    [[nodiscard]] size_t find_first_output_index() const;
    [[nodiscard]] size_t largest_binding_data_element_count() const;

    GpuResourceManager m_resources;
    std::vector<GpuBufferBinding> m_bindings;
    std::vector<float> m_staging_floats;
    std::vector<uint8_t> m_push_constants;
    std::vector<size_t> m_output_size_overrides;
    std::vector<std::vector<uint8_t>> m_passthrough_bytes;
    std::vector<std::vector<uint8_t>> m_binding_data;

    struct ImageBinding {
        std::shared_ptr<Core::VKImage> image;
        vk::Sampler sampler;
        GpuBufferBinding::ElementType kind {};
    };
    std::vector<ImageBinding> m_image_bindings;

private:
    /**
     * @brief True when a binding the shader statically uses has no staged image.
     *
     * Declared-but-unused image bindings are ignored.
     */
    [[nodiscard]] bool has_unstaged_active_image();

    /**
     * @brief True when native input is staged and no data-less INPUT FLOAT32
     *        or UINT32 binding accepts its element type.
     */
    [[nodiscard]] bool native_input_unreceivable();

    /**
     * @brief Gate for the single-dispatch paths, called before prepare_gpu_inputs.
     *
     * False when the dispatch cannot be fed: native input was rejected while
     * staging, native input has no receiving binding, or a used image binding
     * has no staged image. On false the staged native input is discarded, so
     * it cannot reach a later dispatch, and nothing is bound or recorded. Logs
     * once per episode and re-arms after a dispatch that proceeds. Never throws.
     */
    [[nodiscard]] bool inputs_accepted();

    bool m_native_rejected {};
    bool m_input_warned {};

    /**
     * @brief Stage each of stages in turn and hand its dispatch key, groups,
     *        push constants and indirect source to on_stage, then restore the
     *        context's own config, bindings and staged data.
     */
    void for_each_stage(const std::vector<DependencyStage>& stages,
        const std::function<void(const std::array<uint32_t, 3>&, const Portal::Graphics::IndirectDispatch&)>& on_stage);

    GpuComputeConfig m_gpu_config;
    std::string m_cached_dispatch_key;

    size_t m_last_effective_element_count {};

    /// Native input for the next dispatch, set by stage_native_bytes() or
    /// flatten_native_variants_to_staging(). Consumed by prepare_gpu_inputs.
    std::vector<uint8_t> m_native_staging_bytes;
    std::optional<Kakshya::GpuDataFormat> m_native_format;

    /// Element count of the dispatch most recently prepared. Set by
    /// prepare_gpu_inputs; the dispatch size and readback count come from it.
    size_t m_prepared_elements {};


    /** declare_buffer_bindings less the bindings the current unit's layout does not hold. */
    [[nodiscard]] std::vector<GpuBufferBinding> declared_bindings() const;

    void update_dispatch_key_cache();
    void bind_all_descriptors();
};

} // namespace MayaFlux::Yantra
