#pragma once

#include "MayaFlux/Kinesis/Viewport/Scissor.hpp"
#include "MayaFlux/Kinesis/Viewport/ViewTransform.hpp"
#include "MayaFlux/Portal/Graphics/PrimitiveMill.hpp"
#include "MayaFlux/Portal/Graphics/RenderFlow.hpp"
#include "ShaderProcessor.hpp"

namespace MayaFlux::Core {
class VKImage;
}

namespace MayaFlux::Buffers {

/**
 * @class RenderShaderProcessor
 * @brief Graphics rendering processor - inherits from ShaderProcessor
 *
 * Overrides pipeline creation to use RenderFlow instead of ComputePress.
 * Records draw commands but does NOT submit/present.
 */
class MAYAFLUX_API RenderProcessor : public ShaderProcessor {
public:
    RenderProcessor(const ShaderConfig& config);

    ~RenderProcessor() override
    {
        cleanup();
    }

    void set_geometry_shader(const std::string& geometry_path);
    void set_tess_control_shader(const std::string& tess_control_path);
    void set_tess_eval_shader(const std::string& tess_eval_path);
    void set_fragment_shader(const std::string& fragment_path);
    void set_target_window(const std::shared_ptr<Core::Window>& window, const std::shared_ptr<VKBuffer>& buffer);

    Portal::Graphics::RenderPipelineID get_render_pipeline_id() const { return m_pipeline_id; }

    void on_attach(const std::shared_ptr<Buffer>& buffer) override;

    /** @brief Withdraw presentation on chain detachment; resource teardown remains in cleanup. */
    void on_detach(const std::shared_ptr<Buffer>& buffer) override;

    /** @brief Change an attached buffer's visibility without stopping its processing. */
    void set_visible(bool visible, const std::shared_ptr<VKBuffer>& buffer);

    /** @brief Whether this processor contributes to presentation. */
    bool is_visible(const std::shared_ptr<VKBuffer>& buffer) const;

    /**
     * @brief Record the buffer's prepared geometry into a fresh secondary command buffer.
     *
     * Does not advance feeds, upload geometry, or dispatch milling. Both normal
     * processing and presentation refreshes use this operation. A buffer that has
     * not prepared geometry yet records nothing.
     */
    void record_draw(const std::shared_ptr<VKBuffer>& buffer);

    /// Set primitive topology (e.g., triangle list, line list, point list)
    inline void set_primitive_topology(Portal::Graphics::PrimitiveTopology topology)
    {
        m_primitive_topology = topology;
        m_needs_pipeline_rebuild = true;
    }

    /// Set polygon mode (e.g., fill, line, point)
    inline void set_polygon_mode(Portal::Graphics::PolygonMode mode)
    {
        m_polygon_mode = mode;
        m_needs_pipeline_rebuild = true;
    }

    /// Set cull mode (e.g., none, front, back)
    inline void set_cull_mode(Portal::Graphics::CullMode mode)
    {
        m_cull_mode = mode;
        m_needs_pipeline_rebuild = true;
    }

    /**
     * @brief Bind a texture to a descriptor binding point
     * @param binding Binding index (matches shader layout(binding = N))
     * @param texture VKImage texture to bind
     * @param sampler Optional sampler (uses default linear if null)
     */
    void bind_texture(
        uint32_t binding,
        const std::shared_ptr<Core::VKImage>& texture,
        vk::Sampler sampler = nullptr);

    /**
     * @brief Bind a texture to a named descriptor
     * @param descriptor_name Logical name (must be in config.bindings)
     * @param texture VKImage texture to bind
     * @param sampler Optional sampler (uses default linear if null)
     */
    void bind_texture(
        const std::string& descriptor_name,
        const std::shared_ptr<Core::VKImage>& texture,
        vk::Sampler sampler = nullptr);

    /**
     * @brief Check if pipeline is created
     */
    bool is_pipeline_ready() const { return m_pipeline_id != Portal::Graphics::INVALID_RENDER_PIPELINE; }

