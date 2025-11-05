#pragma once

#include "MayaFlux/Buffers/VKBuffer.hpp"

namespace MayaFlux::Nodes {
class TextureNode;
} // namespace MayaFlux::Nodes

namespace MayaFlux::Buffers {

/**
 * @class TextureBindingsProcessor
 * @brief BufferProcessor that uploads multiple texture nodes to GPU
 *
 * Manages bindings between TextureNode instances and GPU texture buffers.
 * Each frame, reads pixel data from nodes and uploads to corresponding GPU textures.
 *
 * Behavior:
 * - If ATTACHED buffer is host-visible: uploads all textures to their targets + attached buffer
 * - If ATTACHED buffer is device-local: uploads all textures via staging buffers
 *
 * Usage:
 *   auto texture_buffer = std::make_shared<VKBuffer>(...);
 *   auto processor = std::make_shared<TextureBindingsProcessor>();
 *
 *   processor->bind_texture_node("spectrum", spectrum_node, spectrum_texture);
 *   processor->bind_texture_node("waveform", waveform_node, waveform_texture);
 *
 *   texture_buffer->set_default_processor(processor);
 *   texture_buffer->process_default();  // Uploads all bound textures
 */
class MAYAFLUX_API TextureBindingsProcessor : public VKBufferProcessor {
public:
    struct TextureBinding {
        std::shared_ptr<Nodes::TextureNode> node;
        std::shared_ptr<VKBuffer> gpu_texture; // Target texture buffer
        std::shared_ptr<VKBuffer> staging_buffer; // Staging (only if gpu_texture is device-local)
    };

    /**
     * @brief Bind a texture node to a GPU texture buffer
     * @param name Logical name for this binding
     * @param node TextureNode to read pixels from
     * @param texture GPU texture buffer to upload to
     *
     * If texture is device-local, a staging buffer is automatically created.
     * If texture is host-visible, no staging is needed.
     */
    void bind_texture_node(
        const std::string& name,
        const std::shared_ptr<Nodes::TextureNode>& node,
        const std::shared_ptr<VKBuffer>& texture);

    /**
     * @brief Remove a texture binding
     * @param name Name of binding to remove
     */
    void unbind_texture_node(const std::string& name);

    /**
     * @brief Check if a binding exists
     * @param name Binding name
     * @return True if binding exists
     */
    bool has_binding(const std::string& name) const { return m_bindings.contains(name); }

    /**
     * @brief Get all binding names
     * @return Vector of binding names
     */
    std::vector<std::string> get_binding_names() const;

    /**
     * @brief Get number of active bindings
     * @return Binding count
     */
    size_t get_binding_count() const { return m_bindings.size(); }

    void processing_function(std::shared_ptr<Buffer> buffer) override;

private:
    std::unordered_map<std::string, TextureBinding> m_bindings;
};
} // namespace MayaFlux::Buffers
