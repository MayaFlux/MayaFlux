#pragma once

#include "MayaFlux/Kakshya/Semantic/MeshAssemblyParts.hpp"
#include "MayaFlux/Kinesis/VertexSampler.hpp"
#include "MayaFlux/Kriya/MeshReflow.hpp"
#include "MayaFlux/Kriya/MotionReflow.hpp"
#include "MayaFlux/Kriya/SpatialReflow.hpp"
#include "MayaFlux/Nodes/Network/MeshNetwork.hpp"

namespace MayaFlux::Buffers {
class MeshNetworkBuffer;
class InstanceNetworkBuffer;
class RecordedGeometryBuffer;
}

namespace MayaFlux::Nodes::Network {
class InstanceNetwork;
class GpuFieldOperator;
}

namespace MayaFlux::Core {
class VKImage;
}

namespace MayaFlux::Kriya {

/**
 * @struct MeshAssemblyRealizeOptions
 * @brief How an assembly is cut into parts.
 */
struct MeshAssemblyRealizeOptions {
    /** Which part each row belongs to. Empty keeps the rows of each kind together. */
    Kakshya::MeshAssemblyPartition partition;

    /** Whether a part of meshes is flat or keeps the hierarchy. Empty makes every part flat. */
    std::function<Nodes::Network::MeshPartPlacement(const Kakshya::MeshAssemblyPart&)> placement;

    /**
     * The operator a buffer of points or curves runs, for the spatial stages (hash, density
     * colour, claims) and fields its SpatialFieldConfig and bindings ask for, or null for a
     * buffer that only draws. Called with the part and the row of each buffer, so every
     * row can have an operator of its own.
     */
    std::function<std::shared_ptr<Nodes::Network::GpuFieldOperator>(const Kakshya::MeshAssemblyPart&, uint64_t)> spatial_field;

    /**
     * The box the spatial hash of every buffer of points or curves covers, in the source's own
     * coordinates. Empty takes the box of each source's first shot, padded a little, so a
     * stage has bounds without anyone measuring the geometry.
     */
    std::optional<Kinesis::SamplerBounds> bounds;
};

/**
 * @brief Gives the image a material shows, or null for none.
 *
 * Called once for each material a part uses, with the material as the file wrote it, so the
 * resolver sees the path and whether it points into the file (diffuse_embedded). The images
 * follow TextureLoom's holders: the part that binds one retains it, and the caller releases
 * the hold its resolver took.
 */
using MeshAssemblyMaterialResolver = std::function<std::shared_ptr<Core::VKImage>(const Kakshya::MeshAssemblyMaterial&)>;

/**
 * @struct MeshAssemblySkinnedSlot
 * @brief A slot of a network part whose mesh is skinned, the row it shows and the skin it follows.
 */
struct MeshAssemblySkinnedSlot {
    uint32_t slot {};
    uint64_t row {};
    std::shared_ptr<const Kakshya::MeshSkin> skin;
};

/**
 * @struct MeshAssemblyRealizedPart
 * @brief One target of a realized assembly and the rows it shows.
 *
 * A part is one of: a network of meshes (network and network_buffer, with the slots whose mesh is skinned in
 * skinned, each given its palette by the placement sink), the instances of one mesh
 * (instances and instance_buffer), or points or curves (one entry of spatial_buffers for each
 * row, nullptr for a row whose geometry is empty). rows[i] is the row slot, instance or buffer i
 * shows. The buffers are made and not registered, set up or drawn: that is the caller's.
 */
struct MeshAssemblyRealizedPart {
    Kakshya::MeshAssemblyRowKind kind {};
    bool instanced {};
    Nodes::Network::MeshPartPlacement placement { Nodes::Network::MeshPartPlacement::Flat };
    std::vector<uint64_t> rows;

    std::shared_ptr<Nodes::Network::MeshNetwork> network;
    std::shared_ptr<Buffers::MeshNetworkBuffer> network_buffer;
    std::vector<MeshAssemblySkinnedSlot> skinned;

    std::shared_ptr<Nodes::Network::InstanceNetwork> instances;
    std::shared_ptr<Buffers::InstanceNetworkBuffer> instance_buffer;

