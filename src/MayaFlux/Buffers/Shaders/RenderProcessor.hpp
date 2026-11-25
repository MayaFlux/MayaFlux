#pragma once

#include "MayaFlux/Kinesis/Viewport/Scissor.hpp"
#include "MayaFlux/Kinesis/Viewport/ViewTransform.hpp"
#include "MayaFlux/Kinesis/Viewport/ViewportArea.hpp"
#include "MayaFlux/Portal/Graphics/ImageTarget.hpp"
#include "MayaFlux/Portal/Graphics/PrimitiveMill.hpp"
#include "MayaFlux/Portal/Graphics/RenderFlow.hpp"
#include "ShaderProcessor.hpp"

namespace MayaFlux::Core {
class VKImage;
}

namespace MayaFlux::Buffers {

/**
 * @struct DestinationView
 * @brief The view one destination of a RenderProcessor draws with.
 *
 * Names the destination with exactly one of @c window and @c target, and the
 * view with exactly one of @c transform and @c source. A source runs once per
 * cycle for its destination and is given the pixel size of what that
 * destination fills: the window, the target, or the part of the target its
 * area names, so one source gets the aspect right everywhere.
 *
 * @code
 * rp->set_destination_view({ .window = window, .transform = camera });
 * rp->set_destination_view({ .target = target, .source = [](glm::uvec2 size) {
 *     return look_at_origin(static_cast<float>(size.x) / static_cast<float>(size.y));
 * } });
 * @endcode
 *
 * @c enable_depth and @c cull are as RenderProcessor::set_view_transform takes
 * them. They are state of the processor, so they apply to every destination.
 */
struct DestinationView {
    std::shared_ptr<Core::Window> window;
    std::shared_ptr<Portal::Graphics::ImageTarget> target;
    std::optional<Kinesis::ViewTransform> transform;
    std::function<Kinesis::ViewTransform(glm::uvec2)> source;
    bool enable_depth { true };
    Portal::Graphics::CullMode cull { Portal::Graphics::CullMode::BACK };
};

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
    /**
     * @brief Make a window the only destination of this processor.
     *
     * Every other destination is dropped, and the pipeline is rebuilt for the
     * window's formats.
     */
    void set_target_window(const std::shared_ptr<Core::Window>& window, const std::shared_ptr<VKBuffer>& buffer);

    /**
     * @brief Make an image target the only destination of this processor.
     *
     * Every other destination is dropped, and the pipeline is rebuilt for the
     * image's formats. The target redraws. @p area picks the layer and the part
     * of it that is drawn.
     */
    void set_target_image(
        const std::shared_ptr<Portal::Graphics::ImageTarget>& image,
        const std::shared_ptr<VKBuffer>& buffer,
        const Kinesis::ViewportArea& area = {});

    /**
     * @brief Make a window and image targets the destinations of this processor.
     *
     * The window takes the first place when there is one. With no window the
     * first image does, and the others are added. Every earlier destination is
     * dropped. Logs when there is no destination at all.
     */
    void set_destinations(
        const std::shared_ptr<Core::Window>& window,
        const std::vector<std::shared_ptr<Portal::Graphics::ImageTarget>>& images,
        const std::shared_ptr<VKBuffer>& buffer);

    /** @brief Also draw into a window. Ignored when the window is already a destination. */
    void add_target_window(const std::shared_ptr<Core::Window>& window, const std::shared_ptr<VKBuffer>& buffer);

    /**
     * @brief Also draw into an image target, into the layer and part of it @p area names.
     *
     * When the target is already a destination only its area changes.
     */
    void add_target_image(
        const std::shared_ptr<Portal::Graphics::ImageTarget>& image,
        const std::shared_ptr<VKBuffer>& buffer,
        const Kinesis::ViewportArea& area = {});

