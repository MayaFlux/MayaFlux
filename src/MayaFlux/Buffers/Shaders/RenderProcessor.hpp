#pragma once

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

protected:
    void initialize_pipeline(const std::shared_ptr<VKBuffer>& buffer) override;
    void execute_shader(const std::shared_ptr<VKBuffer>& buffer) override;
    void initialize_descriptors(const std::shared_ptr<VKBuffer>& buffer) override;

    bool on_before_execute(Portal::Graphics::CommandBufferID cmd_id, const std::shared_ptr<VKBuffer>& buffer) override;

    void cleanup() override;

private:
    struct VertexInfo {
        Kakshya::VertexLayout semantic_layout;
        bool use_reflection {};
    };

    Portal::Graphics::RenderPipelineID m_pipeline_id = Portal::Graphics::INVALID_RENDER_PIPELINE;
    Portal::Graphics::ShaderID m_geometry_shader_id = Portal::Graphics::INVALID_SHADER;
    Portal::Graphics::ShaderID m_tess_control_shader_id = Portal::Graphics::INVALID_SHADER;
    Portal::Graphics::ShaderID m_tess_eval_shader_id = Portal::Graphics::INVALID_SHADER;
    Portal::Graphics::ShaderID m_fragment_shader_id = Portal::Graphics::INVALID_SHADER;
    std::shared_ptr<Core::Window> m_target_window;

    std::unordered_map<std::shared_ptr<VKBuffer>, VertexInfo> m_buffer_info;
    Registry::Service::DisplayService* m_display_service = nullptr;

    Portal::Graphics::PrimitiveTopology m_primitive_topology { Portal::Graphics::PrimitiveTopology::TRIANGLE_LIST };
    Portal::Graphics::PolygonMode m_polygon_mode { Portal::Graphics::PolygonMode::FILL };
    Portal::Graphics::CullMode m_cull_mode { Portal::Graphics::CullMode::NONE };

    struct TextureBinding {
        std::shared_ptr<Core::VKImage> texture;
        vk::Sampler sampler;
    };
    std::unordered_map<uint32_t, TextureBinding> m_texture_bindings;

    uint32_t m_first_vertex { 0 };
    uint32_t m_vertex_count { 0 };

    const Kakshya::VertexLayout* get_or_cache_vertex_layout(
        std::unordered_map<std::shared_ptr<VKBuffer>, VertexInfo>& buffer_info,
        const std::shared_ptr<VKBuffer>& buffer);
};

} // namespace MayaFlux::Buffers