    std::vector<std::shared_ptr<Buffers::RecordedGeometryBuffer>> spatial_buffers;
};

/**
 * @class MeshAssemblyRealization
 * @brief An assembly cut into targets: networks, instanced networks and buffers of points or
 *        curves, each showing some of its rows, with the sinks and lanes that move them.
 *
 * One container is not one network. The rows are sorted into parts by a partition, and each part
 * gets the target its kind needs. A level can then be a network of its static meshes, a network
 * of the one mesh that animates, an instanced draw for every mesh shown many times, and a buffer
 * for each set of points, all placed by the one hierarchy. The partition is yours, and a single
 * network of the whole assembly is only the case where it puts every row together.
 *
 * Placement is resolved once for all parts. A MotionReflow whose frames come from a
 * MeshAssemblyMotionFrames gives every frame both local and world placements, and
 * placement_sink hands each part the ones it needs: flat parts and instances and buffers the
 * world placement of their rows, chained parts the local one. Geometry that changes over time is
 * served by a lane per source (mesh_routes, spatial_routes), each writing the frame to every
 * place its source is shown.
 *
 * Nothing here registers, sets up or draws a buffer, and no sink or lane starts anything.
 */
class MAYAFLUX_API MeshAssemblyRealization {
public:
    /**
     * @brief Cut an assembly into targets.
     * @param assembly The assembly. Null gives nullptr.
     * @return nullptr for a null assembly or one that shows nothing.
     */
    [[nodiscard]] static std::shared_ptr<MeshAssemblyRealization> create(
        const std::shared_ptr<const Kakshya::MeshAssemblyContainer>& assembly,
        const MeshAssemblyRealizeOptions& options = {});

    [[nodiscard]] const std::vector<MeshAssemblyRealizedPart>& parts() const noexcept { return m_parts; }

    [[nodiscard]] const std::shared_ptr<const Kakshya::MeshAssemblyContainer>& assembly() const noexcept { return m_assembly; }

    /** @brief A sink that places every part. For a MotionReflow over a MeshAssemblyMotionFrames. */
    [[nodiscard]] MotionReflow::Sink placement_sink() const;

    /** @brief A sink that places one part. An empty sink for a part out of range. */
    [[nodiscard]] MotionReflow::Sink placement_sink(size_t part) const;

    /**
     * @brief One lane for each mesh source that has frames and is shown by a part, writing them to every place it is shown.
     * @param edge What each lane does at the ends of its source.
     *
     * Each lane has the source's own frames and a course over their times, running to the end of
     * the last frame's duration so a loop shows every frame, and a sink of its own, so it belongs
     * to a reflow built with the lanes as routes. Change a course
     * setting, add operations or replace the sink before adding it.
     */
    [[nodiscard]] std::vector<CourseRoute<Kakshya::MeshData>> mesh_routes(CourseEdge edge = CourseEdge::Loop) const;

    /** @brief As mesh_routes, for sources of points and curves, showing their frames in the buffers of their rows. */
    [[nodiscard]] std::vector<CourseRoute<Kakshya::SpatialData>> spatial_routes(CourseEdge edge = CourseEdge::Loop) const;

    /**
     * @brief Show the first shot of every source of points or curves in the buffers made for it.
     *
     * A buffer takes its first frame after it is set up, never before, so call this once the
     * buffers are registered and rendering is set up. A source with frames is then replaced by
     * the lane that plays them.
     */
    void show_rest() const;

    /**
     * @brief Give every part the images its materials name.
     * @param resolver Finds the image of a material. A null image leaves what the part had.
     * @return How many images were bound.
     *
     * A network gives each slot the image of its row's material (MeshNetwork::set_slot_texture)
     * and an instanced part the one image its group shares (InstanceNetworkBuffer::bind_instance_textures).
     * Points and curves have no surface to texture and are left alone. Call it before the buffers
     * are set up when the textured shaders are wanted, as the buffers read their textures at setup.
     */
    size_t bind_materials(const MeshAssemblyMaterialResolver& resolver) const;

    /**
     * @brief Tint every instance with the colour of its row's material.
     * @return How many instances were tinted.
     *
     * Reads the MeshAssemblySurface a reader kept in the material's facets and sets the instance's tint
     * to its base colour, which the instance shaders multiply into the template's colour. Only instanced
     * parts have a tint of their own: a network's slots and the buffers of points and curves are drawn
     * with the colour of their vertices, which a field or the frames themselves set. Call it when the
     * parts are made, or again after the facets change.
     */
    size_t apply_surfaces() const;

private:
    MeshAssemblyRealization() = default;

    std::shared_ptr<const Kakshya::MeshAssemblyContainer> m_assembly;
    std::vector<MeshAssemblyRealizedPart> m_parts;
};

} // namespace MayaFlux::Kriya