    /**
     * @brief Change the layer and part of an image target this processor draws into.
     *
     * The target redraws. Does nothing for a target that is not a destination,
     * or a layer the target does not have.
     */
    void set_target_area(
        const std::shared_ptr<Portal::Graphics::ImageTarget>& image,
        const Kinesis::ViewportArea& area);

    /** @brief Where this processor draws into an image target. The whole first layer when it is not a destination. */
    [[nodiscard]] Kinesis::ViewportArea get_target_area(
        const std::shared_ptr<Portal::Graphics::ImageTarget>& image) const;

    /** @brief Stop drawing into a window and release its pipeline. */
    void remove_target_window(const std::shared_ptr<Core::Window>& window, const std::shared_ptr<VKBuffer>& buffer);

    /** @brief Stop drawing into an image target and release its pipeline. The target redraws. */
    void remove_target_image(
        const std::shared_ptr<Portal::Graphics::ImageTarget>& image,
        const std::shared_ptr<VKBuffer>& buffer);

    /** @brief Draw into the window, or keep its pipeline and skip it, without changing the other destinations. */
    void set_window_enabled(bool enabled);

    /** @brief Draw into an image target, or keep its pipeline and skip it, without changing the other destinations. */
    void set_target_enabled(const std::shared_ptr<Portal::Graphics::ImageTarget>& image, bool enabled);

    /** @brief The window this processor draws into, or null. */
    [[nodiscard]] std::shared_ptr<Core::Window> get_target_window() const;

    /** @brief The first image target this processor draws into, or null. */
    [[nodiscard]] std::shared_ptr<Portal::Graphics::ImageTarget> get_target_image() const;

    /** @brief Every image target this processor draws into. */
    [[nodiscard]] std::vector<std::shared_ptr<Portal::Graphics::ImageTarget>> get_target_images() const;

    /** @brief Whether this processor samples the image of @p target, however it was bound, so the root can draw that target first. */
    [[nodiscard]] bool samples(const std::shared_ptr<Portal::Graphics::ImageTarget>& target) const;

    /** @brief The pipeline of the first destination. */
    Portal::Graphics::RenderPipelineID get_render_pipeline_id() const { return m_lanes.front()->pipeline; }

    /** @brief Whether a pipeline belongs to one of this processor's destinations. */
    [[nodiscard]] bool owns_pipeline(Portal::Graphics::RenderPipelineID id) const;

    /** @brief Whether this processor draws into the window. */
    [[nodiscard]] bool targets_window(const std::shared_ptr<Core::Window>& window) const { return find_lane(window) != nullptr; }

    /** @brief How many places this processor draws into. */
    [[nodiscard]] size_t destination_count() const noexcept
    {
        return static_cast<size_t>(std::ranges::count_if(m_lanes, [](const auto& lane) { return lane->window || lane->image; }));
    }

    void on_attach(const std::shared_ptr<Buffer>& buffer) override;

    /**
     * @brief Remove a buffer from this processor and its target window.
     *
     * Processor-wide pipeline and window state remain active for other buffers.
     */
    void on_detach(const std::shared_ptr<Buffer>& buffer) override;

    /** @brief Change an attached buffer's visibility without stopping its processing. */
    void set_visible(bool visible, const std::shared_ptr<VKBuffer>& buffer);

    /** @brief Whether this processor contributes to presentation. */
    bool is_visible(const std::shared_ptr<VKBuffer>& buffer) const;

    /**
     * @brief Record the buffer's prepared geometry into a fresh secondary command buffer.
     * @param pipeline The destination to record for, one of this processor's pipelines.
     *
     * Does not advance feeds, upload geometry, or dispatch milling. Both normal
     * processing and presentation refreshes use this operation. A buffer that has
     * not prepared geometry yet records nothing.
     */
    void record_draw(const std::shared_ptr<VKBuffer>& buffer, Portal::Graphics::RenderPipelineID pipeline);

