#pragma once

#include "StructureConfig.hpp"
#include "MayaFlux/Transitive/Memory/LiveArena.hpp"

namespace MayaFlux::Buffers {
class ComputeMeshBuffer;
class InstanceNetworkBuffer;
class MeshBuffer;
class MeshNetworkBuffer;
class NetworkGeometryBuffer;
}

namespace MayaFlux {

/**
 * @class Mint
 * @brief Turns a structural config into a registered and rendered object.
 *
 * Each overload performs the complete sequence for one arrangement: resolve
 * its source, construct any required network, register the network and buffer
 * in their required order, attach textures and configure rendering. Every
 * config requires RenderConfig::target_window.
 *
 * Nothing here is realtime safe. Call from the thread that constructs the
 * objects.
 */
class MAYAFLUX_API Mint {
public:
    Mint();
    ~Mint();

    Mint(const Mint&) = delete;
    Mint& operator=(const Mint&) = delete;
    Mint(Mint&&) noexcept;
    Mint& operator=(Mint&&) noexcept;

    /**
     * @brief Mint one independently rendered object and its backing MeshWriterNode.
     * @return MeshBuffer whose get_node() returns the supplied or created node.
     */
    auto operator()(const StructureConfig::Object& config)
        -> std::shared_ptr<Buffers::MeshBuffer>
    {
        auto structure = build(config);
        if (structure) {
            MF_LIVE_EXPOSE_AUTO(structure);
        }
        return structure;
    }

    /**
     * @brief Mint a model and its backing MeshNetwork.
     * @return MeshNetworkBuffer whose get_network() returns the supplied or created network.
     */
    auto operator()(const StructureConfig::Model& config)
        -> std::shared_ptr<Buffers::MeshNetworkBuffer>
    {
        auto structure = build(config);
        if (structure) {
            MF_LIVE_EXPOSE_AUTO(structure);
        }
        return structure;
    }

    /**
     * @brief Mint repeated instances and their backing InstanceNetwork.
     * @return InstanceNetworkBuffer whose get_network() returns the supplied or created network.
     */
    auto operator()(const StructureConfig::Instances& config)
        -> std::shared_ptr<Buffers::InstanceNetworkBuffer>
    {
        auto structure = build(config);
        if (structure) {
            MF_LIVE_EXPOSE_AUTO(structure);
        }
        return structure;
    }

    /** @brief Mint independently supplied geometry into one rendered assembly. */
    auto operator()(const StructureConfig::Assembly& config)
        -> std::shared_ptr<Buffers::NetworkGeometryBuffer>
    {
        auto structure = build(config);
        if (structure) {
            MF_LIVE_EXPOSE_AUTO(structure);
        }
        return structure;
    }

    /** @brief Mint a CPU or GPU evaluated isosurface. */
    auto operator()(const StructureConfig::Isosurface& config)
        -> std::shared_ptr<Buffers::ComputeMeshBuffer>
    {
        auto structure = build(config);
        if (structure) {
            MF_LIVE_EXPOSE_AUTO(structure);
        }
        return structure;
    }

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;

    std::shared_ptr<Buffers::MeshBuffer> build(
        const StructureConfig::Object& config);
    std::shared_ptr<Buffers::MeshNetworkBuffer> build(
        const StructureConfig::Model& config);
    std::shared_ptr<Buffers::InstanceNetworkBuffer> build(
        const StructureConfig::Instances& config);
    std::shared_ptr<Buffers::NetworkGeometryBuffer> build(
        const StructureConfig::Assembly& config);
    std::shared_ptr<Buffers::ComputeMeshBuffer> build(
        const StructureConfig::Isosurface& config);
};

} // namespace MayaFlux
