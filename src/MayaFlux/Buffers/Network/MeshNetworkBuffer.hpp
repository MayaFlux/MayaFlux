#pragma once

#include "MeshNetworkProcessor.hpp"

#include "MayaFlux/Buffers/State/StateFieldBuffer.hpp"

namespace MayaFlux::Core {
class VKImage;
}

namespace MayaFlux::Nodes::Network {
class MeshNetwork;
}

namespace MayaFlux::Buffers {

class RenderProcessor;
class SkinProcessor;

/**
 * @class MeshNetworkBuffer
 * @brief StateFieldBuffer that renders a MeshNetwork as a single indexed draw call.
 *
 * Analogous to MeshBuffer but driven by a MeshNetwork rather than a single
 * MeshData. All slots are concatenated by MeshNetworkProcessor into one
 * combined vertex buffer and one combined index buffer, with per-slot index
 * rebasing so the topology is correct across slot boundaries.
 *
 * Texture support follows MeshBuffer's model exactly:
 * - bind_diffuse_texture() binds one shared diffuse texture for all slots.
 * - RenderConfig::additional_textures carries any further bindings.
 * - A slot's diffuse_texture, set with MeshNetwork::set_slot_texture, is bound for that slot's
 *   draw range, so slots may carry distinct textures.
 *
 * Usage:
 * @code
 * auto net = std::make_shared<MeshNetwork>();
 * // ... add slots ...
 *
 * auto buf = std::make_shared<MeshNetworkBuffer>(net);
 * buf->bind_diffuse_texture(image);
 * buf->setup_processors(ProcessingToken::GRAPHICS_BACKEND);
 * buf->setup_rendering({ .target_window = window });
 * @endcode
 */
class MAYAFLUX_API MeshNetworkBuffer : public StateFieldBuffer {
public:
    /**
     * @brief Construct from an existing MeshNetwork.
     * @param network Must be non-null and have at least one slot before
     *                setup_processors() is called.
     * @param over_allocate_factor Initial buffer size multiplier. Applied to
     *                             the estimated total vertex byte count.
     */
    explicit MeshNetworkBuffer(
        std::shared_ptr<Nodes::Network::MeshNetwork> network,
        float over_allocate_factor = 1.5F);

    ~MeshNetworkBuffer() override;

    /**
     * @brief Create and attach MeshNetworkProcessor as the default processor.
     */
    void setup_processors(ProcessingToken token) override;

    /**
     * @brief Attach a RenderProcessor with optional texture support.
     *
     * Every slot that carries a skin or morph targets (MeshSlot::skin, MeshSlot::morphs) gets a
     * pass ahead of the field pass.
     * Each pass finds its slot's range among the concatenated vertices every cycle, so slots may
     * be added, removed or reordered.
     *
     * Mirrors MeshBuffer::setup_rendering:
     * - Topology is always TRIANGLE_LIST.
     * - Defaults to triangle.vert.spv / triangle.frag.spv (untextured)
     *   or mesh_textured.frag.spv when a diffuse texture is bound.
     * - RenderConfig::additional_textures registers extra texture slots.
     *
     * A GpuFieldOperator in the network's operator chain adds its field pass to
     * this buffer here, once the vertex record size is known, so a network that
     * is never rendered gets no field pass. The pass displaces the vertices in
     * their own slot space; the model matrices still place them. While it has a
     * bound field MeshNetworkProcessor restores the vertices from the nodes every
     * cycle, so the field is applied to the rest geometry and does not
     * accumulate. Only the field pass is wired: SpatialFieldConfig stages (hash,
     * claim, density, population) and cluster scoped fields are not, and each
     * is reported when asked for. The operator's layout must have the size of the
     * vertex record, MeshVertex or, when the draw is milled, MeshVertex and two
     * floats.
     */
    void setup_rendering(const RenderConfig& config);

    // -------------------------------------------------------------------------
    // Texture binding
    // -------------------------------------------------------------------------

    /**
     * @brief Bind a single shared diffuse texture for all slots.
     * @param image       GPU image to bind. Pass nullptr to unbind.
     * @param binding_name Shader descriptor name (default: "diffuseTex").
     *
     * Must be called before setup_rendering() for the ShaderConfig to include
     * the descriptor slot. Calling after setup_rendering() updates the binding
     * on the existing RenderProcessor but will warn if the pipeline was
     * compiled without the binding. The buffer holds the image until it is
     * replaced or the buffer ends.
     */
    void bind_diffuse_texture(
        std::shared_ptr<Core::VKImage> image,
        std::string_view binding_name = "diffuseTex");

    [[nodiscard]] bool has_diffuse_texture() const noexcept
    {
        return m_diffuse_texture != nullptr;
    }

    [[nodiscard]] std::shared_ptr<Core::VKImage> get_diffuse_texture() const noexcept
    {
        return m_diffuse_texture;
    }

    // -------------------------------------------------------------------------
    // Skinning
    // -------------------------------------------------------------------------

    /**
     * @brief Publish the palette of a skinned slot, one matrix for each joint, in the space of the slot's mesh.
     * @return False when the slot has no skin pass wired yet or the count is not its skin's joint count.
     *
     * Safe from any thread (see Kinesis::relative_transforms). A slot that names its joint slots has its
     * palette recomputed from them every cycle, which replaces what is published here.
     */
    bool set_slot_palette(uint32_t slot, std::vector<glm::mat4> palette);

    /**
     * @brief A copy of the palette a skinned slot is deformed with now.
     * @return Empty when the slot has no skin pass wired yet.
     */
    [[nodiscard]] std::vector<glm::mat4> get_slot_palette(uint32_t slot) const;

    // -------------------------------------------------------------------------
    // Accessors
    // -------------------------------------------------------------------------

    [[nodiscard]] std::shared_ptr<Nodes::Network::MeshNetwork> get_network() const
    {
        return m_network;
    }

    [[nodiscard]] std::shared_ptr<MeshNetworkProcessor> get_mesh_processor() const
    {
        return m_processor;
    }

private:
    std::shared_ptr<Nodes::Network::MeshNetwork> m_network;
    std::shared_ptr<MeshNetworkProcessor> m_processor;

    std::shared_ptr<Core::VKImage> m_diffuse_texture;
    std::string m_diffuse_binding { "diffuseTex" };

    RenderConfig m_render_config;

    void wire_field_pass(size_t record_bytes);
    void wire_skin_passes(size_t record_bytes);

    struct SkinEntry {
        uint32_t slot {};
        std::shared_ptr<SkinProcessor> processor;
    };
    std::vector<SkinEntry> m_skins;

    static size_t estimate_vertex_bytes(
        const std::shared_ptr<Nodes::Network::MeshNetwork>& network,
        float over_allocate_factor);
};

} // namespace MayaFlux::Buffers
