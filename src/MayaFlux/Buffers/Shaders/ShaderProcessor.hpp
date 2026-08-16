#pragma once

#include "MayaFlux/Buffers/Staging/StagingUtils.hpp"

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
    uint32_t count = 1; ///< Array count for array descriptors (default 1)

    ShaderBinding() = default;

    /**
     * @brief Construct with semantic role — preferred public API.
     */
    ShaderBinding(uint32_t s, uint32_t b, Portal::Graphics::DescriptorRole role = Portal::Graphics::DescriptorRole::STORAGE, uint32_t c = 1)
        : set(s)
        , binding(b)
        , type(to_vk_descriptor_type(role))
        , count(c)
    {
    }

    /**
     * @brief Construct with explicit Vulkan type — internal / advanced use only.
     */
    ShaderBinding(uint32_t s, uint32_t b, vk::DescriptorType t, uint32_t c = 1)
        : set(s)
        , binding(b)
        , type(t)
        , count(c)
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
    Portal::Graphics::ShaderID shader_id { Portal::Graphics::INVALID_SHADER };

    std::unordered_map<std::string, ShaderBinding> bindings;

    size_t push_constant_size = 0;

    std::vector<Portal::Graphics::PushConstantField> pc_fields; ///< Retained from a ShaderSpec so feeds can resolve a field name to an offset. Empty for file shaders.

    std::unordered_map<uint32_t, uint32_t> specialization_constants;

    ShaderConfig() = default;
    ShaderConfig(std::string path)
        : shader_path(std::move(path))
    {
    }
    ShaderConfig(const Portal::Graphics::ShaderSpec& spec)
        : shader_id(Portal::Graphics::get_shader_foundry().load_shader(spec))
        , push_constant_size(spec.push_constant_bytes)
        , pc_fields(spec.pc_fields)
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

    void processing_function(const std::shared_ptr<Buffer>& buffer) override;
    void on_attach(const std::shared_ptr<Buffer>& buffer) override;
    void on_detach(const std::shared_ptr<Buffer>& buffer) override;

    [[nodiscard]] bool is_compatible_with(const std::shared_ptr<Buffer>& buffer) const override;

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

    /**
     * @brief Download the buffer currently bound to a named descriptor.
     * @param descriptor_name Logical name previously bound via bind_buffer.
     * @param data Destination pointer, at least the bound buffer's size in bytes.
     * @param size Byte count to copy.
     * @param staging Optional staging buffer, forwarded to download_from_gpu.
     * @return True if the name resolved to a bound buffer and the download ran.
     */
    bool download_bound(
        const std::string& descriptor_name,
        void* data,
        size_t size,
        const std::shared_ptr<VKBuffer>& staging = nullptr) const;

    template <typename T>
    bool download_bound(const std::string& descriptor_name, std::vector<T>& data) const
    {
        auto buffer = get_bound_buffer(descriptor_name);
        if (!buffer) {
            MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "download_bound: no buffer bound to descriptor '{}'", descriptor_name);
            return false;
        }
        download_from_gpu(buffer, data);
        return true;
    }

    //==========================================================================
    // Feeds
    //==========================================================================

    /**
     * @brief What a feed callable returns.
     *
     * A double lands in the push constant block. A DataVariant lands in a
     * storage descriptor. Which one an entry expects is fixed when it is
     * registered by the name it resolves to.
     */
    using FeedValue = std::variant<double, Kakshya::DataVariant>;

    /** @brief A callable pulled once per processing cycle. */
    using FeedSource = std::function<FeedValue()>;

    /**
     * @brief Supply a shader input from a callable, resolved by name.
     * @param name A name already declared on this shader: a descriptor in
     *        config.bindings, or a push constant field from the ShaderSpec
     *        this processor was constructed from.
     * @param source Callable pulled each cycle. Returns a DataVariant for a
     *        descriptor name, a double for a push constant field.
     *
     * Resolution happens once, here. Descriptor names take precedence; a
     * name matching neither is an error and registers nothing. File-shader
     * processors have no record of push constant field names, so their
     * constant feeds must use the explicit overload.
     */
    void feed(const std::string& name, FeedSource source);

    /**
     * @brief Supply a push constant from a callable at an explicit offset.
     * @param name Logical name, used for removal and error reporting. Need
     *        not match anything on the shader.
     * @param source Callable returning a double.
     * @param offset Byte offset in the push constant block.
     * @param size Byte width written. Four narrows to float, eight keeps
     *        double. Other values are rejected.
     *
     * The form hand-written shaders need, since their push constant block
     * is described nowhere the processor can read.
     */
    void feed(const std::string& name, FeedSource source, uint32_t offset, size_t size = sizeof(float));

    /** @brief Remove a feed. Any buffer it created is released. */
    void remove_feed(const std::string& name);

    /** @brief Whether a feed of this name is registered. */
    [[nodiscard]] bool has_feed(const std::string& name) const;

    /** @brief Names of every registered feed. */
    [[nodiscard]] std::vector<std::string> get_feed_names() const;

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
    virtual void set_push_constant_data_raw(const void* data, size_t size);

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
     * @brief Submit asynchronously and resolve at the top of a later cycle.
     * @param deferred True to submit via submit_async, false for submit_and_wait.
     *
     * Only meaningful for children that submit their own command buffers.
     * Children that record without submitting, such as RenderProcessor,
     * are unaffected. Switching back to synchronous while a submission is
     * outstanding waits for and releases it first.
     */
    void set_deferred_submission(bool deferred);

    /** @brief Whether this processor submits asynchronously. */
    [[nodiscard]] bool is_deferred_submission() const { return m_deferred_submission; }

    /** @brief True while an asynchronous submission is outstanding. */
    [[nodiscard]] bool is_dispatch_pending() const
    {
        return m_pending_fence != Portal::Graphics::INVALID_FENCE;
    }

    /**
     * @brief Resolve an outstanding asynchronous submission.
     * @param block True to wait for the fence, false to return immediately
     *        when it is not yet signaled.
     * @return True if a submission was resolved by this call.
     *
     * On resolution invokes on_dispatch_complete, then release_fence, which
     * also frees the associated command buffer. Invoked with block=false at
     * the top of processing_function and with block=true from cleanup.
     */
    bool resolve_pending_dispatch(bool block);

    /**
     * @brief Check if compute has been executed at least once
     * @return True if processing_function() has been called
     */
    [[nodiscard]] virtual inline bool has_executed() const
    {
        return m_last_command_buffer != Portal::Graphics::INVALID_COMMAND_BUFFER;
    }

