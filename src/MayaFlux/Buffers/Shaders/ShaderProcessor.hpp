#pragma once

#include "MayaFlux/Buffers/VKBuffer.hpp"
#include "MayaFlux/Portal/Graphics/ComputePress.hpp"

namespace MayaFlux::Buffers {

/**
 * @struct ShaderBinding
 * @brief Describes how a VKBuffer binds to a shader descriptor
 */
struct ShaderBinding {
    uint32_t set = 0; ///< Descriptor set index
    uint32_t binding = 0; ///< Binding point within set
    vk::DescriptorType type = vk::DescriptorType::eStorageBuffer;

    ShaderBinding() = default;
    ShaderBinding(uint32_t s, uint32_t b, vk::DescriptorType t = vk::DescriptorType::eStorageBuffer)
        : set(s)
        , binding(b)
        , type(t)
    {
    }
};

/**
 * @struct ShaderProcessorConfig
 * @brief Complete configuration for shader processor
 */
struct ShaderConfig {
    std::string shader_path; ///< Path to shader file
    Portal::Graphics::ShaderStage stage = Portal::Graphics::ShaderStage::COMPUTE;
    std::string entry_point = "main";

    std::unordered_map<std::string, ShaderBinding> bindings;

    size_t push_constant_size = 0;

    std::unordered_map<uint32_t, uint32_t> specialization_constants;

    ShaderConfig() = default;
    ShaderConfig(std::string path)
        : shader_path(std::move(path))
    {
    }
};

/**
 * @class ShaderProcessor
 * @brief Abstract base class for shader-based buffer processing
 *
 * ShaderProcessor provides the foundational infrastructure for managing shader resources,
 * descriptor sets, and buffer bindings. It is designed to be stage-agnostic, serving as
 * the common parent for specialized processors like ComputeProcessor and RenderProcessor.
 *
 * Core Responsibilities:
 * - **Shader Management:** Loads and manages shader modules via Portal::Graphics::ShaderFoundry.
 * - **Descriptor Management:** Handles descriptor set allocation, updates, and binding.
 * - **Buffer Binding:** Maps logical names (e.g., "input", "output") to physical VKBuffers.
 * - **Constants:** Manages push constants and specialization constants.
 * - **Hot-Reload:** Supports runtime shader reloading and pipeline invalidation.
 *
 * It does NOT define specific pipeline creation or execution logic (e.g., dispatch vs draw),
 * leaving those details to derived classes (ComputeProcessor, RenderProcessor).
 *
 * Quality-of-life features:
 * - **Data movement hints:** Query buffer usage (input/output/in-place) for automation.
 * - **Binding introspection:** Validate if required bindings are satisfied.
 * - **State queries:** Track processing state for chain management.
 *
 * Design Philosophy:
 * - **Inheritance-focused**: Provides the "plumbing" for shader processors without dictating the pipeline type.
 * - **Buffer-agnostic**: Works with any VKBuffer modality/usage.
 * - **Flexible binding**: Decouples logical shader parameters from physical buffers.
 *
 * Integration:
 * - Base class for `ComputeProcessor` (Compute Pipelines)
 * - Base class for `RenderProcessor` (Graphics Pipelines)
 * - Base class for `NodeBindingsProcessor` (Node-driven parameters)
 *
 * Usage (via derived classes):
 *   // Compute example
 *   auto compute = std::make_shared<ComputeProcessor>("shaders/kernel.comp");
 *   compute->bind_buffer("data", buffer);
 *
 *   // Graphics example
 *   auto render = std::make_shared<RenderProcessor>(config);
 *   render->bind_buffer("vertices", vertex_buffer);
 */
class MAYAFLUX_API ShaderProcessor : public VKBufferProcessor {
public:
    /**
     * @brief Get buffer usage characteristics needed for safe data flow
     *
     * Returns flags indicating:
     * - Does compute read from input? (HOST_TO_DEVICE upload needed?)
     * - Does compute write to output? (DEVICE_TO_HOST readback needed?)
     *
     * This lets ComputeProcessingChain auto-determine staging needs.
     */
    enum class BufferUsageHint : uint8_t {
        NONE = 0,
        INPUT_READ = 1 << 0, ///< Shader reads input
        OUTPUT_WRITE = 1 << 1, ///< Shader writes output (modifies)
        BIDIRECTIONAL = INPUT_READ | OUTPUT_WRITE
    };

    /**
     * @brief Construct processor with shader path
     * @param shader_path Path to shader file (e.g., .comp, .vert, .frag, .spv)
     */
    explicit ShaderProcessor(const std::string& shader_path);

    /**
     * @brief Construct processor with full configuration
     * @param config Complete shader processor configuration
     */
    explicit ShaderProcessor(ShaderConfig config);

    ~ShaderProcessor() override;

    //==========================================================================
    // BufferProcessor Interface
    //==========================================================================

    void processing_function(std::shared_ptr<Buffer> buffer) override;
    void on_attach(std::shared_ptr<Buffer> buffer) override;
    void on_detach(std::shared_ptr<Buffer> buffer) override;

    [[nodiscard]] bool is_compatible_with(std::shared_ptr<Buffer> buffer) const override;

