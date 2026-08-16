#include "RaymarchBuffer.hpp"

#include "MayaFlux/Buffers/BufferProcessingChain.hpp"
#include "MayaFlux/Buffers/Shaders/RenderProcessor.hpp"
#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Kakshya/NDData/VertexFormats.hpp"

namespace MayaFlux::Buffers {

namespace {
    constexpr uint32_t k_box_vertices = 36;

    constexpr std::array<uint32_t, 36> k_box_corners {
        0, 2, 1, 1, 2, 3,
        4, 5, 6, 5, 7, 6,
        0, 1, 4, 1, 5, 4,
        2, 6, 3, 3, 6, 7,
        0, 4, 2, 2, 4, 6,
        1, 3, 5, 3, 7, 5
    };
}

RaymarchBuffer::RaymarchBuffer(
    Kinesis::Lattice3D lattice,
    RaymarchProcessor::FieldSource source,
    size_t field_bytes)
    : VKBuffer(
          k_box_vertices * sizeof(Kakshya::MeshVertex),
          Usage::VERTEX,
          Kakshya::DataModality::VERTICES_3D)
    , m_lattice(lattice)
    , m_source(std::move(source))
    , m_field_bytes(field_bytes)
{
    auto layout = Kakshya::VertexLayout::for_meshes(sizeof(Kakshya::MeshVertex));
    layout.vertex_count = k_box_vertices;
    set_vertex_layout(layout);
}

void RaymarchBuffer::write_box()
{
    const glm::vec3 lo = m_lattice.bounds.min;
    const glm::vec3 hi = m_lattice.bounds.max;

    const std::array<glm::vec3, 8> corners {
        glm::vec3(lo.x, lo.y, lo.z),
        glm::vec3(hi.x, lo.y, lo.z),
        glm::vec3(lo.x, hi.y, lo.z),
        glm::vec3(hi.x, hi.y, lo.z),
        glm::vec3(lo.x, lo.y, hi.z),
        glm::vec3(hi.x, lo.y, hi.z),
        glm::vec3(lo.x, hi.y, hi.z),
        glm::vec3(hi.x, hi.y, hi.z),
    };

    std::vector<Kakshya::MeshVertex> vertices(k_box_vertices);
    for (uint32_t i = 0; i < k_box_vertices; ++i) {
        vertices[i].position = corners[k_box_corners[i]];
    }

    Buffers::upload_to_gpu(
        std::span<const Kakshya::MeshVertex>(vertices),
        std::static_pointer_cast<VKBuffer>(shared_from_this()));
}

void RaymarchBuffer::setup_processors(ProcessingToken token)
{
    auto chain = get_processing_chain();
    if (!chain) {
        chain = std::make_shared<BufferProcessingChain>();
        set_processing_chain(chain);
    }
    chain->set_preferred_token(token);

    m_march_processor = std::make_shared<RaymarchProcessor>(
        m_source, m_field_bytes, m_lattice);
    m_march_processor->set_processing_token(token);

    set_default_processor(m_march_processor);

    write_box();

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::Init,
        "RaymarchBuffer: proxy box over {}x{}x{} lattice",
        m_lattice.resolution.x, m_lattice.resolution.y, m_lattice.resolution.z);
}

void RaymarchBuffer::setup_rendering(const RenderConfig& config)
{
    RenderConfig resolved = config;
    resolved.topology = Portal::Graphics::PrimitiveTopology::TRIANGLE_LIST;

    if (resolved.vertex_shader.empty())
        resolved.vertex_shader = "volume_raymarch.vert.spv";
    if (resolved.fragment_shader.empty())
        resolved.fragment_shader = "volume_raymarch.frag.spv";

    apply_render_config(resolved, ShaderConfig { resolved.vertex_shader });

    m_render_processor->set_cull_mode(Portal::Graphics::CullMode::FRONT);
    m_render_processor->enable_alpha_blending();
    m_render_processor->enable_depth_test();
    m_render_processor->set_vertex_range(0, k_box_vertices);

    get_processing_chain()->add_final_processor(m_render_processor, shared_from_this());

    set_needs_depth_attachment(true);
}

} // namespace MayaFlux::Buffers