protected:
    /**
     * @brief Byte width of this processor's push constant block, extended to
     *        cover any fragment staged on the buffer.
     */
    [[nodiscard]] size_t resolve_push_constant_size(const std::shared_ptr<VKBuffer>& buffer) const;

    /**
     * @brief This processor's push constant data with buffer-staged fragments
     *        overlaid at their declared offsets.
     */
    [[nodiscard]] std::vector<uint8_t> resolve_push_constants(const std::shared_ptr<VKBuffer>& buffer) const;

    /**
     * @brief Pull every feed and write its result.
     *
     * Called at the top of processing_function, before descriptors are
     * rebuilt, so a storage feed creating its buffer binds the same cycle.
     * Callables run on whichever thread drives the chain.
     */
    void pump_feeds();

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
     * @return True to proceed with execution, false to skip
     *
     * Override to update push constants or dynamic descriptors.
     */
    virtual bool on_before_execute(Portal::Graphics::CommandBufferID cmd_id, const std::shared_ptr<VKBuffer>& buffer);

    /**
     * @brief Called after each process callback
     * @param cmd Command buffer
     * @param buffer Currently processed buffer
     *
     * Override for post-dispatch synchronization or state updates.
     */
    virtual void on_after_execute(Portal::Graphics::CommandBufferID cmd_id, const std::shared_ptr<VKBuffer>& buffer);

    /**
     * @brief Called once when an asynchronous submission is observed complete.
     * @param buffer Buffer processed by the completed submission.
     *
     * The correct place for device-to-host readback under deferred
     * submission, since on_after_execute runs at record time, before the
     * GPU has executed anything. Never invoked under synchronous
     * submission, where submit_and_wait already precedes the return.
     */
    virtual void on_dispatch_complete(const std::shared_ptr<VKBuffer>& buffer);

    /**
     * @brief Submit a recorded command buffer honoring the submission mode.
     * @param cmd_id Command buffer to submit.
     * @param buffer Buffer being processed. Retained until resolution when
     *        deferred, so on_dispatch_complete receives the same instance.
     *
     * Synchronous submission calls submit_and_wait and returns. Deferred
     * submission calls submit_async and stores the fence; a failed
     * submission falls back to leaving nothing pending.
     */
    void submit_recorded(Portal::Graphics::CommandBufferID cmd_id, const std::shared_ptr<VKBuffer>& buffer);

    /**
     * @brief Resolve logical descriptor set index to actual index
     * @param set Logical set index from ShaderBinding
     * @return Resolved set index, or std::nullopt if invalid
     *
     * Handles cases where the engine reserves set 0 for global resources.
     * If m_engine_owns_set_zero is true, logical set 0 maps to no descriptor,
     * and logical sets are offset by +1 in the actual descriptor sets.
     */
    [[nodiscard]] std::optional<uint32_t> resolve_ds_index(uint32_t set) const;

    //==========================================================================
    // Protected State - Available to Subclasses
    //==========================================================================

    ShaderConfig m_config;

    Portal::Graphics::ShaderID m_shader_id = Portal::Graphics::INVALID_SHADER;
    std::vector<Portal::Graphics::DescriptorSetID> m_descriptor_set_ids;
    Portal::Graphics::CommandBufferID m_last_command_buffer = Portal::Graphics::INVALID_COMMAND_BUFFER;

    std::unordered_map<std::string, std::shared_ptr<VKBuffer>> m_bound_buffers;
    std::shared_ptr<VKBuffer> m_last_processed_buffer;

    bool m_deferred_submission {}; ///< False submits synchronously, preserving pre-existing behaviour.
    Portal::Graphics::FenceID m_pending_fence { Portal::Graphics::INVALID_FENCE }; ///< Outstanding async submission, if any.
    std::shared_ptr<VKBuffer> m_pending_buffer; ///< Buffer retained for the outstanding submission.

    std::vector<uint8_t> m_push_constant_data;

    bool m_initialized {};
    bool m_needs_pipeline_rebuild = true;
    bool m_needs_descriptor_rebuild = true;

    size_t m_auto_bind_index {};

    /**
     * @brief Whether the engine reserves set=0 for global resources
     *
     * Defaults to false. Only RenderProcessor sets this to true in its
     * constructor. When true, resolve_ds_index() maps logical set=0 to
     * nullopt (no user descriptor) and offsets all other sets by -1.
     * Compute subclasses leave this false: their descriptor sets are
     * numbered from set=0 with no engine reservation.
     *
     * A future subclass that needs engine-owned sets must set this
     * explicitly and be aware of the index offset applied by resolve_ds_index.
     */
    bool m_engine_owns_set_zero {};

    virtual void initialize_pipeline(const std::shared_ptr<VKBuffer>& buffer) = 0;
    virtual void initialize_descriptors(const std::shared_ptr<VKBuffer>& buffer) = 0;
    virtual void execute_shader(const std::shared_ptr<VKBuffer>& buffer) = 0;

    virtual void update_descriptors(const std::shared_ptr<VKBuffer>& buffer);
    virtual void cleanup();

private:
    //==========================================================================
    // Internal Implementation
    //==========================================================================

    /**
     * @struct Feed
     * @brief One registered callable and where its result is written.
     */
    struct Feed {
        FeedSource source;
        bool is_storage {};
        uint32_t offset {};
        size_t size {};
        std::string descriptor_name;
        std::shared_ptr<VKBuffer> buffer;
        bool mismatch_logged {};
    };

    std::unordered_map<std::string, Feed> m_feeds;

    void initialize_shader();
};

template <typename T>
void ShaderProcessor::set_push_constant_data(const T& data)
{
    const auto size = sizeof(T);
    static_assert(size <= 128, "Push constants typically limited to 128 bytes");
    if (m_push_constant_data.size() < size) {
        set_push_constant_size(size);
    }

    std::memcpy(m_push_constant_data.data(), &data, size);
}

} // namespace MayaFlux::Buffers
