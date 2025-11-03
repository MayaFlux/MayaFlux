#pragma once

#include "MayaFlux/Portal/Graphics/RenderFlow.hpp"
#include "ShaderProcessor.hpp"

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
    RenderProcessor(const ShaderProcessorConfig& config);

    void set_geometry_shader(const std::string& geometry_path);
    void set_tess_control_shader(const std::string& tess_control_path);
    void set_tess_eval_shader(const std::string& tess_eval_path);
    void set_fragment_shader(const std::string& fragment_path);

    void set_render_pass(Portal::Graphics::RenderPassID render_pass_id);
    void set_target_window(std::shared_ptr<Core::Window> window);

    Portal::Graphics::RenderPipelineID get_render_pipeline_id() const { return m_render_pipeline_id; }

    void on_attach(std::shared_ptr<Buffer> buffer) override;

protected:
    void initialize_pipeline(const std::shared_ptr<Buffer>& buffer) override;
    void processing_function(std::shared_ptr<Buffer> buffer) override;

private:
    struct VertexInfo {
        Kakshya::VertexLayout semantic_layout;
        bool use_reflection {};
    };

    Portal::Graphics::RenderPipelineID m_render_pipeline_id = Portal::Graphics::INVALID_RENDER_PIPELINE;
    Portal::Graphics::ShaderID m_geometry_shader_id = Portal::Graphics::INVALID_SHADER;
    Portal::Graphics::ShaderID m_tess_control_shader_id = Portal::Graphics::INVALID_SHADER;
    Portal::Graphics::ShaderID m_tess_eval_shader_id = Portal::Graphics::INVALID_SHADER;
    Portal::Graphics::ShaderID m_fragment_shader_id = Portal::Graphics::INVALID_SHADER;
    Portal::Graphics::RenderPassID m_render_pass_id = Portal::Graphics::INVALID_RENDER_PASS;
    std::shared_ptr<Core::Window> m_target_window;

    std::unordered_map<std::shared_ptr<VKBuffer>, VertexInfo> m_buffer_info;
    Registry::Service::DisplayService* m_display_service = nullptr;
};

} // namespace MayaFlux::Buffers
