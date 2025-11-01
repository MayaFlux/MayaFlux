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
 * @struct ShaderDispatchConfig
 * @brief Configuration for compute shader dispatch
 */
struct ShaderDispatchConfig {
    uint32_t workgroup_x = 256; ///< Workgroup size X (should match shader)
    uint32_t workgroup_y = 1;
    uint32_t workgroup_z = 1;

    enum class DispatchMode : uint8_t {
        ELEMENT_COUNT, ///< Calculate from buffer element count
        MANUAL, ///< Use explicit group counts
        BUFFER_SIZE, ///< Calculate from buffer byte size
        CUSTOM ///< User-provided calculation function
    } mode
        = DispatchMode::ELEMENT_COUNT;

    // Manual dispatch (MANUAL mode)
    uint32_t group_count_x = 1;
    uint32_t group_count_y = 1;
    uint32_t group_count_z = 1;

    std::function<std::array<uint32_t, 3>(const std::shared_ptr<VKBuffer>&)> custom_calculator;

    ShaderDispatchConfig() = default;
};

/**
 * @struct ShaderProcessorConfig
 * @brief Complete configuration for shader processor
 */
struct ShaderProcessorConfig {
    std::string shader_path; ///< Path to shader file
    Portal::Graphics::ShaderStage stage = Portal::Graphics::ShaderStage::COMPUTE;
    std::string entry_point = "main";

    ShaderDispatchConfig dispatch;

    std::unordered_map<std::string, ShaderBinding> bindings;

    size_t push_constant_size = 0;

    std::unordered_map<uint32_t, uint32_t> specialization_constants;

    ShaderProcessorConfig() = default;
    ShaderProcessorConfig(std::string path)
        : shader_path(std::move(path))
    {
    }
};

