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
    enum class ProcessingMode : uint8_t {
        INTERNAL, ///< Processor calls extract_single_sample() - owns the processing
        EXTERNAL ///< Processor reads get_last_output() - node processed elsewhere
    };

    struct NodeBinding {
        std::shared_ptr<Nodes::Node> node;
        uint32_t push_constant_offset;
        size_t size { sizeof(float) };
        std::atomic<ProcessingMode> processing_mode { ProcessingMode::INTERNAL };
    };

    using ShaderProcessor::ShaderProcessor;

    /**
     * @brief Bind node output to push constant offset
     * @param name Logical name for this binding
     * @param node Node whose output will be read
     * @param offset Byte offset in push constant struct
     * @param size Size of value (default: sizeof(float))
     * @param mode Processing mode (default: INTERNAL)
     */
    void bind_node(
        const std::string& name,
        const std::shared_ptr<Nodes::Node>& node,
        uint32_t offset,
        size_t size = sizeof(float),
        ProcessingMode mode = ProcessingMode::INTERNAL);

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

    /**
     * @brief Set processing mode for a specific binding
     * @param name Binding name
     * @param mode INTERNAL (processor extracts samples) or EXTERNAL (processor reads node state)
     */
    void set_processing_mode(const std::string& name, ProcessingMode mode);

    /**
     * @brief Set processing mode for all bindings
     * @param mode INTERNAL or EXTERNAL
     */
    void set_processing_mode(ProcessingMode mode);

    /**
     * @brief Get processing mode for a specific binding
     * @param name Binding name
     * @return Current processing mode
     */
    ProcessingMode get_processing_mode(const std::string& name) const;

protected:
    void execute_shader(const std::shared_ptr<VKBuffer>& buffer) override;

    void initialize_pipeline(const std::shared_ptr<VKBuffer>& buffer) override { }

    void initialize_descriptors(const std::shared_ptr<VKBuffer>& buffer) override { }

private:
    void update_push_constants_from_nodes();

    std::unordered_map<std::string, NodeBinding> m_bindings;
};

} // namespace MayaFlux::Buffers