    /**
     * @brief Set vertex range for drawing subset of buffer
     * @param first_vertex Starting vertex index in buffer
     * @param vertex_count Number of vertices to draw
     *
     * Enables drawing a specific range of vertices from the bound buffer.
     * Used for composite geometry where multiple collections are aggregated
     * into a single buffer but rendered with different topologies.
     *
     * Default: draws all vertices (first_vertex=0, vertex_count=0 means "use layout count")
     */
    void set_vertex_range(uint32_t first_vertex, uint32_t vertex_count);

    /**
     * @brief Override the vertex layout used when building the pipeline for buffer
     * @param buffer Target buffer (key into m_buffer_info)
     * @param layout Layout specific to this processor's topology
     *
     * Called by CompositeGeometryBuffer to give each RenderProcessor its own
     * topology-specific layout rather than the shared aggregate on the VKBuffer.
     * Triggers a pipeline rebuild on the next execute_shader() call.
     */
    void set_buffer_vertex_layout(
        const std::shared_ptr<VKBuffer>& buffer,
        const Kakshya::VertexLayout& layout);

    /**
     * @brief Set blend mode for color attachment
     * @param config Blend attachment configuration
     */
    inline void set_blend_attachment(const Portal::Graphics::BlendAttachmentConfig& config)
    {
        m_blend_attachment = config;
        m_needs_pipeline_rebuild = true;
    }

    /** @brief Enable standard alpha blending (src_alpha, one_minus_src_alpha). Does not touch depth state. */
    void enable_alpha_blending();

    /** @brief Disable blending on the color attachment, restoring opaque writes. Does not touch depth state. */
    void disable_alpha_blending();

    /**
     * @brief Toggle standard alpha blending.
     * @param enabled true enables src_alpha/one_minus_src_alpha blending, false restores opaque.
     *
     * Blending and depth are independent. Toggling blend never changes depth
     * test or depth write; use enable_depth_test / disable_depth_test for that.
     */
    void set_alpha_blending(bool enabled);

    /** @brief Query whether the color attachment currently has blending enabled. */
    [[nodiscard]] bool is_alpha_blending_enabled() const
    {
        return m_blend_attachment.has_value() && m_blend_attachment->blend_enable;
    }

    /**
     * @brief Enable depth testing for this processor's pipeline
     * @param compare_op Depth comparison operation (default: LESS)
     *
     * Marks the owning buffer as requiring a depth attachment.
     * Pipeline will be created with D32_SFLOAT depth format.
     */
    void enable_depth_test(Portal::Graphics::CompareOp compare_op = Portal::Graphics::CompareOp::LESS);

    /**
     * @brief Disable depth testing and depth writes for this processor's pipeline.
     *
     * Leaves the buffer's depth attachment requirement intact so the pass still
     * has a depth buffer available; only the per-pipeline test and write are off.
     */
    void disable_depth_test();

    /**
     * @brief Set static view transform (evaluated once)
     * @param vt View and projection matrices
     * @param enable_depth Enable the depth test if it is not already on.
     *        False leaves depth state untouched, which is what volume
     *        proxy geometry wants: a box that writes depth occludes every
     *        later volume drawn at the same geometry.
     * @param cull Face culling mode. BACK suits opaque geometry. Volume
     *        rendering rasterises back faces so the entry clip works from
     *        inside the bounds.
     */
    void set_view_transform(
        const Kinesis::ViewTransform& vt,
        bool enable_depth = true,
        Portal::Graphics::CullMode cull = Portal::Graphics::CullMode::BACK);

    /**
     * @brief Set dynamic view transform source (evaluated every frame)
     * @param fn Callable returning ViewTransform, invoked each execute_shader
     * @param enable_depth Enable the depth test if it is not already on.
     * @param cull Face culling mode.
     */
    void set_view_transform_source(
        std::function<Kinesis::ViewTransform()> fn,
        bool enable_depth = true,
        Portal::Graphics::CullMode cull = Portal::Graphics::CullMode::BACK);

    /** @brief Get current static view transform, if set */
    const std::optional<Kinesis::ViewTransform>& get_view_transform() const { return m_view_transform; }

    /** @brief Get current dynamic view transform source, if set */
    const std::function<Kinesis::ViewTransform()>& get_view_transform_source() const { return m_view_transform_source; }

