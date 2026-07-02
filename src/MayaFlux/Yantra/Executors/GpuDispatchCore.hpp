#pragma once

#include "ShaderSpecBinding.hpp"

#include "MayaFlux/Yantra/OperationSpec/ExecutionContext.hpp"

#include "MayaFlux/Yantra/OperationSpec/OperationHelper.hpp"

namespace MayaFlux::Core {
class VKImage;
}

namespace MayaFlux::Yantra {

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
    explicit GpuDispatchCore(GpuComputeConfig config);
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
     * @brief Replace the active shader and invalidate the compiled pipeline.
     *
     * Tears down the current GpuResourceManager state (pipeline, descriptor sets,
     * shader handle) while preserving all staged image and buffer bindings. The
     * next dispatch_core call will re-initialise with the new shader config.
     *
     * Use to drive a multi-op sequence through one context: stage the input image,
     * call swap_shader + dispatch_core per op, pipe get_output_image(0) back in
     * via stage_image between steps.
     *
     * @param config Shader config for the next dispatch.
     */
    void swap_shader(GpuComputeConfig config)
    {
        m_resources.cleanup();
        m_gpu_config = std::move(config);
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
     * @brief Stage a flat native-typed byte buffer for FLOAT32 bindings,
     *        bypassing the double-to-float cast in flatten_channels_to_staging.
     *
     * Intended for pixel data (uint8_t, uint16_t) and pre-converted float
     * buffers that should reach the GPU without an intermediate double widening.
     * Once staged, prepare_gpu_inputs will upload these bytes directly via
     * upload_raw rather than the float staging path.
     *
     * Calling this clears any previously staged native bytes for the slot.
     * It does not affect m_binding_data (PASSTHROUGH) or m_staging_floats.
     *
     * @param binding_index Binding slot. Must match a FLOAT32 INPUT binding.
     * @param data          Raw bytes in the element type the shader expects.
     * @param byte_size     Total byte count.
     */
    void stage_native_bytes(size_t binding_index, const void* data, size_t byte_size);

    [[nodiscard]] const GpuComputeConfig& gpu_config() const;

    /**
     * @brief Full single-pass dispatch.  Drives prepare_gpu_inputs,
     *        on_before_gpu_dispatch, bind_descriptor, and GpuResourceManager::dispatch.
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
     * @return GpuChannelResult containing primary float readback and aux buffers.
     */
    GpuChannelResult dispatch_core_chained(
        const std::vector<std::vector<double>>& channels,
        const DataStructureInfo& structure_info,
        const ExecutionContext& ctx);

    /**
     * @brief Non-blocking variant of dispatch_core.
     *
     * Performs the full setup (on_before_gpu_dispatch, prepare_gpu_inputs,
     * bind_descriptor) then calls GpuResourceManager::dispatch_async.
     * Returns immediately with a FenceID. The caller must poll
     * ShaderFoundry::is_fence_signaled on the returned ID, and once
     * signaled call readback_primary / readback_aux to collect results.
     *
     * @param channels       Extracted double channels from the input Datum.
     * @param structure_info Dimension/modality metadata from OperationHelper.
     * @return FenceID to poll. INVALID_FENCE if dispatch fails.
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
     * the lesser of the requested float count and the allocated buffer size.
     *
     * @param float_count Number of float elements to attempt to read.
     * @return Float vector of length min(float_count, allocated / sizeof(float)).
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
     * Called by prepare_gpu_inputs when structure_info.original_type indicates
     * a non-double native type and no explicit stage_native_bytes call has been
     * made. Visits the variant's active alternative and memcpys bytes directly.
     *
     * No-ops when channels is empty or the modality is structured.
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
    GpuComputeConfig m_gpu_config;

    size_t m_last_effective_element_count {};

    /// Native-typed staging buffer. Non-empty when stage_native_bytes() has
    /// been called or flatten_native_variants_to_staging() produced output.
    /// Takes precedence over m_staging_floats in prepare_gpu_inputs.
    std::vector<uint8_t> m_native_staging_bytes;
};

} // namespace MayaFlux::Yantra
