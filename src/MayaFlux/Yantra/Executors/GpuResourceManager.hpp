#pragma once

#include "MayaFlux/Portal/Graphics/ComputePress.hpp"

namespace MayaFlux::Yantra {

/**
 * @struct GpuShaderConfig
 * @brief Plain-data description of the compute shader to dispatch.
 */
struct GpuShaderConfig {
    std::string shader_path;
    std::array<uint32_t, 3> workgroup_size { 256, 1, 1 };
    size_t push_constant_size { 0 };
};

/**
 * @struct GpuBufferBinding
 * @brief Declares a single storage buffer the shader expects.
 */
struct GpuBufferBinding {
    uint32_t set { 0 };
    uint32_t binding { 0 };

    enum class Direction : uint8_t {
        INPUT,
        OUTPUT,
        INPUT_OUTPUT
    } direction { Direction::INPUT };

    /**
     * @brief Element type the shader expects in this buffer.
     *
     * FLOAT32      — cast double channels to float (default).
     * UINT32       — reinterpret variant bytes as uint32_t.
     * INT32        — reinterpret variant bytes as int32_t.
     * PASSTHROUGH  — upload raw variant bytes with no cast; caller
     *                must pre-stage via stage_passthrough() for
     *                INPUT / INPUT_OUTPUT bindings.
     */
    enum class ElementType : uint8_t {
        FLOAT32,
        UINT32,
        INT32,
        PASSTHROUGH
    } element_type { ElementType::FLOAT32 };
};

struct GpuResourceManagerImpl;

/**
 * @class GpuResourceManager
 * @brief Encapsulates all Vulkan resource lifecycle behind Portal facades.
 *
 * Manages storage buffers, pipeline, shader, and descriptor sets.
 * PIMPL hides vk:: types from the header entirely.
 */
class GpuResourceManager {
public:
    GpuResourceManager();
    ~GpuResourceManager();

    GpuResourceManager(const GpuResourceManager&) = delete;
    GpuResourceManager& operator=(const GpuResourceManager&) = delete;
    GpuResourceManager(GpuResourceManager&&) = delete;
    GpuResourceManager& operator=(GpuResourceManager&&) = delete;

    bool initialise(const GpuShaderConfig& config,
        const std::vector<GpuBufferBinding>& bindings);

    void ensure_buffer(size_t index, size_t required_bytes);
    void upload(size_t index, const float* data, size_t byte_size);
    void upload_raw(size_t index, const uint8_t* data, size_t byte_size);
    void download(size_t index, float* dest, size_t byte_size);
    void bind_descriptor(size_t index, const GpuBufferBinding& spec);

    void dispatch(const std::array<uint32_t, 3>& groups,
        const std::vector<GpuBufferBinding>& bindings,
        const uint8_t* push_constant_data,
        size_t push_constant_size);

    void dispatch_batched(
        uint32_t pass_count,
        const std::array<uint32_t, 3>& groups,
        const std::vector<GpuBufferBinding>& bindings,
        const std::function<void(uint32_t pass, std::vector<uint8_t>&)>& push_constant_updater,
        size_t push_constant_size,
        const std::unordered_map<std::string, std::any>& execution_metadata = {});

    void cleanup();

    [[nodiscard]] bool is_ready() const { return m_ready; }
    [[nodiscard]] size_t buffer_allocated_bytes(size_t index) const;

private:
    Portal::Graphics::ShaderID m_shader_id { Portal::Graphics::INVALID_SHADER };
    Portal::Graphics::ComputePipelineID m_pipeline_id { Portal::Graphics::INVALID_COMPUTE_PIPELINE };
    std::vector<Portal::Graphics::DescriptorSetID> m_descriptor_set_ids;

    std::unique_ptr<GpuResourceManagerImpl> m_impl;

    struct BufferSlot {
        size_t allocated_bytes {};
    };
    std::vector<BufferSlot> m_slots;

    bool m_ready {};
}; // class GpuResourceManager

} // namespace MayaFlux::Yantra
