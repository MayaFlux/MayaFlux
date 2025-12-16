#pragma once

#include "MayaFlux/Buffers/Shaders/ShaderProcessor.hpp"

namespace MayaFlux::Nodes {
class Node;
}

namespace MayaFlux::Buffers {

/**
 * @class NodeBindingsProcessor
 * @brief ShaderProcessor with automatic node-to-push-constant binding
 *
 * Extends ShaderProcessor to automatically read node outputs and write them
 * to shader push constants before dispatch. This enables nodes to drive
 * GPU shader parameters in real-time.
 *
 * Example:
 *   struct MyPushConstants {
 *       float brightness;
 *       float frequency;
 *   };
 *
 *   auto processor = std::make_shared<NodeBindingsProcessor>("shader.comp");
 *   processor->set_push_constant_size<MyPushConstants>();
 *
 *   auto brightness = std::make_shared<Sine>(1.0, 0.5);
 *   processor->bind_node("brightness", brightness, offsetof(MyPushConstants, brightness));
 *
 *   // In frame loop
 *   node_manager->process_token(VISUAL_RATE, 1);  // Tick nodes
 *   processor->process(buffer);  // Auto-updates push constants from nodes, then dispatches
 */
class MAYAFLUX_API NodeBindingsProcessor : public ShaderProcessor {
public:
    struct NodeBinding {
        std::shared_ptr<Nodes::Node> node;
        uint32_t push_constant_offset;
        size_t size = sizeof(float);
    };

    using ShaderProcessor::ShaderProcessor;

    /**
     * @brief Bind node output to push constant offset
     * @param name Logical name for this binding
     * @param node Node whose output will be read
     * @param offset Byte offset in push constant struct
     * @param size Size of value (default: sizeof(float))
     */
    void bind_node(
        const std::string& name,
        const std::shared_ptr<Nodes::Node>& node,
        uint32_t offset,
        size_t size = sizeof(float));

    /**
     * @brief Remove node binding
     */
    void unbind_node(const std::string& name);

    /**
     * @brief Check if binding exists
     */
    bool has_binding(const std::string& name) const;

    /**
     * @brief Get all binding names
     */
    std::vector<std::string> get_binding_names() const;

protected:
    /**
     * @brief Hook called before shader dispatch
     * Automatically updates push constants from bound nodes
     */
    void on_before_execute(
        Portal::Graphics::CommandBufferID cmd_id,
        const std::shared_ptr<VKBuffer>& buffer) override;

private:
    void update_push_constants_from_nodes();

    std::unordered_map<std::string, NodeBinding> m_bindings;
};

} // namespace MayaFlux::Buffers