    /// Set primitive topology (e.g., triangle list, line list, point list)
    inline void set_primitive_topology(Portal::Graphics::PrimitiveTopology topology)
    {
        if (m_primitive_topology == topology) {
            return;
        }
        m_primitive_topology = topology;
        invalidate_pipelines();
        invalidate_all_mills();
    }

    /// Set polygon mode (e.g., fill, line, point)
    inline void set_polygon_mode(Portal::Graphics::PolygonMode mode)
    {
        if (m_polygon_mode == mode) {
            return;
        }
        m_polygon_mode = mode;
        invalidate_pipelines();
        invalidate_all_mills();
    }

    /// Set cull mode (e.g., none, front, back)
    inline void set_cull_mode(Portal::Graphics::CullMode mode)
    {
        if (m_cull_mode == mode) {
            return;
        }
        m_cull_mode = mode;
        invalidate_pipelines();
    }

    /**
     * @brief Bind a texture to a descriptor binding.
     *
     * @code
     * rp->bind_texture({ .name = "tex", .image = image });
     * rp->bind_texture({ .binding = 2, .image = image, .sampler = nearest });
     * rp->bind_texture({ .name = "tex", .target = target });
     * @endcode
     *
     * The sampler is kept for the binding, so a later bind without one reuses
     * it. A binding made with a target is bound again to target->image() on the
     * next execute after the target is resized. Nothing is bound, and an error
     * is logged, for an unknown name, a missing image, or both image and target.
     */
    void bind_texture(const Portal::Graphics::SampledTexture& texture);

    /**
     * @brief Choose the sampler a binding uses when it is bound without one.
     *
     * Applies to later binds, so it can be set before the texture exists. When
     * a texture is already bound at the binding, its sampler changes too, and
     * the descriptors are rebuilt on the next execute.
     */
    void set_binding_sampler(uint32_t binding, const Portal::Graphics::SamplerConfig& config);

    /** @brief Named form of set_binding_sampler. */
    void set_binding_sampler(const std::string& descriptor_name, const Portal::Graphics::SamplerConfig& config);

    /**
     * @brief Check if pipeline is created
     */
    bool is_pipeline_ready() const { return m_lanes.front()->pipeline != Portal::Graphics::INVALID_RENDER_PIPELINE; }

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
        if (m_blend_attachment.has_value() && *m_blend_attachment == config) {
            return;
        }
        m_blend_attachment = config;
        invalidate_pipelines();
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

    /**
     * @brief Give one destination its own view.
     *
     * That destination draws with it, and the others keep theirs or the view of
     * set_view_transform and set_view_transform_source. Setting it again
     * replaces it, and it goes when the destination is removed. An image target
     * redraws. The geometry a world facing ribbon or billboard is turned by is
     * generated once for all destinations, towards the eye of the first one drawn.
     *
     * @return False, changing nothing, when the destination is not one of this
     *         processor's, or the description does not name exactly one
     *         destination and exactly one view.
     */
    bool set_destination_view(const DestinationView& view);

    /**
     * @brief Return a destination to the view of the processor.
     *
     * Only @c window or @c target of @p destination is read.
     */
    void clear_destination_view(const DestinationView& destination);

    /** @brief Whether a destination draws with its own view. Only @c window or @c target is read. */
    [[nodiscard]] bool has_destination_view(const DestinationView& destination) const;

    /** @brief Get current static view transform, if set */
    const std::optional<Kinesis::ViewTransform>& get_view_transform() const { return m_view_transform; }

    /** @brief Get current dynamic view transform source, if set */
    const std::function<Kinesis::ViewTransform()>& get_view_transform_source() const { return m_view_transform_source; }

    /** @brief Set a static geometry transform. */
    void set_geometry_transform(const glm::mat4& transform);

    /** @brief Set a geometry transform source evaluated on each render tick. */
    void set_geometry_transform_source(std::function<glm::mat4()> fn);

    /** @brief Get the static geometry transform, if set. */
    const std::optional<glm::mat4>& get_geometry_transform() const { return m_geometry_transform; }

