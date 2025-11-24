#pragma once

#include "GeometryBindingsProcessor.hpp"
#include "MayaFlux/Buffers/VKBuffer.hpp"
#include "MayaFlux/Nodes/Graphics/GeometryWriterNode.hpp"

#include "MayaFlux/Portal/Graphics/GraphicsUtils.hpp"

namespace MayaFlux::Buffers {

class RenderProcessor;

/**
 * @class GeometryBuffer
 * @brief Specialized buffer for generative geometry from GeometryWriterNode
 *
 * Automatically handles CPU→GPU upload of procedurally generated vertices.
 * Designed for algorithmic geometry generation: particles, simulations,
 * procedural meshes, data visualizations, etc.
 *
 * Philosophy:
 * - Geometry is GENERATED, not loaded from files
 * - Data flows from algorithm → GPU → screen
 * - No primitive worship - users create their own forms
 *
 * Usage:
 *   class ParticleSystem : public GeometryWriterNode {
 *       void compute_frame() override {
 *           // Generate 1000 particle positions algorithmically
 *           for (int i = 0; i < 1000; i++) {
 *               positions[i] = simulate_physics(i);
 *           }
 *           update_vertex_buffer(positions);
 *       }
 *   };
 *
 *   auto particles = std::make_shared<ParticleSystem>(1000);
 *   auto buffer = std::make_shared<GeometryBuffer>(particles);
 *
 *   auto render = std::make_shared<RenderProcessor>(config);
 *   render->set_fragment_shader("particle.frag");
 *   render->set_target_window(window);
 *
 *   buffer->add_processor(render) | Graphics;
 */
class MAYAFLUX_API GeometryBuffer : public VKBuffer {
public:
    struct RenderConfig {
        std::shared_ptr<Core::Window> target_window;
        std::string vertex_shader = "point.vert.spv";
        std::string fragment_shader = "point.frag.spv";
        Portal::Graphics::PrimitiveTopology topology = Portal::Graphics::PrimitiveTopology::POINT_LIST;
        Portal::Graphics::PolygonMode polygon_mode = Portal::Graphics::PolygonMode::FILL;
        Portal::Graphics::CullMode cull_mode = Portal::Graphics::CullMode::NONE;
    };

    /**
     * @brief Create geometry buffer from generative node
     * @param node GeometryWriterNode that generates vertices each frame
     * @param binding_name Logical name for this geometry binding (default: "geometry")
     * @param over_allocate_factor Buffer size multiplier for dynamic growth (default: 1.5x)
     *
     * Buffer size is initially set to node->get_vertex_buffer_size_bytes().
     * If over_allocate_factor > 1.0, buffer will be larger to accommodate growth
     * without reallocation.
     */
    explicit GeometryBuffer(
        std::shared_ptr<Nodes::GpuSync::GeometryWriterNode> node,
        const std::string& binding_name = "geometry",
        float over_allocate_factor = 1.5f);

    ~GeometryBuffer() override = default;

    /**
     * @brief Initialize the buffer and its processors
     */
    void setup_processors(ProcessingToken token) override;

    /**
     * @brief Get the geometry node driving this buffer
     */
    [[nodiscard]] std::shared_ptr<Nodes::GpuSync::GeometryWriterNode> get_geometry_node() const
    {
        return m_geometry_node;
    }

    /**
     * @brief Get the bindings processor managing uploads
     */
    [[nodiscard]] std::shared_ptr<GeometryBindingsProcessor> get_bindings_processor() const
    {
        return m_bindings_processor;
    }

    /**
     * @brief Get the logical binding name
     */
    [[nodiscard]] const std::string& get_binding_name() const
    {
        return m_binding_name;
    }

    /**
     * @brief Get current vertex count from node
     */
    [[nodiscard]] uint32_t get_vertex_count() const
    {
        return m_geometry_node ? m_geometry_node->get_vertex_count() : 0;
    }

    /**
     * @brief Trigger vertex computation on the node
     *
     * Calls node->compute_frame() to regenerate geometry.
     * Useful for explicit frame updates when not using domain-driven processing.
     */
    void update_geometry()
    {
        if (m_geometry_node) {
            m_geometry_node->compute_frame();
        }
    }

    /**
     * @brief Setup rendering with RenderProcessor
     * @param config Rendering configuration
     */
    void setup_rendering(const RenderConfig& config);

    std::shared_ptr<RenderProcessor> get_render_processor() const
    {
        return m_render_processor;
    }

private:
    std::shared_ptr<Nodes::GpuSync::GeometryWriterNode> m_geometry_node;
    std::shared_ptr<GeometryBindingsProcessor> m_bindings_processor;
    std::string m_binding_name;

    std::shared_ptr<RenderProcessor> m_render_processor;

    /**
     * @brief Calculate initial buffer size with optional over-allocation
     */
    static size_t calculate_buffer_size(
        const std::shared_ptr<Nodes::GpuSync::GeometryWriterNode>& node,
        float over_allocate_factor);
};

} // namespace MayaFlux::Buffers
