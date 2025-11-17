#pragma once

#include "MayaFlux/Buffers/Shaders/DescriptorBindingsProcessor.hpp"
#include "MayaFlux/Buffers/VKBuffer.hpp"

namespace MayaFlux::Buffers {

/**
 * @class DescriptorBuffer
 * @brief Specialized buffer for shader parameter bindings from nodes
 *
 * Binds node outputs to shader uniforms and storage buffers.
 * Designed for parameterizing shaders with live data: time, frequencies,
 * control values, arrays, matrices, etc.
 *
 * Philosophy:
 * - Shaders are controlled by DATA, not hardcoded values
 * - Any node can drive any shader parameter
 * - Cross-domain flow: audio nodes → visual shader parameters
 *
 * Usage:
 *   ShaderProcessorConfig config;
 *   config.shader_path = "parametric.comp";
 *   config.bindings = {
 *       {"time", {.binding = 0, .type = vk::DescriptorType::eUniformBuffer}},
 *       {"spectrum", {.binding = 1, .type = vk::DescriptorType::eStorageBuffer}}
 *   };
 *
 *   auto buffer = std::make_shared<DescriptorBuffer>(config);
 *   buffer->bind_scalar("time", time_node, "time");
 *   buffer->bind_vector("spectrum", fft_node, "spectrum");
 *
 *   auto compute = std::make_shared<ComputeProcessor>(config);
 *   buffer->add_processor(compute) | Graphics;
 */
class MAYAFLUX_API DescriptorBuffer : public VKBuffer {
public:
    /**
     * @brief Create descriptor buffer with shader configuration
     * @param config Shader processor configuration with binding definitions
     * @param initial_size Initial buffer size (will grow as needed)
     */
    explicit DescriptorBuffer(
        const ShaderProcessorConfig& config,
        size_t initial_size = 4096);

    ~DescriptorBuffer() override = default;

    /**
     * @brief Initialize the buffer and its processors
     */
    void initialize();

    /**
     * @brief Bind scalar node output to uniform/SSBO
     * @param name Logical binding name
     * @param node Node providing scalar value
     * @param descriptor_name Name in shader config bindings
     * @param set Descriptor set index (default: 0)
     */
    void bind_scalar(
        const std::string& name,
        const std::shared_ptr<Nodes::Node>& node,
        const std::string& descriptor_name,
        uint32_t set = 0);

    /**
     * @brief Bind vector node output to SSBO
     * @param name Logical binding name
     * @param node Node providing vector data (via VectorContext)
     * @param descriptor_name Name in shader config bindings
     * @param set Descriptor set index (default: 0)
     */
    void bind_vector(
        const std::string& name,
        const std::shared_ptr<Nodes::Node>& node,
        const std::string& descriptor_name,
        uint32_t set = 0);

    /**
     * @brief Bind matrix node output to SSBO
     * @param name Logical binding name
     * @param node Node providing matrix data (via MatrixContext)
     * @param descriptor_name Name in shader config bindings
     * @param set Descriptor set index (default: 0)
     */
    void bind_matrix(
        const std::string& name,
        const std::shared_ptr<Nodes::Node>& node,
        const std::string& descriptor_name,
        uint32_t set = 0);

    /**
     * @brief Bind structured node output to SSBO
     * @param name Logical binding name
     * @param node Node providing structured data
     * @param descriptor_name Name in shader config bindings
     * @param set Descriptor set index (default: 0)
     */
    void bind_structured(
        const std::string& name,
        const std::shared_ptr<Nodes::Node>& node,
        const std::string& descriptor_name,
        uint32_t set = 0);

    /**
     * @brief Remove a binding
     */
    void unbind(const std::string& name);

    /**
     * @brief Get the bindings processor
     */
    [[nodiscard]] std::shared_ptr<DescriptorBindingsProcessor> get_bindings_processor() const
    {
        return m_bindings_processor;
    }

    /**
     * @brief Get all binding names
     */
    [[nodiscard]] std::vector<std::string> get_binding_names() const;

private:
    std::shared_ptr<DescriptorBindingsProcessor> m_bindings_processor;
    ShaderProcessorConfig m_config;
};

} // namespace MayaFlux::Buffers
