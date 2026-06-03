#include "ComputeMeshBuffer.hpp"

#include "MayaFlux/Buffers/BufferProcessingChain.hpp"
#include "MayaFlux/Buffers/Shaders/RenderProcessor.hpp"
#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Buffers {

// ============================================================================
// Construction
// ============================================================================

ComputeMeshBuffer::ComputeMeshBuffer(
    Kinesis::SpatialField field,
    const glm::vec3& bounds_min,
    const glm::vec3& bounds_max,
    uint32_t res_x,
    uint32_t res_y,
    uint32_t res_z,
    float iso_level)
    : VKBuffer(
          worst_case_bytes(res_x, res_y, res_z),
          Usage::VERTEX,
          Kakshya::DataModality::VERTEX_POSITIONS_3D)
    , m_res_x(std::max(res_x, 1U))
    , m_res_y(std::max(res_y, 1U))
    , m_res_z(std::max(res_z, 1U))
    , m_field(std::move(field))
    , m_bounds_min(bounds_min)
    , m_bounds_max(bounds_max)
    , m_iso_level(iso_level)
{
    MF_INFO(Journal::Component::Buffers, Journal::Context::Init,
        "ComputeMeshBuffer: {}x{}x{} grid, {:.1f} MB worst-case",
        m_res_x, m_res_y, m_res_z,
        static_cast<float>(get_size_bytes()) / (1024.F * 1024.F));
}

// ============================================================================
// Processor setup
// ============================================================================

void ComputeMeshBuffer::setup_processors(ProcessingToken token)
{
    auto self = std::dynamic_pointer_cast<ComputeMeshBuffer>(shared_from_this());

    auto layout = Kakshya::VertexLayout::for_meshes(sizeof(Kakshya::MeshVertex));
    layout.vertex_count = static_cast<uint32_t>(worst_case_bytes(m_res_x, m_res_y, m_res_z)
        / sizeof(Kakshya::MeshVertex));
    set_vertex_layout(layout);

    m_sdf_processor = std::make_shared<SDFMeshProcessor>(
        std::move(m_field),
        m_bounds_min,
        m_bounds_max,
        m_res_x,
        m_res_y,
        m_res_z,
        m_iso_level);

    m_sdf_processor->set_processing_token(token);
    set_default_processor(m_sdf_processor);

    auto chain = get_processing_chain();
    if (!chain) {
        chain = std::make_shared<BufferProcessingChain>();
        set_processing_chain(chain);
    }
    chain->set_preferred_token(token);
}

void ComputeMeshBuffer::setup_rendering(const RenderConfig& config)
{
    const bool textured = m_diffuse_texture != nullptr
        || !config.default_texture_binding.empty();

    RenderConfig resolved = config;
    resolved.topology = Portal::Graphics::PrimitiveTopology::TRIANGLE_LIST;

    if (resolved.vertex_shader.empty())
        resolved.vertex_shader = "triangle.vert.spv";

    if (resolved.fragment_shader.empty()) {
        resolved.fragment_shader = textured
            ? "mesh_textured.frag.spv"
            : "triangle.frag.spv";
    }

    ShaderConfig sc { resolved.vertex_shader };

    if (textured) {
        const std::string slot = resolved.default_texture_binding.empty()
            ? m_diffuse_binding
            : resolved.default_texture_binding;
        sc.bindings[slot] = ShaderBinding(0, 1, vk::DescriptorType::eCombinedImageSampler);
    }

    apply_render_config(resolved, sc);

    if (m_diffuse_texture)
        m_render_processor->bind_texture(m_diffuse_binding, m_diffuse_texture);

    get_processing_chain()->add_final_processor(
        m_render_processor, shared_from_this());

    MF_INFO(Journal::Component::Buffers, Journal::Context::Init,
        "ComputeMeshBuffer: rendering configured ({}textured)",
        textured ? "" : "un");
}

// ============================================================================
// Field and geometry control
// ============================================================================

void ComputeMeshBuffer::set_field(Kinesis::SpatialField field)
{
    if (m_sdf_processor) {
        m_sdf_processor->set_field(std::move(field));
    } else {
        m_field = std::move(field);
    }
}

void ComputeMeshBuffer::set_bounds(const glm::vec3& bounds_min, const glm::vec3& bounds_max)
{
    if (m_sdf_processor) {
        m_sdf_processor->set_bounds(bounds_min, bounds_max);
    } else {
        m_bounds_min = bounds_min;
        m_bounds_max = bounds_max;
    }
}

void ComputeMeshBuffer::set_resolution(uint32_t res_x, uint32_t res_y, uint32_t res_z)
{
    m_res_x = std::max(res_x, 1U);
    m_res_y = std::max(res_y, 1U);
    m_res_z = std::max(res_z, 1U);
    if (m_sdf_processor)
        m_sdf_processor->set_resolution(m_res_x, m_res_y, m_res_z);
}

void ComputeMeshBuffer::set_iso_level(float iso_level)
{
    m_iso_level = iso_level;
    if (m_sdf_processor)
        m_sdf_processor->set_iso_level(iso_level);
}

void ComputeMeshBuffer::set_dirty()
{
    if (m_sdf_processor)
        m_sdf_processor->set_dirty();
}

// ============================================================================
// Texture
// ============================================================================

void ComputeMeshBuffer::set_texture(std::shared_ptr<Core::VKImage> image, std::string binding)
{
    m_diffuse_texture = std::move(image);
    m_diffuse_binding = std::move(binding);

    if (m_render_processor && m_diffuse_texture)
        m_render_processor->bind_texture(m_diffuse_binding, m_diffuse_texture);
}

} // namespace MayaFlux::Buffers
