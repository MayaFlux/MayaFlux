#pragma once

#include "ShaderProcessor.hpp"

namespace MayaFlux::Nodes {
class Node;
}

namespace MayaFlux::Buffers {

/**
 * @class DescriptorBindingsProcessor
 * @brief ShaderProcessor that uploads node outputs to descriptor sets
 *
 * Binds nodes to UBO/SSBO descriptors. Supports:
 * - Scalar nodes (single value)
 * - Vector nodes (contiguous arrays via VectorContext)
 * - Matrix nodes (2D grids via MatrixContext)
 * - Structured nodes (arrays of POD structs)
 *
 * Usage:
 *   auto processor = std::make_shared<DescriptorBindingsProcessor>(shader_config);
 *
 *   // Bind scalar node to UBO
 *   processor->bind_scalar_node("frequency", freq_node,
 *                                "params", 0, vk::DescriptorType::eUniformBuffer);
 *
 *   // Bind vector node to SSBO
 *   processor->bind_vector_node("spectrum", spectrum_node,
 *                                "spectrum_data", 0, vk::DescriptorType::eStorageBuffer);
 */
class MAYAFLUX_API DescriptorBindingsProcessor : public ShaderProcessor {
public:
    enum class BindingType : uint8_t {
        SCALAR, ///< Single value from node output
        VECTOR, ///< Array from VectorContext
        MATRIX, ///< 2D grid from MatrixContext
        STRUCTURED ///< Array of structs from StructuredContext
    };

    struct DescriptorBinding {
        std::shared_ptr<Nodes::Node> node;
        std::string descriptor_name; ///< Matches ShaderProcessor binding name
        uint32_t set_index;
        uint32_t binding_index;
        vk::DescriptorType type; ///< UBO or SSBO
        BindingType binding_type;
        std::shared_ptr<VKBuffer> gpu_buffer; ///< UBO/SSBO backing storage
        size_t buffer_offset; ///< Offset within buffer (for packed UBOs)
        size_t buffer_size; ///< Size to write
    };

    /**
     * @brief Create DescriptorBindingsProcessor with shader path
     * @param shader_path Path to compute shader
     */
    DescriptorBindingsProcessor(const std::string& shader_path);

    /**
     * @brief Create DescriptorBindingsProcessor with shader config
     * @param config Shader processor configuration
     */
    DescriptorBindingsProcessor(ShaderConfig config);

    /**
     * @brief Bind scalar node output to descriptor
     * @param name Logical binding name
     * @param node Node to read from
     * @param descriptor_name Name in shader config bindings
     * @param set Descriptor set index
     * @param type UBO or SSBO
     */
    void bind_scalar_node(
        const std::string& name,
        const std::shared_ptr<Nodes::Node>& node,
        const std::string& descriptor_name,
        uint32_t set,
        vk::DescriptorType type = vk::DescriptorType::eUniformBuffer);

    /**
     * @brief Bind vector node (VectorContext) to descriptor
     * @param name Logical binding name
     * @param node Node that creates VectorContext
     * @param descriptor_name Name in shader config bindings
     * @param set Descriptor set index
     * @param type Typically eStorageBuffer for arrays
     */
    void bind_vector_node(
        const std::string& name,
        const std::shared_ptr<Nodes::Node>& node,
        const std::string& descriptor_name,
        uint32_t set,
        vk::DescriptorType type = vk::DescriptorType::eStorageBuffer);

    /**
     * @brief Bind matrix node (MatrixContext) to descriptor
     */
    void bind_matrix_node(
        const std::string& name,
        const std::shared_ptr<Nodes::Node>& node,
        const std::string& descriptor_name,
        uint32_t set,
        vk::DescriptorType type = vk::DescriptorType::eStorageBuffer);

    /**
     * @brief Bind structured node (arrays of POD structs) to descriptor
     * @param name Logical binding name
     * @param node Node that creates context with GpuStructuredData
     * @param descriptor_name Name in shader config bindings
     * @param set Descriptor set index
     * @param type Typically eStorageBuffer for structured arrays
     */
    void bind_structured_node(
        const std::string& name,
        const std::shared_ptr<Nodes::Node>& node,
        const std::string& descriptor_name,
        uint32_t set,
        vk::DescriptorType type = vk::DescriptorType::eStorageBuffer);

    /**
     * @brief Remove a binding
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
     * @brief Called before shader dispatch - updates all descriptors
     */
    void on_before_execute(
        Portal::Graphics::CommandBufferID cmd_id,
        const std::shared_ptr<VKBuffer>& buffer) override;

    /**
     * @brief Called after pipeline creation - allocates GPU buffers for descriptors
     */
    void on_pipeline_created(Portal::Graphics::ComputePipelineID pipeline_id) override;

    void execute_shader(const std::shared_ptr<VKBuffer>& buffer) override { }

    void initialize_pipeline(const std::shared_ptr<Buffer>& buffer) override { }

    void initialize_descriptors() override { }

private:
    std::unordered_map<std::string, DescriptorBinding> m_bindings;

    /**
     * @brief Ensure descriptor buffer has sufficient capacity
     * @param binding Descriptor binding to check
     * @param required_size Minimum required size in bytes
     *
     * If buffer is too small, resizes with 50% over-allocation.
     * Sets m_needs_descriptor_rebuild flag if resize occurs.
     */
    void ensure_buffer_capacity(DescriptorBinding& binding, size_t required_size);

    /**
     * @brief Update descriptor from node context
     */
    void update_descriptor_from_node(DescriptorBinding& binding);

    /**
     * @brief Create GPU buffer for a descriptor binding
     */
    std::shared_ptr<VKBuffer> create_descriptor_buffer(
        size_t size,
        vk::DescriptorType type);
};

} // namespace MayaFlux::Buffers
