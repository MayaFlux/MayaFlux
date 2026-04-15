#pragma once

#include "GpuResourceManager.hpp"
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
    explicit GpuDispatchCore(GpuShaderConfig config);
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
     * @brief Register a VKImage for an IMAGE_STORAGE binding.
     *
     * The image will be transitioned to eGeneral layout if not already there.
     *
     * @param binding_index Index matching the IMAGE_STORAGE declaration.
     * @param image         Initialised VKImage.
     */
    void stage_image_storage(size_t binding_index, std::shared_ptr<Core::VKImage> image);

    /**
     * @brief Register a VKImage + sampler for an IMAGE_SAMPLED binding.
     *
     * The image will be transitioned to eShaderReadOnlyOptimal if needed.
     *
     * @param binding_index Index matching the IMAGE_SAMPLED declaration.
     * @param image         Initialised VKImage.
     * @param sampler       Vulkan sampler handle.
     */
    void stage_image_sampled(size_t binding_index,
        std::shared_ptr<Core::VKImage> image,
        vk::Sampler sampler);

    [[nodiscard]] const GpuShaderConfig& gpu_config() const;

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
    GpuShaderConfig m_gpu_config;
};

} // namespace MayaFlux::Yantra
