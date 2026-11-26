#pragma once

#include "AssembledGeometryProcessor.hpp"

#include "MayaFlux/Buffers/State/StateFieldBuffer.hpp"

namespace MayaFlux::Buffers {

class RenderProcessor;

/**
 * @class AssembledGeometryBuffer
 * @brief Draws an AssemblyNetwork: points, lines and indexed triangle meshes in one buffer.
 *
 * A GpuFieldOperator in the network's operator chain adds its field pass to this
 * buffer. The spatial stages and cluster scoped fields are not wired. The draw is
 * always milled, so items of any topology may be added while it runs.
 *
 * @code
 * auto network = std::make_shared<Nodes::Network::AssemblyNetwork>();
 * network->assembly()->add_geometry(mesh_data);
 *
 * auto buffer = std::make_shared<AssembledGeometryBuffer>(network);
 * buffer->setup_processors(ProcessingToken::GRAPHICS_BACKEND);
 * buffer->setup_rendering({ .target_window = window });
 * @endcode
 */
class MAYAFLUX_API AssembledGeometryBuffer : public StateFieldBuffer {
public:
    /**
     * @param network Must be non-null.
     * @param over_allocate_factor Multiplier on the initial vertex byte count.
     */
    explicit AssembledGeometryBuffer(
        const std::shared_ptr<Nodes::Network::AssemblyNetwork>& network,
        float over_allocate_factor = 1.5F);

    ~AssembledGeometryBuffer() override = default;

    void setup_processors(ProcessingToken token) override;

    void setup_rendering(const RenderConfig& config);

    /// @brief Set the spans to draw. Triangle spans count indices when the buffer is indexed.
    void update_draw(std::vector<Portal::Graphics::DrawRun> runs);

    [[nodiscard]] std::shared_ptr<Nodes::Network::AssemblyNetwork> get_network() const
    {
        return m_network;
    }

private:
    std::shared_ptr<Nodes::Network::AssemblyNetwork> m_network;
    std::shared_ptr<AssembledGeometryProcessor> m_processor;

    std::vector<Portal::Graphics::DrawRun> m_runs;
    std::vector<Portal::Graphics::DrawRun> m_applied_runs;

    void wire_field_pass();

    static size_t estimate_vertex_bytes(
        const std::shared_ptr<Nodes::Network::AssemblyNetwork>& network,
        float over_allocate_factor);
};

} // namespace MayaFlux::Buffers
