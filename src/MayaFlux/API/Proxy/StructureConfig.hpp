#pragma once

#include "MayaFlux/Buffers/Geometry/ComputeMeshBuffer.hpp"
#include "MayaFlux/Buffers/VKBuffer.hpp"
#include "MayaFlux/Kakshya/NDData/MeshData.hpp"
#include "MayaFlux/Kakshya/NDData/VertexFormats.hpp"
#include "MayaFlux/Kinesis/Tendency/Tendency.hpp"
#include "MayaFlux/Nodes/Network/AssemblyNetwork.hpp"
#include "MayaFlux/Portal/Graphics/PrimitiveMill.hpp"

namespace MayaFlux {

namespace Core {
    class VKImage;
}

namespace Nodes::GpuSync {
class MeshWriterNode;
}

namespace Nodes::Network {
class InstanceNetwork;
class MeshNetwork;
}

/**
 * @struct StructureConfig
 * @brief API-side playground for a complete visible structure.
 *
 * A StructureConfig accepts the concrete forms already present in the API,
 * plus the required rendering destination. Exactly one source is selected by
 * the eventual Creator consumer.
 *
 * This type is not a library contract. It is the API experiment from which
 * repeated, concrete arrangements may later earn class-level configuration.
 */
struct StructureConfig {
    /** @brief Render target and pipeline overrides. target_window is required. */
    using RenderConfig = Buffers::VKBuffer::RenderConfig;

    /** @brief A one-shot typed mesh expression, normally invoking Kinesis. */
    using MeshExpression = std::function<Kakshya::MeshData()>;

    /**
     * @struct Object
     * @brief One independently rendered mesh-backed object.
     */
    struct Object {
        /** @brief Fixed materialised source. Exclusive with expression and writer. */
        std::optional<Kakshya::MeshData> mesh;
        /** @brief Fixed construction expression. Exclusive with mesh and writer. */
        MeshExpression expression;
        /** @brief Live geometry source. Exclusive with mesh and expression. */
        std::shared_ptr<Nodes::GpuSync::MeshWriterNode> writer;
        /** @brief Images available to the selected render pipeline. */
        std::vector<std::shared_ptr<Core::VKImage>> textures;
        /** @brief Required render destination and pipeline overrides. */
        RenderConfig render;
    };

    /**
     * @struct ModelComponent
     * @brief One local mesh in a hierarchical model.
     */
    struct ModelComponent {
        /** @brief Fixed materialised source. Exclusive with expression and writer. */
        std::optional<Kakshya::MeshData> mesh;
        /** @brief Fixed construction expression. Exclusive with mesh and writer. */
        MeshExpression expression;
        /** @brief Live geometry source. Exclusive with mesh and expression. */
        std::shared_ptr<Nodes::GpuSync::MeshWriterNode> writer;
        /** @brief Fixed local transform within the hierarchy. */
        glm::mat4 local_transform { 1.0F };
        /** @brief Parent index within Model::components, or no local parent. */
        std::optional<uint32_t> parent_component;
        /** @brief Parent slot index in an existing Model::network, or no existing parent. */
        std::optional<uint32_t> parent_slot;
    };

    /**
     * @struct Model
     * @brief Mesh components and allocation policy for one MeshNetworkBuffer.
     */
    struct Model {
        /** @brief Existing network to complete, or null to construct one. */
        std::shared_ptr<Nodes::Network::MeshNetwork> network;
        /** @brief Components appended before registration. */
        std::vector<ModelComponent> components;
        /** @brief Fixed initial vertex-buffer capacity multiplier. */
        float over_allocate_factor { 1.5F };
        /** @brief Images available to the selected render pipeline. */
        std::vector<std::shared_ptr<Core::VKImage>> textures;
        /** @brief Required render destination and pipeline overrides. */
        RenderConfig render;
    };

    /**
     * @struct Instances
     * @brief Prototype mesh and world transforms for one InstanceNetworkBuffer.
     */
    struct Instances {
        /** @brief Existing network to complete, or null to construct one. */
        std::shared_ptr<Nodes::Network::InstanceNetwork> network;
        /** @brief Fixed prototype source. Exclusive with prototype_expression and prototype_writer. */
        std::optional<Kakshya::MeshData> prototype_mesh;
        /** @brief Fixed prototype expression. Exclusive with prototype_mesh and prototype_writer. */
        MeshExpression prototype_expression;
        /** @brief Live prototype source. Exclusive with prototype_mesh and prototype_expression. */
        std::shared_ptr<Nodes::GpuSync::MeshWriterNode> prototype_writer;
        /** @brief Fixed initial world transform for each new instance. */
        std::vector<glm::mat4> transforms;
        /** @brief Fixed initial vertex-buffer capacity multiplier. */
        float over_allocate_factor { 1.5F };
        /** @brief Images available to the selected render pipeline. */
        std::vector<std::shared_ptr<Core::VKImage>> textures;
        /** @brief Required render destination and pipeline overrides. */
        RenderConfig render;
    };

    /**
     * @struct Assembly
     * @brief Independent heterogeneous sources and their shared milling policy.
     */
    struct Assembly {
        /** @brief Existing assembly to complete, or null to construct one. */
        std::shared_ptr<Nodes::Network::AssemblyNetwork> network;
        /** @brief Independent sources appended before registration. */
        std::vector<Nodes::Network::AssemblyNetwork::Source> sources;
        /** @brief Fixed milling policy shared by the rendered assembly. */
        Portal::Graphics::MillSpec mill;
        /** @brief Images available to the selected render pipeline. */
        std::vector<std::shared_ptr<Core::VKImage>> textures;
        /** @brief Required render destination and pipeline overrides. */
        RenderConfig render;
    };

    /**
     * @struct Isosurface
     * @brief Existing isosurface buffer or new buffer construction state.
     */
    struct Isosurface {
        /** @brief Existing isosurface buffer, exclusive with config. */
        std::shared_ptr<Buffers::ComputeMeshBuffer> buffer;
        /** @brief Library-owned construction state when Mint creates the buffer. */
        std::optional<Buffers::ComputeMeshBuffer::Config> config;
        /** @brief Images available to the selected render pipeline. */
        std::vector<std::shared_ptr<Core::VKImage>> textures;
        /** @brief Required render destination and pipeline overrides. */
        RenderConfig render;
    };
};

} // namespace MayaFlux