    /**
     * @brief Set number of instances for the next draw call.
     * @param count Instance count. 1 is the default (non-instanced draw).
     */
    void set_instance_count(uint32_t count) { m_instance_count = count; }
    [[nodiscard]] uint32_t get_instance_count() const { return m_instance_count; }

    /**
     * @brief Reduce supplied spans to triangles before drawing them.
     *
     * When enabled, the topology given to set_primitive_topology() describes
     * the spans handed to set_runs() rather than the pipeline: it is built as
     * TRIANGLE_LIST and no geometry stage is used. Points become quads and line
     * segments ribbons, all drawn by one non-indexed draw. The vertex layout is
     * preserved, so the vertex shader is unaffected.
     *
     * Resolved from RenderConfig::triangulate at configuration time, because
     * shader and geometry-stage selection depend on it and happen there.
     */
    void set_triangulate(bool enabled);
    [[nodiscard]] bool is_triangulate() const { return m_triangulate; }

    /**
     * @brief Clip this buffer's draws to an NDC region instead of the full framebuffer.
     *
     * Resolved from RenderConfig::scissor at configuration time. Unset means
     * the full framebuffer, unchanged from before this existed. The pixel
     * rect is recomputed from live swapchain dimensions every frame in
     * execute_shader, so a set scissor stays correct across a resize.
     */
    void set_scissor(std::optional<Kinesis::Scissor> scissor) { m_scissor = scissor; }
    [[nodiscard]] const std::optional<Kinesis::Scissor>& get_scissor() const { return m_scissor; }

    /**
     * @brief Spans to draw on subsequent frames. Only used when triangulating.
     * @param runs Spans in recording order.
     *
     * Left empty, triangulation covers the whole buffer as one span at the
     * topology given to set_primitive_topology(), honouring any range set by
     * set_vertex_range(). Supplying spans is how a caller whose geometry mixes
     * topologies overrides that.
     *
     * Not thread safe against a concurrent processing_function call; call from
     * the thread driving the graphics tick.
     */
    void set_runs(std::vector<Portal::Graphics::DrawRun> runs);

    /** @brief Spans as last supplied to set_runs(). */
    [[nodiscard]] const std::vector<Portal::Graphics::DrawRun>& get_runs() const { return m_runs; }

    /** @brief Shaping parameters for triangulation. Takes effect next frame. */
    void set_mill_spec(const Portal::Graphics::MillSpec& spec);
    [[nodiscard]] const Portal::Graphics::MillSpec& get_mill_spec() const { return m_mill_spec; }

    /** @brief Vertices the last triangulation produced. Diagnostic only. */
    [[nodiscard]] uint32_t milled_vertex_count() const;

protected:
    void initialize_pipeline(const std::shared_ptr<VKBuffer>& buffer) override;

    /**
     * @brief Prepare this buffer's geometry and record its draw commands.
     *
     * Publishes the view transform and advances triangulation before recording.
     * Valid empty geometry still records a secondary command buffer so normal
     * processing can present an empty frame. Presentation refreshes call
     * record_draw() directly to reuse the prepared geometry.
     */
    void execute_shader(const std::shared_ptr<VKBuffer>& buffer) override;
    void initialize_descriptors(const std::shared_ptr<VKBuffer>& buffer) override;

    bool on_before_execute(Portal::Graphics::CommandBufferID cmd_id, const std::shared_ptr<VKBuffer>& buffer) override;

    void cleanup() override;

    /**
     * @brief Resolve the active view transform and write it to the UBO.
     * @return The transform just published.
     *
     * Host-side only, no command recording. Runs before the triangulation
     * dispatch so that reads this frame's camera. Prefer pushing the returned
     * value into a dispatch over binding the UBO, which has no
     * per-frame-in-flight copies.
     */
    const Kinesis::ViewTransform& publish_view_transform();

    /** @brief Transform published for this frame by publish_view_transform(). */
    [[nodiscard]] const Kinesis::ViewTransform& published_view_transform() const
    {
        return m_published_view_transform;
    }

