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

    void set_render_pass(Portal::Graphics::RenderPassID render_pass_id);
    void set_target_window(std::shared_ptr<Core::Window> window);

    Portal::Graphics::RenderPipelineID get_render_pipeline_id() const { return m_pipeline_id; }

    void on_attach(std::shared_ptr<Buffer> buffer) override;

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

protected:
    void initialize_pipeline(const std::shared_ptr<VKBuffer>& buffer) override;
    void execute_shader(const std::shared_ptr<VKBuffer>& buffer) override;
    void initialize_descriptors(const std::shared_ptr<VKBuffer>& buffer) override;

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
    Portal::Graphics::RenderPassID m_render_pass_id = Portal::Graphics::INVALID_RENDER_PASS;
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
};

} // namespace MayaFlux::Buffers