    /** @brief Get the geometry transform source, if set. */
    const std::function<glm::mat4()>& get_geometry_transform_source() const { return m_geometry_transform_source; }

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
     * @brief Spans to draw on subsequent frames.
     * @param runs Spans in recording order.
     *
     * Used when triangulating. Without triangulation they are used only when more
     * than one span is supplied and every span has the topology given to
     * set_primitive_topology(): each span is then drawn on its own, so strip and fan
     * geometry from several sources is not joined into one primitive. Any other
     * combination draws the whole range as a single primitive, as before.
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

    /**
     * @brief Reuse the last milled output while nothing it depends on changed.
     *
     * Off by default, because a buffer whose vertices are written from outside
     * this processor would go stale. Only the owner of a buffer that calls
     * invalidate_mill() after every vertex change should enable it. Spans that
     * follow the viewpoint (Ribbon::WorldFacing) are always milled.
     */
    void set_mill_on_change(bool enabled);

    /** @brief Mill @p buffer again on its next draw. */
    void invalidate_mill(const std::shared_ptr<VKBuffer>& buffer);

    /** @brief Mill every buffer again on its next draw. */
    void invalidate_all_mills();

    /** @brief Dispatches that produced vertices, for observing the gate. */
    [[nodiscard]] uint32_t mill_passes() const;

protected:
    void initialize_pipeline(const std::shared_ptr<VKBuffer>& buffer) override;

    /**
     * @brief Prepare this buffer's geometry and record its draw commands.
     *
     * Publishes the render transform and advances triangulation before recording.
     * Valid empty geometry still records a secondary command buffer so normal
     * processing can present an empty frame. Presentation refreshes call
     * record_draw() directly to reuse the prepared geometry.
     */
    void execute_shader(const std::shared_ptr<VKBuffer>& buffer) override;
    void initialize_descriptors(const std::shared_ptr<VKBuffer>& buffer) override;

    bool on_before_execute(Portal::Graphics::CommandBufferID cmd_id, const std::shared_ptr<VKBuffer>& buffer) override;

    [[nodiscard]] bool tracks_pipeline_layout() const override { return true; }

    void cleanup() override;

    /**
     * @brief Resolve the view and geometry transforms and write them to the UBO.
     * @return The render transform just published.
     *
     * Host-side only, no command recording. Runs before the triangulation
     * dispatch so that reads this frame's camera. Prefer pushing the returned
     * value into a dispatch over binding the UBO, which has no
     * per-frame-in-flight copies.
     */
    const Kinesis::RenderTransform& publish_render_transform();

    /** @brief Resolve the view and geometry transforms without writing them anywhere. */
    const Kinesis::RenderTransform& resolve_render_transform();

    /** @brief Write the resolved transform to the current destination's UBO when it changed. */
    void write_render_transform();

    /** @brief Transform published for this frame by publish_render_transform(). */
    [[nodiscard]] const Kinesis::RenderTransform& published_render_transform() const
    {
        return m_published_render_transform;
    }

    /** @brief Resolve the active scissor against a live framebuffer size. Full framebuffer when unset. */
    [[nodiscard]] vk::Rect2D resolve_scissor_rect(uint32_t width, uint32_t height) const noexcept;