    /** @brief Resolve the active scissor against a live framebuffer size. Full framebuffer when unset. */
    [[nodiscard]] vk::Rect2D resolve_scissor_rect(uint32_t width, uint32_t height) const noexcept;

private:
    struct BufferState {
        Kakshya::VertexLayout semantic_layout;
        bool use_reflection {};
        std::unique_ptr<Portal::Graphics::PrimitiveMill> mill;
        std::shared_ptr<VKBuffer> draw_source;
        uint32_t first_vertex {};
        uint32_t vertex_count {};
        uint32_t index_count {};
        bool geometry_prepared {};
    };

    Portal::Graphics::RenderPipelineID m_pipeline_id = Portal::Graphics::INVALID_RENDER_PIPELINE;
    Portal::Graphics::ShaderID m_geometry_shader_id = Portal::Graphics::INVALID_SHADER;
    Portal::Graphics::ShaderID m_tess_control_shader_id = Portal::Graphics::INVALID_SHADER;
    Portal::Graphics::ShaderID m_tess_eval_shader_id = Portal::Graphics::INVALID_SHADER;
    Portal::Graphics::ShaderID m_fragment_shader_id = Portal::Graphics::INVALID_SHADER;
    Portal::Graphics::DescriptorSetID m_view_transform_descriptor_set_id {
        Portal::Graphics::INVALID_DESCRIPTOR_SET
    };
    std::shared_ptr<Core::Window> m_target_window;

    std::unordered_map<std::shared_ptr<VKBuffer>, BufferState> m_buffer_info;
    /** @brief Non-owning exceptions to default visibility, removed on detach. */
    std::vector<const VKBuffer*> m_hidden_buffers;
    Registry::Service::DisplayService* m_display_service = nullptr;

    Portal::Graphics::PrimitiveTopology m_primitive_topology { Portal::Graphics::PrimitiveTopology::TRIANGLE_LIST };
    Portal::Graphics::PolygonMode m_polygon_mode { Portal::Graphics::PolygonMode::FILL };
    Portal::Graphics::CullMode m_cull_mode { Portal::Graphics::CullMode::NONE };

    struct TextureBinding {
        std::shared_ptr<Core::VKImage> texture;
        vk::Sampler sampler;
    };
    std::unordered_map<uint32_t, TextureBinding> m_texture_bindings;

    std::optional<Portal::Graphics::BlendAttachmentConfig> m_blend_attachment;

    Portal::Graphics::DepthStencilConfig m_depth_stencil;

    bool m_depth_enabled {};
    std::shared_ptr<VKBuffer> m_view_transform_ubo;
    bool m_view_transform_active {};
    uint32_t m_first_vertex { 0 };
    uint32_t m_vertex_count { 0 };
    uint32_t m_instance_count { 1 };

    std::optional<Kinesis::ViewTransform> m_view_transform;
    std::function<Kinesis::ViewTransform()> m_view_transform_source;
    Kinesis::ViewTransform m_published_view_transform {};

    bool m_triangulate {};
    std::optional<Kinesis::Scissor> m_scissor;
    std::vector<Portal::Graphics::DrawRun> m_runs;
    Portal::Graphics::MillSpec m_mill_spec {};

    uint32_t m_last_milled_vertex_count {};

    /**
     * @brief Topology the pipeline is built with, as opposed to the topology
     *        m_runs are declared in.
     */
    [[nodiscard]] Portal::Graphics::PrimitiveTopology pipeline_topology() const;

    /**
     * @brief Mill m_runs into this attachment's output.
     * @return Number of vertices prepared for drawing.
     */
    uint32_t mill_runs(const std::shared_ptr<VKBuffer>& buffer, Portal::Graphics::PrimitiveMill& mill);

    /** @brief Prepare and retain the source and draw range for one attached buffer. */
    void prepare_geometry(const std::shared_ptr<VKBuffer>& buffer, BufferState& state);

    /** @brief Record an already validated attachment without repeating its lookup. */
    void record_draw(const std::shared_ptr<VKBuffer>& buffer, const BufferState& state);

    BufferState* get_or_cache_buffer_state(const std::shared_ptr<VKBuffer>& buffer);
};

} // namespace MayaFlux::Buffers
