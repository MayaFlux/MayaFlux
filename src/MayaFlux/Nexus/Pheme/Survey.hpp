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
 * its vertices; an InstanceNetworkBuffer, with no index, the centre of its bounds. A
 * MeshNetworkBuffer is read through its slots' transforms.
 *
 * With @p slot, one slot of a MeshNetworkBuffer or InstanceNetworkBuffer is read
 * in the slot's own coordinates, and @p index counts that slot's vertices;
 * read_placement with the same slot takes the result to where it is drawn.
 * Blocks for a ComputeMeshBuffer and for a buffer a field processor drives.
 *
 * @param buf   Buffer to read.
 * @param index Vertex to anchor at, if any.
 * @param slot  Slot of a mesh or instance network, if not all of it. Ignored for
 *              other buffers.
 * @return The anchor, or nullopt when no geometry can be read from the buffer,
 *         as for a slot out of range or a joint slot.
 */
MAYAFLUX_API std::optional<glm::vec3> read_anchor(
    const std::shared_ptr<Buffers::VKBuffer>& buf,
    const std::optional<uint32_t>& index,
    const std::optional<uint32_t>& slot = std::nullopt);

/**
 * @brief The anchor of a buffer or one slot of it, as it stands at each call.
 *
 * A skinned slot of a mesh or instance network answers the mean of its vertices
 * under the palette it is drawn with, summed once from the rest vertices so that a
 * call costs the joints and never reads the device. Morph targets are not applied,
 * and before a palette exists the slot answers as bound. Anything else answers
 * read_anchor, read at the first call that can read it and held from then on.
 *
 * @param buf  Buffer to follow. Not retained.
 * @param slot Slot of a mesh or instance network, if not all of it.
 * @return A callable answering the anchor in the coordinates read_anchor uses, or
 *         nullopt while nothing can be read.
 */
MAYAFLUX_API std::function<std::optional<glm::vec3>()> track_anchor(
    const std::shared_ptr<Buffers::VKBuffer>& buf,
    const std::optional<uint32_t>& slot = std::nullopt);

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
 * of its drawn geometry, the first slot's node, through each slot's transform as the CPU holds it; the eight
 * corners are placed, so a rotated slot is bounded loosely, and a transform a GPU
 * kernel drives is not seen. A MeshNetworkBuffer is read through its slots' transforms.
 * Other mesh buffers are read as read_anchor reads them.
 *
 * For a MeshNetworkBuffer or InstanceNetworkBuffer, @p collection is a slot, bounded
 * in the slot's own coordinates; a skinned instance covers its joints as well. An
 * instance with a weight for every morph target of the template is bounded with
 * those targets applied, at the cost of a pass over the template's vertices.
 * Blocks for a ComputeMeshBuffer and for a buffer a field processor drives: call it
 * from a FRAME_ACCURATE metro or the main thread, never the graphics thread.
 *
 * @param buf        Buffer to read.
 * @param collection Collection of a NetworkGeometryBuffer or RecordedGeometryBuffer,
 *                   or slot of a mesh or instance network, to bound, if not all of
 *                   it. Ignored for other buffers.
 * @return The bounds, or nullopt when no geometry can be read from the buffer.
 */
MAYAFLUX_API std::optional<Kinesis::AABB3D> read_bounds(
    const std::shared_ptr<Buffers::VKBuffer>& buf,
    const std::optional<uint32_t>& collection = std::nullopt);

/**
 * @brief Where a buffer is drawn: the geometry transform of its primary render processor.
 *
 * The transform source if it has one, else the static transform, else identity.
 * Takes stored coordinates to drawn coordinates. With @p slot, a slot of a mesh
 * or instance network, that slot's transform is applied inside it, as the CPU
 * holds it: a mesh network's as last propagated.
 *
 * @param buf  Buffer to read.
 * @param slot Slot of a mesh or instance network, if any. Ignored for other
 *             buffers and when out of range.
 */
MAYAFLUX_API glm::mat4 read_placement(
    const std::shared_ptr<Buffers::VKBuffer>& buf,
    const std::optional<uint32_t>& slot = std::nullopt);

} // namespace MayaFlux::Nexus