    /** @brief The pixel rect of a destination's area inside a target of the given size. The whole target when the area is unset. */
    [[nodiscard]] static vk::Rect2D resolve_area_rect(
        const Kinesis::ViewportArea& area, uint32_t width, uint32_t height) noexcept;

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
        bool mill_current {};
        uint32_t milled_count {};
    };

    Portal::Graphics::ShaderID m_geometry_shader_id = Portal::Graphics::INVALID_SHADER;
    Portal::Graphics::ShaderID m_tess_control_shader_id = Portal::Graphics::INVALID_SHADER;
    Portal::Graphics::ShaderID m_tess_eval_shader_id = Portal::Graphics::INVALID_SHADER;
    Portal::Graphics::ShaderID m_fragment_shader_id = Portal::Graphics::INVALID_SHADER;

    /**
     * @brief One place this processor draws into, with everything that belongs to it alone.
     *
     * Shaders, geometry, bindings and draw runs are shared by every lane. The
     * first lane keeps its shader processor state in ShaderProcessor, as a
     * processor with one destination always did. The others hold theirs here and
     * trade it with ShaderProcessor while they run.
     */
    struct Lane {
        std::shared_ptr<Core::Window> window;
        std::shared_ptr<Portal::Graphics::ImageTarget> image;
        Kinesis::ViewportArea area;
        std::optional<Kinesis::ViewTransform> view;
        std::function<Kinesis::ViewTransform(glm::uvec2)> view_source;
        Portal::Graphics::RenderPipelineID pipeline { Portal::Graphics::INVALID_RENDER_PIPELINE };
        Portal::Graphics::SlotArena transform_arena;
        size_t transform_slot {};
        std::optional<Kinesis::RenderTransform> written_transform;
        bool enabled { true };

        std::vector<Portal::Graphics::DescriptorSetID> descriptor_sets;
        std::optional<uint64_t> created_layout;
        bool initialized {};
        bool descriptors_ready {};
        bool needs_pipeline { true };
        bool needs_descriptors { true };
    };

    std::vector<std::unique_ptr<Lane>> m_lanes;
    Lane* m_lane { nullptr };

    /** @brief Runs a lane as the current one, trading its shader processor state in and out. */
    class ActiveLane {
    public:
        ActiveLane(RenderProcessor& owner, Lane& lane);
        ~ActiveLane();
        ActiveLane(const ActiveLane&) = delete;
        ActiveLane& operator=(const ActiveLane&) = delete;

    private:
        RenderProcessor& m_owner;
        Lane* m_previous;
        bool m_swapped;
    };

    void exchange_shader_state(Lane& lane);

    template <typename Destination>
    [[nodiscard]] Lane* find_lane(const std::shared_ptr<Destination>& destination) const
    {
        const auto found = std::ranges::find_if(m_lanes, [&destination](const auto& lane) {
            if constexpr (std::is_same_v<Destination, Core::Window>) {
                return lane->window && lane->window == destination;
            } else {
                return lane->image && lane->image == destination;
            }
        });
        return found == m_lanes.end() ? nullptr : found->get();
    }

    [[nodiscard]] Lane& claim_lane();
    [[nodiscard]] Lane* find_view_lane(const DestinationView& destination) const;
    [[nodiscard]] static bool has_view(const Lane& lane) noexcept { return lane.view || lane.view_source; }

    /** @brief Put the current destination's own view, when it has one, into the published transform. */
    void apply_lane_view();
    void clear_destinations(const std::shared_ptr<VKBuffer>& buffer);
    void drop_lane(Lane& lane, const std::shared_ptr<VKBuffer>& buffer);
    void release_window(const std::shared_ptr<Core::Window>& window, const std::shared_ptr<VKBuffer>& buffer);
    void invalidate_pipelines();

    /** @brief What the pipeline and the viewport need to know about the destination. */
    struct DestinationInfo {
        std::vector<vk::Format> colors;
        vk::Format depth {};
        uint32_t width {};
        uint32_t height {};
    };

    [[nodiscard]] bool has_destination() const noexcept { return m_lane->window || m_lane->image; }
    [[nodiscard]] bool destination_ready(const Lane& lane) const;
    [[nodiscard]] DestinationInfo describe() const;
    [[nodiscard]] std::string destination_name() const;
    void request_refresh(const std::shared_ptr<VKBuffer>& buffer) const;
    void register_pipeline(const std::shared_ptr<VKBuffer>& buffer) const;

    std::unordered_map<std::shared_ptr<VKBuffer>, BufferState> m_buffer_info;
    /** @brief Non-owning exceptions to default visibility, removed on detach. */
    std::vector<const VKBuffer*> m_hidden_buffers;
    Registry::Service::DisplayService* m_display_service = nullptr;

    Portal::Graphics::PrimitiveTopology m_primitive_topology { Portal::Graphics::PrimitiveTopology::TRIANGLE_LIST };
    Portal::Graphics::PolygonMode m_polygon_mode { Portal::Graphics::PolygonMode::FILL };
    std::vector<Portal::Graphics::DrawRun> m_mill_fill_runs;
    Portal::Graphics::CullMode m_cull_mode { Portal::Graphics::CullMode::NONE };

    struct TextureBinding {
        std::shared_ptr<Core::VKImage> texture;
        vk::Sampler sampler;
    };
    std::unordered_map<uint32_t, TextureBinding> m_texture_bindings;
    std::unordered_map<uint32_t, std::shared_ptr<Portal::Graphics::ImageTarget>> m_target_bindings;

    /** @brief Bind again every binding whose target now has another image. */
    void follow_target_images();

    /** @brief Bind an image at a binding with the sampler the binding has, forgetting any target it followed. */
    void bind_image(uint32_t binding, const std::shared_ptr<Core::VKImage>& texture);
    std::unordered_map<uint32_t, vk::Sampler> m_binding_samplers;

    std::optional<Portal::Graphics::BlendAttachmentConfig> m_blend_attachment;

    Portal::Graphics::DepthStencilConfig m_depth_stencil;

    bool m_depth_enabled {};

    /**
     * @brief A replaced pipeline kept alive until the frames that may still use it finish
     */
    struct RetiredPipeline {
        Portal::Graphics::RenderPipelineID pipeline;
        std::shared_ptr<Core::Window> window;
        std::shared_ptr<VKBuffer> transform_ubo;
        uint64_t after_frame;
    };
    std::vector<RetiredPipeline> m_retired;
    uint32_t m_binding_wait_ticks {};
    std::set<std::pair<uint32_t, uint32_t>> m_awaited_bindings;

    /**
     * @brief Move the current pipeline to the retired list and drop its buffer associations
     */
    void retire_current_pipeline();

    /**
     * @brief Destroy retired pipelines whose window has completed every frame submitted before retirement
     *
     * Never blocks. Without a completion signal, retired pipelines live until cleanup().
     */
    void collect_retired_pipelines();
    bool m_view_transform_active {};
    uint32_t m_first_vertex { 0 };
    uint32_t m_vertex_count { 0 };
    uint32_t m_instance_count { 1 };

    std::optional<Kinesis::ViewTransform> m_view_transform;
    std::function<Kinesis::ViewTransform()> m_view_transform_source;
    Kinesis::RenderTransform m_published_render_transform {};
    std::optional<glm::mat4> m_geometry_transform;
    std::function<glm::mat4()> m_geometry_transform_source;

    bool m_triangulate {};
    std::optional<Kinesis::Scissor> m_scissor;
    std::vector<Portal::Graphics::DrawRun> m_runs;
    Portal::Graphics::MillSpec m_mill_spec {};

    uint32_t m_last_milled_vertex_count {};
    uint32_t m_mill_passes {};
    bool m_mill_on_change {};

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

    /**
     * @brief Gather the writes for the push descriptor set 0 of the current pipeline.
     *
     * Covers the render transform, the buffer's engine SSBOs and the set 0 textures.
     * The info vectors back the pointers in @p writes and must outlive its use.
     */
    void collect_engine_writes(
        const std::shared_ptr<VKBuffer>& buffer,
        std::vector<vk::WriteDescriptorSet>& writes,
        std::vector<vk::DescriptorBufferInfo>& buffer_infos,
        std::vector<vk::DescriptorImageInfo>& image_infos,
        std::vector<Portal::Graphics::SlotLease>& leases) const;

    BufferState* get_or_cache_buffer_state(const std::shared_ptr<VKBuffer>& buffer);
};

} // namespace MayaFlux::Buffers
