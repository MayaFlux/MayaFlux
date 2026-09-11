#pragma once

#include "MayaFlux/IO/ModelWriter.hpp"

namespace MayaFlux::Buffers {
class MeshBuffer;
class MeshNetworkBuffer;
class ComputeMeshBuffer;
}

namespace MayaFlux::Nodes::GpuSync {
class MeshWriterNode;
}

namespace MayaFlux::Nodes::Network {
class MeshNetwork;
}

namespace MayaFlux::IO {

/**
 * @brief Save a MeshBuffer's current mesh data to disk via ModelWriterRegistry.
 *
 * Synchronous: MeshData is always CPU-authoritative for MeshBuffer (every
 * mutation path writes the CPU copy first, GPU is upload-only), so there is
 * no GPU download step here, unlike save_image/save_volume.
 *
 * @param buffer   Source mesh.
 * @param filepath Destination path with extension; selects the writer via
 *                 ModelWriterRegistry.
 * @param options  Format-specific writer options.
 * @return True on success. Failure is logged.
 */
[[nodiscard]] bool save_mesh(
    const std::shared_ptr<Buffers::MeshBuffer>& buffer,
    const std::string& filepath,
    const ModelWriteOptions& options = {});

/**
 * @brief Save a MeshNetworkBuffer's current slots to disk as one multi-mesh
 *        file, one aiMesh per slot.
 *
 * Each slot's vertices (position, normal, tangent) are baked into world
 * space from that slot's current world_transform before export, so an
 * exploded/rotated network exports in the pose it is actually in, not its
 * local rest pose. A slot with no node, or a node with no vertices yet, is
 * skipped rather than failing the whole export; the export fails only if
 * every slot is empty.
 *
 * @param network_buffer Source network buffer.
 * @param filepath       Destination path with extension.
 * @param options        Format-specific writer options.
 * @return True on success. Failure is logged.
 */
[[nodiscard]] bool save_mesh(
    const std::shared_ptr<Buffers::MeshNetworkBuffer>& network_buffer,
    const std::string& filepath,
    const ModelWriteOptions& options = {});

/**
 * @brief Save a MeshNetwork's current slots to disk as one multi-mesh file.
 *
 * Same shape as the MeshNetworkBuffer overload (each slot's vertices baked
 * into world space from its current world_transform), but works directly
 * on the network: slot data lives on MeshNetwork itself, not on the GPU
 * buffer, so a network never wrapped in a MeshNetworkBuffer, or not yet
 * rendered, is still exportable. The MeshNetworkBuffer overload forwards
 * here via get_network().
 *
 * @param network  Source network.
 * @param filepath Destination path with extension.
 * @param options  Format-specific writer options.
 * @return True on success. Failure is logged.
 */
[[nodiscard]] bool save_mesh(
    const std::shared_ptr<Nodes::Network::MeshNetwork>& network,
    const std::string& filepath,
    const ModelWriteOptions& options = {});

/**
 * @brief Save a bare MeshWriterNode's current vertices/indices to disk.
 *
 * For geometry driven directly through GeometryBuffer rather than wrapped
 * in a MeshBuffer. No world transform is applied: a bare node has none of
 * its own, unlike a MeshNetwork slot.
 *
 * @param node     Source node. Must have both vertices and indices set.
 * @param filepath Destination path with extension.
 * @param options  Format-specific writer options.
 * @return True on success. Failure is logged.
 */
[[nodiscard]] bool save_mesh(
    const std::shared_ptr<Nodes::GpuSync::MeshWriterNode>& node,
    const std::string& filepath,
    const ModelWriteOptions& options = {});

/**
 * @brief Download a ComputeMeshBuffer's current live geometry to CPU.
 *
 * ComputeMeshBuffer's own doc states its live rendering path never reads
 * back vertex data; this is the deliberate, one-shot exception for export.
 * Reads the true live vertex count off the atomic counter buffer's mapped
 * pointer (host-visible, no transfer), then downloads exactly that many
 * vertices from the buffer itself, not its worst-case allocated capacity.
 * The result is non-indexed triangles, so a trivial sequential index array
 * (0..N-1) is synthesized to satisfy Kakshya::MeshData's index requirement.
 *
 * @param buffer Source buffer. setup_processors() must have run.
 * @return The current geometry, or nullopt if the buffer is null, has no
 *         mesh processor yet, or the live vertex count is zero.
 */
[[nodiscard]] std::optional<Kakshya::MeshData> download_compute_mesh(
    const std::shared_ptr<Buffers::ComputeMeshBuffer>& buffer);

/**
 * @brief Save a ComputeMeshBuffer's current live geometry to disk.
 *
 * Wraps download_compute_mesh() and write_via_registry() in one call. See
 * download_compute_mesh's own doc for the one-shot readback this performs.
 */
[[nodiscard]] bool save_mesh(
    const std::shared_ptr<Buffers::ComputeMeshBuffer>& buffer,
    const std::string& filepath,
    const ModelWriteOptions& options = {});

/**
 * @brief Save a ComputeMeshBuffer with a millisecond epoch timestamp
 *        spliced into the path. See the MeshBuffer overload's doc for the
 *        case this serves.
 */
[[nodiscard]] bool save_mesh_snapshot(
    const std::shared_ptr<Buffers::ComputeMeshBuffer>& buffer,
    const std::string& path_pattern,
    const ModelWriteOptions& options = {});

/**
 * @brief Save with a millisecond epoch timestamp spliced into the path.
 *
 * For the "save whenever, whatever it looks like right now" case: a caller
 * watching a live-deforming mesh who wants to catch a particular passing
 * state, as many times as they like, without managing a counter or
 * overwriting the last capture. Needs no state of its own, unlike a
 * frame-numbered sequence.
 *
 * @param buffer      Source mesh.
 * @param path_pattern Path with one std::format replacement field for the
 *        timestamp, e.g. "drone_{}.obj".
 * @param options     Format-specific writer options.
 * @return True on success. Failure is logged.
 */
[[nodiscard]] bool save_mesh_snapshot(
    const std::shared_ptr<Buffers::MeshBuffer>& buffer,
    const std::string& path_pattern,
    const ModelWriteOptions& options = {});

/**
 * @brief Save a MeshNetworkBuffer with a millisecond epoch timestamp spliced
 *        into the path. See the MeshBuffer overload's doc for the case this
 *        serves.
 */
[[nodiscard]] bool save_mesh_snapshot(
    const std::shared_ptr<Buffers::MeshNetworkBuffer>& network_buffer,
    const std::string& path_pattern,
    const ModelWriteOptions& options = {});

/**
 * @brief Save a MeshNetwork with a millisecond epoch timestamp spliced into
 *        the path. See the MeshBuffer overload's doc for the case this serves.
 */
[[nodiscard]] bool save_mesh_snapshot(
    const std::shared_ptr<Nodes::Network::MeshNetwork>& network,
    const std::string& path_pattern,
    const ModelWriteOptions& options = {});

/**
 * @brief Save a bare MeshWriterNode with a millisecond epoch timestamp
 *        spliced into the path. See the MeshBuffer overload's doc for the
 *        case this serves.
 */
[[nodiscard]] bool save_mesh_snapshot(
    const std::shared_ptr<Nodes::GpuSync::MeshWriterNode>& node,
    const std::string& path_pattern,
    const ModelWriteOptions& options = {});

} // namespace MayaFlux::IO
