#pragma once

#include "MayaFlux/Kinesis/Spatial/Bounds.hpp"

namespace MayaFlux::Buffers {
class VKBuffer;
}

namespace MayaFlux::Nexus {

/**
 * @brief Anchor of a buffer's geometry in its own stored coordinates.
 *
 * The position of the vertex at @p index, else the surface centroid, else the
 * vertex mean for a mesh without faces. A point or path buffer reports the mean of
 * its vertices; an InstanceNetworkBuffer, with no index, the centre of its bounds.
 * Blocks for a ComputeMeshBuffer and for a buffer a field processor drives.
 *
 * @param buf   Buffer to read.
 * @param index Vertex to anchor at, if any.
 * @return The anchor, or nullopt when no geometry can be read from the buffer.
 */
MAYAFLUX_API std::optional<glm::vec3> read_anchor(
    const std::shared_ptr<Buffers::VKBuffer>& buf,
    const std::optional<uint32_t>& index);

/**
 * @brief Bounds of a buffer's geometry in its own stored coordinates.
 *
 * A VolumeGridBuffer reports its lattice bounds. A NetworkGeometryBuffer reports
 * its vertices, or those of one collection. The primary operator only knows the
 * authored positions and the CPU updates it dispatched, so once the chain holds a
 * GpuFieldOperator the bytes are downloaded from the device instead, as the spatial
 * export does; before the device buffer exists the operator's bytes are used. A
 * GeometryBuffer on a node other than a MeshWriterNode, a point or path node for
 * example, reports the node's own vertices, downloaded from the device when a
 * VertexFieldProcessor is on the buffer. An InstanceNetworkBuffer reports the union
 * of its slots' bounds through the slot transforms as the CPU holds them; the eight
 * corners are placed, so a rotated slot is bounded loosely, and a transform a GPU
 * kernel drives is not seen. Other mesh buffers are read as read_anchor reads them.
 * Blocks for a ComputeMeshBuffer and for a buffer a field processor drives: call it
 * from a FRAME_ACCURATE metro or the main thread, never the graphics thread.
 *
 * @param buf        Buffer to read.
 * @param collection Collection of a NetworkGeometryBuffer to bound, if not all of it.
 *                   Ignored for other buffers.
 * @return The bounds, or nullopt when no geometry can be read from the buffer.
 */
MAYAFLUX_API std::optional<Kinesis::AABB3D> read_bounds(
    const std::shared_ptr<Buffers::VKBuffer>& buf,
    const std::optional<uint32_t>& collection = std::nullopt);

/**
 * @brief Where a buffer is drawn: the geometry transform of its primary render processor.
 *
 * The transform source if it has one, else the static transform, else identity.
 * Takes stored coordinates to drawn coordinates.
 *
 * @param buf Buffer to read.
 */
MAYAFLUX_API glm::mat4 read_placement(const std::shared_ptr<Buffers::VKBuffer>& buf);

} // namespace MayaFlux::Nexus