    //==========================================================================
    // Buffer Binding - Multi-buffer Support
    //==========================================================================

    /**
     * @brief Bind a VKBuffer to a named shader descriptor
     * @param descriptor_name Logical name (e.g., "input", "output")
     * @param buffer VKBuffer to bind
     *
     * Registers the buffer for descriptor set binding.
     * The descriptor_name must match a key in config.bindings.
     */
    void bind_buffer(const std::string& descriptor_name, const std::shared_ptr<VKBuffer>& buffer);

    /**
     * @brief Unbind a buffer from a descriptor
     * @param descriptor_name Logical name to unbind
     */
    void unbind_buffer(const std::string& descriptor_name);

    /**
     * @brief Get bound buffer for a descriptor name
     * @param descriptor_name Logical name
     * @return Bound buffer, or nullptr if not bound
     */
    [[nodiscard]] std::shared_ptr<VKBuffer> get_bound_buffer(const std::string& descriptor_name) const;

    /**
     * @brief Auto-bind buffer based on attachment order
     * @param buffer Buffer to auto-bind
     *
     * First attachment -> "input" or first binding
     * Second attachment -> "output" or second binding
     * Useful for simple single-buffer or input/output patterns.
     */
    void auto_bind_buffer(const std::shared_ptr<VKBuffer>& buffer);

    //==========================================================================
    // Shader Management
    //==========================================================================

    /**
     * @brief Hot-reload shader from ShaderFoundry
     * @return True if reload succeeded
     *
     * Invalidates cached shader and rebuilds pipeline.
     * Existing descriptor sets are preserved if compatible.
     */
    bool hot_reload_shader();

    /**
     * @brief Update shader path and reload
     * @param shader_path New shader path
     */
    void set_shader(const std::string& shader_path);

    /**
     * @brief Get current shader path
     */
    [[nodiscard]] const std::string& get_shader_path() const { return m_config.shader_path; }

    //==========================================================================
    // Push Constants
    //==========================================================================

    /**
     * @brief Set push constant size
     * @param size Size in bytes
     */
    void set_push_constant_size(size_t size);

    /**
     * @brief Set push constant size from type
     * @tparam T Push constant struct type
     */
    template <typename T>
    void set_push_constant_size()
    {
        set_push_constant_size(sizeof(T));
    }

    /**
     * @brief Update push constant data (type-safe)
     * @tparam T Push constant struct type
     * @param data Push constant data
     *
     * Data is copied and uploaded during next process() call.
     */
    template <typename T>
    void set_push_constant_data(const T& data);

    /**
     * @brief Update push constant data (raw bytes)
     * @param data Pointer to data
     * @param size Size in bytes
     */
    void set_push_constant_data_raw(const void* data, size_t size);

    /**
     * @brief Get current push constant data
     */
    [[nodiscard]] const std::vector<uint8_t>& get_push_constant_data() const { return m_push_constant_data; }
    [[nodiscard]] std::vector<uint8_t>& get_push_constant_data() { return m_push_constant_data; }

    //==========================================================================
    // Specialization Constants
    //==========================================================================

    /**
     * @brief Set specialization constant
     * @param constant_id Specialization constant ID
     * @param value Value to set
     *
     * Requires pipeline recreation to take effect.
     */
    void set_specialization_constant(uint32_t constant_id, uint32_t value);

    /**
     * @brief Clear all specialization constants
     */
    void clear_specialization_constants();

    //==========================================================================
    // Configuration
    //==========================================================================

    /**
     * @brief Update entire configuration
     * @param config New configuration
     *
     * Triggers pipeline recreation.
     */
    void set_config(const ShaderConfig& config);

    /**
     * @brief Get current configuration
     */
    [[nodiscard]] const ShaderConfig& get_config() const { return m_config; }

    /**
     * @brief Add descriptor binding configuration
     * @param descriptor_name Logical name
     * @param binding Shader binding info
     */
    void add_binding(const std::string& descriptor_name, const ShaderBinding& binding);

    //==========================================================================
    // Data movement hints
    //==========================================================================

    /**
     * @brief Get buffer usage hint for a descriptor
     * @param descriptor_name Binding name
     * @return BufferUsageHint flags
     */
    [[nodiscard]] virtual BufferUsageHint get_buffer_usage_hint(const std::string& descriptor_name) const;

    /**
     * @brief Check if shader modifies a specific buffer in-place
     * @param descriptor_name Binding name
     * @return True if shader both reads and writes this buffer
     */
    [[nodiscard]] virtual bool is_in_place_operation(const std::string& descriptor_name) const;

    /**
     * @brief Check if a descriptor binding exists
     * @param descriptor_name Name of the binding (e.g., "input", "output")
     * @return True if binding is configured
     */
    [[nodiscard]] bool has_binding(const std::string& descriptor_name) const;

    /**
     * @brief Get all configured descriptor names
     * @return Vector of binding names
     *
     * Useful for introspection: which buffers does this shader expect?
     */
    [[nodiscard]] std::vector<std::string> get_binding_names() const;

