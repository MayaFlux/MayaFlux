#pragma once

#include "MayaFlux/Buffers/VKBuffer.hpp"

namespace MayaFlux::Core {
class VKImage;
}

namespace MayaFlux::Nodes::GpuSync {
class TextureNode;
}

namespace MayaFlux::Buffers {

class NodeTextureBuffer;

/**
 * @class NodeTextureProcessor
 * @brief Uploads TextureNode pixel data to GPU textures via TextureLoom
 *
 * Manages one or more TextureNode → VKImage bindings. Each processing cycle:
 * 1. Checks if node->needs_gpu_update()
 * 2. Retrieves pixel data from node->get_pixel_buffer()
 * 3. Uploads directly to GPU texture via TextureLoom
 * 4. Clears node's dirty flag
 *
 * Philosophy:
 * - Nodes generate pixels (CPU algorithms)
 * - Processor orchestrates upload (delegates to TextureLoom)
 * - Textures are VKImages, not VKBuffers
 * - TextureLoom handles all staging buffer logistics internally
 * - Multiple node→texture bindings share upload infrastructure
 *
 * Usage (single binding - typical):
 *   auto buffer = std::make_shared<NodeTextureBuffer>(node, "noise");
 *   buffer->setup_processors(Graphics);
 *   // Processor automatically created and bound
 *
 * Usage (multiple bindings - advanced):
 *   auto staging = std::make_shared<NodeTextureBuffer>(primary_node);
 *   auto processor = staging->get_texture_processor();
 *
 *   processor->bind_texture_node("secondary", secondary_node, secondary_texture);
 *   processor->bind_texture_node("tertiary", tertiary_node, tertiary_texture);
 *
 *   staging->process_default();  // Uploads all three textures
 */
class MAYAFLUX_API NodeTextureProcessor : public VKBufferProcessor {
public:
    /**
     * @brief Represents a TextureNode → GPU texture binding
     */
    struct TextureBinding {
        std::shared_ptr<Nodes::GpuSync::TextureNode> node; // Generates pixels
        std::shared_ptr<Core::VKImage> gpu_texture; // Target GPU texture
    };

    NodeTextureProcessor();
    ~NodeTextureProcessor() override = default;

    /**
     * @brief Bind a TextureNode to a GPU texture
     * @param name Logical binding name
     * @param node TextureNode to read pixels from
     * @param texture GPU VKImage to upload to
     *
     * TextureLoom handles all staging buffer creation/cleanup internally.
     */
    void bind_texture_node(
        const std::string& name,
        const std::shared_ptr<Nodes::GpuSync::TextureNode>& node,
        const std::shared_ptr<Core::VKImage>& texture);

    /**
     * @brief Remove a texture binding
     * @param name Name of binding to remove
     */
    void unbind_texture_node(const std::string& name);

    /**
     * @brief Check if a binding exists
     */
    [[nodiscard]] bool has_binding(const std::string& name) const
    {
        return m_bindings.contains(name);
    }

    /**
     * @brief Get all binding names
     */
    [[nodiscard]] std::vector<std::string> get_binding_names() const;

    /**
     * @brief Get number of active bindings
     */
    [[nodiscard]] size_t get_binding_count() const
    {
        return m_bindings.size();
    }

    /**
     * @brief Get a specific binding
     * @param name Binding name
     * @return Optional containing binding if it exists
     */
    [[nodiscard]] std::optional<TextureBinding> get_binding(const std::string& name) const;

    /**
     * @brief Process all texture uploads
     * @param buffer The staging buffer (VKBuffer) this processor is attached to
     *
     * Uploads all bound textures that have dirty flags set.
     * Delegates actual upload to TextureLoom which handles staging internally.
     */
    void processing_function(const std::shared_ptr<Buffer>& buffer) override;

    void on_attach(const std::shared_ptr<Buffer>& buffer) override;
    void on_detach(const std::shared_ptr<Buffer>& buffer) override;

private:
    std::unordered_map<std::string, TextureBinding> m_bindings;

    std::shared_ptr<NodeTextureBuffer> m_attached_buffer;

    void initialize_gpu_resources();
};

} // namespace MayaFlux::Buffers
