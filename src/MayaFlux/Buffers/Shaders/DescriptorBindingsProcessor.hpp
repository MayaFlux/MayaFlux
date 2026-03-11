#pragma once

#include "ShaderProcessor.hpp"

namespace MayaFlux::Nodes {
class Node;
}

namespace MayaFlux::Buffers {

class AudioBuffer;

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
 *                                "params", 0, Portal::Graphics::DescriptorRole::UNIFORM);
 *
 *   // Bind vector node to SSBO
 *   processor->bind_vector_node("spectrum", spectrum_node,
 *                                "spectrum_data", 0, Portal::Graphics::DescriptorRole::STORAGE);
 */
class MAYAFLUX_API DescriptorBindingsProcessor : public ShaderProcessor {
public:
    enum class ProcessingMode : uint8_t {
        INTERNAL, ///< Processor calls extract_single_sample() or processes node context
        EXTERNAL ///< Processor reads node's current state (get_last_output/get_last_context)
    };

    enum class BindingType : uint8_t {
        SCALAR, ///< Single value from node output
        VECTOR, ///< Array from VectorContext
        MATRIX, ///< 2D grid from MatrixContext
        STRUCTURED ///< Array of structs from StructuredContext
    };

    enum class SourceType : uint8_t {
        NODE,
        AUDIO_BUFFER,
        HOST_VK_BUFFER,
        NETWORK_AUDIO,
        NETWORK_GPU
    };

    struct DescriptorBinding {
        std::shared_ptr<Nodes::Node> node;
        std::shared_ptr<Buffers::Buffer> buffer;
        std::shared_ptr<Nodes::Network::NodeNetwork> network;
        std::string descriptor_name; ///< Matches ShaderProcessor binding name
        uint32_t set_index;
        uint32_t binding_index;
        Portal::Graphics::DescriptorRole role; ///< UBO or SSBO
        BindingType binding_type;
        SourceType source_type { SourceType::NODE };
        std::shared_ptr<VKBuffer> gpu_buffer; ///< UBO/SSBO backing storage
        size_t buffer_offset {}; ///< Offset within buffer (for packed UBOs)
        size_t buffer_size; ///< Size to write
        std::atomic<ProcessingMode> processing_mode { ProcessingMode::INTERNAL };
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
     * @param role UBO or SSBO
     * @param mode Processing mode (default: INTERNAL)
     */
    void bind_scalar_node(
        const std::string& name,
        const std::shared_ptr<Nodes::Node>& node,
        const std::string& descriptor_name,
        uint32_t set,
        Portal::Graphics::DescriptorRole role = Portal::Graphics::DescriptorRole::UNIFORM,
        ProcessingMode mode = ProcessingMode::INTERNAL);

    /**
     * @brief Bind vector node (VectorContext) to descriptor
     * @param name Logical binding name
     * @param node Node that creates VectorContext
     * @param descriptor_name Name in shader config bindings
     * @param set Descriptor set index
     * @param role Typically STORAGE for arrays
     * @param mode Processing mode (default: INTERNAL)
     */
    void bind_vector_node(
        const std::string& name,
        const std::shared_ptr<Nodes::Node>& node,
        const std::string& descriptor_name,
        uint32_t set,
        Portal::Graphics::DescriptorRole role = Portal::Graphics::DescriptorRole::STORAGE,
        ProcessingMode mode = ProcessingMode::INTERNAL);

    /**
     * @brief Bind matrix node (MatrixContext) to descriptor
     */
    void bind_matrix_node(
        const std::string& name,
        const std::shared_ptr<Nodes::Node>& node,
        const std::string& descriptor_name,
        uint32_t set,
        Portal::Graphics::DescriptorRole role = Portal::Graphics::DescriptorRole::STORAGE,
        ProcessingMode mode = ProcessingMode::INTERNAL);

    /**
     * @brief Bind structured node (arrays of POD structs) to descriptor
     * @param name Logical binding name
     * @param node Node that creates context with GpuStructuredData
     * @param descriptor_name Name in shader config bindings
     * @param set Descriptor set index
     * @param role Typically STORAGE for structured arrays
     */
    void bind_structured_node(
        const std::string& name,
        const std::shared_ptr<Nodes::Node>& node,
        const std::string& descriptor_name,
        uint32_t set,
        Portal::Graphics::DescriptorRole role = Portal::Graphics::DescriptorRole::STORAGE,
        ProcessingMode mode = ProcessingMode::INTERNAL);

    /**
     * @brief Bind an AudioBuffer as a descriptor source.
     *
     * Reads the buffer's double sample data each cycle, converts to float,
     * and uploads to the descriptor. Always treated as VECTOR binding.
     *
     * @param name        Logical binding name
     * @param buffer      AudioBuffer to read from
     * @param descriptor_name Name in shader config bindings
     * @param set         Descriptor set index
     * @param role        Descriptor role (default: STORAGE)
     */
    void bind_audio_buffer(
        const std::string& name,
        const std::shared_ptr<AudioBuffer>& buffer,
        const std::string& descriptor_name,
        uint32_t set,
        Portal::Graphics::DescriptorRole role = Portal::Graphics::DescriptorRole::STORAGE);

    /**
     * @brief Bind a host-visible VKBuffer as a descriptor source.
     *
     * Fails hard at bind time if the buffer is not host-visible.
     * Reads mapped memory each cycle and uploads to the descriptor.
     * Always treated as VECTOR binding.
     *
     * @param name        Logical binding name
     * @param buffer      Host-visible VKBuffer to read from
     * @param descriptor_name Name in shader config bindings
     * @param set         Descriptor set index
     * @param role        Descriptor role (default: STORAGE)
     */
    void bind_host_vk_buffer(
        const std::string& name,
        const std::shared_ptr<VKBuffer>& buffer,
        const std::string& descriptor_name,
        uint32_t set,
        Portal::Graphics::DescriptorRole role = Portal::Graphics::DescriptorRole::STORAGE);

    /**
     * @brief Bind a NodeNetwork to a descriptor.
     *
     * Resolves source type at bind time:
     * - Networks with get_audio_buffer() output → NETWORK_AUDIO (VECTOR binding)
     * - Networks with a GraphicsOperator         → NETWORK_GPU   (STRUCTURED binding)
     *
     * Fails hard if the network satisfies neither condition.
     *
     * @param name            Logical binding name
     * @param network         NodeNetwork to read from
     * @param descriptor_name Name in shader config bindings
     * @param set             Descriptor set index
     * @param role            Descriptor role (default: STORAGE)
     */
    void bind_network(
        const std::string& name,
        const std::shared_ptr<Nodes::Network::NodeNetwork>& network,
        const std::string& descriptor_name,
        uint32_t set,
        Portal::Graphics::DescriptorRole role = Portal::Graphics::DescriptorRole::STORAGE);

    /**
     * @brief Remove a binding
     */
    void unbind(const std::string& name);

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
     * @param mode INTERNAL (processor processes node) or EXTERNAL (processor reads node state)
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
    /**
     * @brief Called after pipeline creation - allocates GPU buffers for descriptors
     */
    void on_pipeline_created(Portal::Graphics::ComputePipelineID pipeline_id) override;

    void execute_shader(const std::shared_ptr<VKBuffer>& buffer) override;

    void initialize_pipeline(const std::shared_ptr<VKBuffer>& buffer) override { }

    void initialize_descriptors(const std::shared_ptr<VKBuffer>& buffer) override { }

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
        Portal::Graphics::DescriptorRole role);
};

} // namespace MayaFlux::Buffers