    /**
     * @brief Check if all required bindings are satisfied
     * @return True if all configured bindings have buffers bound
     */
    [[nodiscard]] bool are_bindings_complete() const;

    //==========================================================================
    // State Queries
    //==========================================================================

    /**
     * @brief Check if shader is loaded
     */
    [[nodiscard]] bool is_shader_loaded() const { return m_shader_id != Portal::Graphics::INVALID_SHADER; }

    /**
     * @brief Check if descriptors are initialized
     */
    [[nodiscard]] bool are_descriptors_ready() const { return !m_descriptor_set_ids.empty(); }

    /**
     * @brief Get number of bound buffers
     */
    [[nodiscard]] size_t get_bound_buffer_count() const { return m_bound_buffers.size(); }

    /**
     * @brief Get the output buffer after compute dispatch
     *
     * Returns the buffer that was last processed (input/output depends on
     * shader and binding configuration). Used by ComputeProcessingChain
     * to determine where compute results ended up.
     *
     * Typically the buffer passed to processing_function(), but can be
     * overridden by subclasses if compute modifies different buffers.
     */
    [[nodiscard]] virtual std::shared_ptr<VKBuffer> get_output_buffer() const { return m_last_processed_buffer; }

    /**
     * @brief Check if compute has been executed at least once
     * @return True if processing_function() has been called
     */
    [[nodiscard]] virtual inline bool has_executed() const
    {
        return m_last_command_buffer != Portal::Graphics::INVALID_COMMAND_BUFFER;
    }

protected:
    //==========================================================================
    // Overridable Hooks for Specialized Processors
    //==========================================================================

    /**
     * @brief Called before shader compilation
     * @param shader_path Path to shader
     *
     * Override to modify shader compilation (e.g., add defines, includes).
     */
    virtual void on_before_compile(const std::string& shader_path);

    /**
     * @brief Called after shader is loaded
     * @param shader Loaded shader module
     *
     * Override to extract reflection data or validate shader.
     */
    virtual void on_shader_loaded(Portal::Graphics::ShaderID shader_id);

    /**
     * @brief Called before pipeline creation
     * @param config Pipeline configuration
     *
     * Override to modify pipeline configuration.
     */
    virtual void on_before_pipeline_create(Portal::Graphics::ComputePipelineID pipeline_id) { }

    /**
     * @brief Called after pipeline is created
     * @param pipeline Created pipeline
     *
     * Override for post-pipeline setup.
     */
    virtual void on_pipeline_created(Portal::Graphics::ComputePipelineID pipeline_id);

    /**
     * @brief Called before descriptor sets are created
     *
     * Override to add custom descriptor bindings.
     */
    virtual void on_before_descriptors_create();

    /**
     * @brief Called after descriptor sets are created
     *
     * Override for custom descriptor updates.
     */
    virtual void on_descriptors_created();

    /**
     * @brief Called before each process callback
     * @param cmd Command buffer
     * @param buffer Currently processing buffer
     *
     * Override to update push constants or dynamic descriptors.
     */
    virtual void on_before_execute(Portal::Graphics::CommandBufferID cmd_id, const std::shared_ptr<VKBuffer>& buffer);

    /**
     * @brief Called after each process callback
     * @param cmd Command buffer
     * @param buffer Currently processed buffer
     *
     * Override for post-dispatch synchronization or state updates.
     */
    virtual void on_after_execute(Portal::Graphics::CommandBufferID cmd_id, const std::shared_ptr<VKBuffer>& buffer);

    //==========================================================================
    // Protected State - Available to Subclasses
    //==========================================================================

    ShaderConfig m_config;

    Portal::Graphics::ShaderID m_shader_id = Portal::Graphics::INVALID_SHADER;
    std::vector<Portal::Graphics::DescriptorSetID> m_descriptor_set_ids;
    Portal::Graphics::CommandBufferID m_last_command_buffer = Portal::Graphics::INVALID_COMMAND_BUFFER;

    std::unordered_map<std::string, std::shared_ptr<VKBuffer>> m_bound_buffers;
    std::shared_ptr<VKBuffer> m_last_processed_buffer;

    std::vector<uint8_t> m_push_constant_data;

    bool m_initialized {};
    bool m_needs_pipeline_rebuild = true;
    bool m_needs_descriptor_rebuild = true;

    size_t m_auto_bind_index {};

protected:
    virtual void initialize_pipeline(const std::shared_ptr<VKBuffer>& buffer) = 0;
    virtual void initialize_descriptors(const std::shared_ptr<VKBuffer>& buffer) = 0;
    virtual void execute_shader(const std::shared_ptr<VKBuffer>& buffer) = 0;

    virtual void update_descriptors(const std::shared_ptr<VKBuffer>& buffer);
    virtual void cleanup();

private:
    //==========================================================================
    // Internal Implementation
    //==========================================================================

    void initialize_shader();
};

template <typename T>
void ShaderProcessor::set_push_constant_data(const T& data)
{
    static_assert(sizeof(T) <= 128, "Push constants typically limited to 128 bytes");
    m_push_constant_data.resize(sizeof(T));
    std::memcpy(m_push_constant_data.data(), &data, sizeof(T));
}
} // namespace MayaFlux::Buffers