/**
 * @class ShaderProcessor
 * @brief Generic compute shader processor for VKBuffers
 *
 * ShaderProcessor is a fully functional base class that:
 * - Loads compute shaders via Portal::Graphics::ShaderFoundry
 * - Automatically creates compute pipelines and descriptor sets
 * - Binds VKBuffers to shader descriptors with configurable mappings
 * - Dispatches compute shaders with flexible workgroup calculation
 * - Supports hot-reload via ShaderFoundry caching
 * - Handles push constants and specialization constants
 *
 * Quality-of-life features:
 * - **Data movement hints:** Query buffer usage (input/output/in-place) for automation and validation.
 * - **Binding introspection:** Check if bindings exist, list expected bindings, and validate binding completeness.
 * - **State queries:** Track last processed buffer and command buffer for chain management and debugging.
 *
 * Design Philosophy:
 * - **Fully usable as-is**: Not just a base class, but a complete processor
 * - **Inheritance-friendly**: Specialized processors can override behavior
 * - **Buffer-agnostic**: Works with any VKBuffer modality/usage
 * - **Flexible binding**: Map buffers to shader descriptors by name
 * - **GPU-efficient**: Uses device-local buffers and staging where needed
 *
 * Integration:
 * - Uses Portal::Graphics::ShaderFoundry for shader compilation
 * - Leverages VKComputePipeline for execution
 * - Works with existing BufferManager/ProcessingChain architecture
 * - Compatible with all VKBuffer usage types (COMPUTE, STORAGE, etc.)
 *
 * Usage:
 *   // Simple usage - single buffer processor
 *   auto processor = std::make_shared<ShaderProcessor>("shaders/kernel.comp");
 *   processor->bind_buffer("input_buffer", my_buffer);
 *   my_buffer->set_default_processor(processor);
 *
 *   // Advanced - multi-buffer with explicit bindings
 *   ShaderProcessorConfig config("shaders/complex.comp");
 *   config.bindings["input"] = ShaderBinding(0, 0);
 *   config.bindings["output"] = ShaderBinding(0, 1);
 *   config.dispatch.workgroup_x = 512;
 *
 *   auto processor = std::make_shared<ShaderProcessor>(config);
 *   processor->bind_buffer("input", input_buffer);
 *   processor->bind_buffer("output", output_buffer);
 *
 *   chain->add_processor(processor, input_buffer);
 *   chain->add_processor(processor, output_buffer);
 *
 *   // With push constants
 *   struct Params { float scale; uint32_t iterations; };
 *   processor->set_push_constant_size<Params>();
 *   processor->set_push_constant_data(Params{2.0f, 100});
 *
 * Specialized Processors:
 *   class FFTProcessor : public ShaderProcessor {
 *       FFTProcessor() : ShaderProcessor("shaders/fft.comp") {
 *           configure_fft_bindings();
 *       }
 *
 *       void on_attach(std::shared_ptr<Buffer> buffer) override {
 *           ShaderProcessor::on_attach(buffer);
 *           // FFT-specific setup
 *       }
 *   };
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
     * @param shader_path Path to compute shader (.comp or .spv)
     * @param workgroup_x Workgroup size X (default 256)
     */
    explicit ShaderProcessor(const std::string& shader_path, uint32_t workgroup_x = 256);

    /**
     * @brief Construct processor with full configuration
     * @param config Complete shader processor configuration
     */
    explicit ShaderProcessor(ShaderProcessorConfig config);

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
    // Dispatch Configuration
    //==========================================================================

    /**
     * @brief Set workgroup size (should match shader local_size)
     * @param x Workgroup size X
     * @param y Workgroup size Y (default 1)
     * @param z Workgroup size Z (default 1)
     */
    void set_workgroup_size(uint32_t x, uint32_t y = 1, uint32_t z = 1);

    /**
     * @brief Set dispatch mode
     * @param mode Dispatch calculation mode
     */
    void set_dispatch_mode(ShaderDispatchConfig::DispatchMode mode);

    /**
     * @brief Set manual dispatch group counts
     * @param x Group count X
     * @param y Group count Y (default 1)
     * @param z Group count Z (default 1)
     */
    void set_manual_dispatch(uint32_t x, uint32_t y = 1, uint32_t z = 1);

    /**
     * @brief Set custom dispatch calculator
     * @param calculator Function that calculates dispatch from buffer
     */
    void set_custom_dispatch(std::function<std::array<uint32_t, 3>(const std::shared_ptr<VKBuffer>&)> calculator);

    /**
     * @brief Get current dispatch configuration
     */
    [[nodiscard]] const ShaderDispatchConfig& get_dispatch_config() const { return m_config.dispatch; }

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
    void set_config(const ShaderProcessorConfig& config);

    /**
     * @brief Get current configuration
     */
    [[nodiscard]] const ShaderProcessorConfig& get_config() const { return m_config; }

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
     * @brief Check if pipeline is created
     */
    [[nodiscard]] bool is_pipeline_ready() const { return m_pipeline_id != Portal::Graphics::INVALID_COMPUTE_PIPELINE; }

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
     * @brief Called before each dispatch
     * @param cmd Command buffer
     * @param buffer Currently processing buffer
     *
     * Override to update push constants or dynamic descriptors.
     */
    virtual void on_before_dispatch(Portal::Graphics::CommandBufferID cmd_id, const std::shared_ptr<VKBuffer>& buffer);

    /**
     * @brief Called after each dispatch
     * @param cmd Command buffer
     * @param buffer Currently processed buffer
     *
     * Override for post-dispatch synchronization or state updates.
     */
    virtual void on_after_dispatch(Portal::Graphics::CommandBufferID cmd_id, const std::shared_ptr<VKBuffer>& buffer);

    /**
     * @brief Calculate dispatch size from buffer
     * @param buffer Buffer to process
     * @return {group_count_x, group_count_y, group_count_z}
     *
     * Override for custom dispatch calculation logic.
     * Default implementation uses m_config.dispatch settings.
     */
    virtual std::array<uint32_t, 3> calculate_dispatch_size(const std::shared_ptr<VKBuffer>& buffer);

    //==========================================================================
    // Protected State - Available to Subclasses
    //==========================================================================

    ShaderProcessorConfig m_config;

    Portal::Graphics::ShaderID m_shader_id = Portal::Graphics::INVALID_SHADER;
    Portal::Graphics::ComputePipelineID m_pipeline_id = Portal::Graphics::INVALID_COMPUTE_PIPELINE;
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
    virtual void initialize_pipeline();

private:
    //==========================================================================
    // Internal Implementation
    //==========================================================================

    void initialize_shader();
    void initialize_descriptors();
    void update_descriptors();
    void execute_dispatch(const std::shared_ptr<VKBuffer>& buffer);
    void cleanup();
};

template <typename T>
void ShaderProcessor::set_push_constant_data(const T& data)
{
    static_assert(sizeof(T) <= 128, "Push constants typically limited to 128 bytes");
    m_push_constant_data.resize(sizeof(T));
    std::memcpy(m_push_constant_data.data(), &data, sizeof(T));
}
} // namespace MayaFlux::Buffers
