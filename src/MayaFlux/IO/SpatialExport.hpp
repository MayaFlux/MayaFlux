#pragma once

#include "MayaFlux/IO/SpatialCache.hpp"

namespace MayaFlux::Buffers {
class RelaxationGridBuffer;
class NetworkGeometryBuffer;
}

namespace MayaFlux::Nodes::Network {
class GraphicsOperator;
}

namespace MayaFlux::IO {

/**
 * @brief Generate the fixed grid positions and cell ids a RelaxationGridBuffer's
 *        cells occupy, for feeding SpatialCache::write() with
 *        PrimitiveTopology::POINT_LIST.
 *
 * Matches exactly the row-major, cell-center-sampled layout its own emit
 * shaders use (data/shaders/relax_vertex_emit.comp,
 * relax_scalar_emit.comp): cell (col, row) at linear index row*width+col
 * samples at ((col+0.5)/width, (row+0.5)/height), remapped to
 * [-extent, extent] on both axes, z always 0.
 *
 * Positions and ids are fixed for a given width/height/extent: a
 * RelaxationGridBuffer's topology never changes generation to generation,
 * only its cell state does (obtained separately via
 * RelaxationGridBuffer::request_snapshot()/snapshot_source(), whose raw
 * bytes only the caller can correctly reinterpret, since state format
 * varies by rule: a single float, a uint32 automaton state, a vec2
 * reaction-diffusion pair, or anything else a rule shader defines). Call
 * once and reuse the result across every sample of a capture rather than
 * regenerating it per frame.
 *
 * ids are the cell's own linear index, so a consumer can track one cell's
 * identity across samples even though every other field about it changes
 * generation to generation.
 *
 * @param grid      Source buffer, for width/height.
 * @param extent    NDC half-span. Match whatever was set via
 *                  RelaxationEmitProcessor::set_extent(); default 1.0
 *                  matches RelaxationEmitProcessor::EmitParams's own default.
 * @param positions Output, resized to grid->get_cell_count().
 * @param ids       Output, resized to grid->get_cell_count().
 * @return False if grid is null; positions/ids are left untouched in that case.
 */
bool relaxation_grid_positions(
    const std::shared_ptr<Buffers::RelaxationGridBuffer>& grid,
    float extent,
    std::vector<glm::vec3>& positions,
    std::vector<uint64_t>& ids);

/**
 * @brief Pack one GraphicsOperator's current vertex state into a
 *        SpatialCache stream, as a Points or Curves sample depending on
 *        what the operator declares.
 *
 * Reads op->get_vertex_data() as Kakshya::Vertex records: PointVertex,
 * LineVertex, and MeshVertex all share its exact 60-byte layout, so this
 * works unchanged for any GraphicsOperator, not only PhysicsOperator. Every
 * field comes along, not only position: color, scalar (size/thickness/
 * weight depending on the concrete type), uv, normal, and tangent each
 * become their own SpatialAttribute, so a vertex reaches the archive whole
 * rather than reduced to a coordinate. Always attaches a "cluster" attribute
 * from op->build_cluster_ids() (0 for every vertex on an operator that never
 * overrides it), plus one SpatialAttribute per op->extract_vertex_attributes()
 * entry (extra state beyond the vertex record itself, e.g. PhysicsOperator's
 * mass).
 *
 * op->declared_topology() decides the sample's shape:
 *   - nullopt or POINT_LIST: the point path. ids are the vertex's own index
 *     within this operator, for cross-sample identity tracking, and native
 *     velocities come from op->extract_vertex_velocities() when non-empty.
 *   - LINE_STRIP (PathOperator): one curve per contiguous build_cluster_ids()
 *     run, matching one path per continuous interpolated strip.
 *   - LINE_LIST (TopologyOperator): fixed 2-vertex curves, one per expanded
 *     edge, since a graph's edges are independent segments, not one
 *     connected strip through the whole graph.
 *   Curve samples carry no ids/velocities (Alembic's Curves schema has
 *   neither) and fail outright if declared_topology() reports LINE_LIST for
 *   an odd vertex count, which no known producer of that topology should do.
 *
 * @param cache       Target cache, already open().
 * @param stream_name Stream to write.
 * @param op          Source operator. Must be non-null, report at least one
 *                     vertex, and use the Kakshya::Vertex record layout
 *                     (every GraphicsOperator does today).
 * @return False if op is null, reports zero vertices, its vertex layout is
 *         not a Kakshya::Vertex record, its vertex buffer is smaller than
 *         layout.stride_bytes * get_vertex_count(), any
 *         extract_vertex_attributes()/extract_vertex_velocities() entry
 *         disagrees with get_vertex_count() in size, or a LINE_LIST
 *         topology's vertex count is odd.
 */
bool write_operator_sample(
    SpatialCache& cache,
    const std::string& stream_name,
    const Nodes::Network::GraphicsOperator* op);

/**
 * @brief Pack a NetworkGeometryBuffer's driving network into one or more
 *        SpatialCache streams, choosing CPU or GPU readback depending on
 *        whether the network's CPU-side vertex state is still authoritative.
 *
 * A GpuFieldOperator anywhere in the network's operator chain (detected via
 * OperatorChain::find<GpuFieldOperator>()) means vertex positions are
 * mutated directly on the GPU: the CPU-side GraphicsOperator buffer
 * write_operator_sample() would otherwise read is stale. In that case this
 * downloads the live vertex bytes straight from the buffer itself (it is a
 * VKBuffer, so the same download_from_gpu_async() pattern
 * download_compute_mesh() uses for ComputeMeshBuffer applies directly),
 * using the still-CPU-tracked get_vertex_count()/get_vertex_layout()/
 * declared_topology() for population size, record schema, and sample shape
 * (these describe the buffer's contract, not its live GPU bytes, so they
 * stay valid regardless of which side owns the vertex data), then reads the
 * downloaded bytes as Kakshya::Vertex records exactly as
 * write_operator_sample() reads them from CPU memory: color, scalar, uv,
 * normal, and tangent all come along with position, since the record
 * schema is unchanged by which side owns the bytes; only the transfer
 * differs. A curve topology chunks on build_cluster_ids() the same way
 * write_operator_sample() does, since graph/path membership is structural
 * and unaffected by what a GpuFieldOperator does to positions. Also
 * attaches the declared hash_cluster_id state field as a "cluster"
 * attribute when present. Written as a single stream named @p stream_name:
 * once GPU-driven, the buffer is one flat array and no longer separable by
 * originating operator. Velocities and any per-rule state beyond
 * hash_cluster_id are left out deliberately, the same way
 * relaxation_grid_positions() leaves per-cell state to the caller: a
 * GpuFieldOperator's own bespoke state fields have no shape this function
 * can decode generically.
 *
 * Without a GpuFieldOperator, every GraphicsOperator on the network (its
 * primary operator plus any in get_operator_chain()) is CPU-readable and
 * each is written through write_operator_sample() as its own set: a
 * network with exactly one GraphicsOperator keeps the plain @p stream_name
 * unchanged, while a network chaining several writes one child stream per
 * operator, named "<stream_name>_<get_type_name()>" (a trailing index
 * appended for a repeated type), the same role multiple named aiMesh
 * entries play for a multi-submesh ModelWriter export. A per-operator
 * "cluster" attribute (from that operator's own build_cluster_ids())
 * continues to mark distinct populations within one operator, e.g.
 * PhysicsOperator's collections.
 *
 * @param cache       Target cache, already open().
 * @param stream_name Stream (or stream-name prefix, if the network chains
 *                     several GraphicsOperators) to write.
 * @param buffer      Source buffer. Must be non-null, with a non-null
 *                     network exposing at least one GraphicsOperator.
 * @return False if buffer/network is missing, no GraphicsOperator is found,
 *         or any underlying write_operator_sample()/cache.write() call
 *         fails.
 */
bool write_network_geometry_buffer_sample(
    SpatialCache& cache,
    const std::string& stream_name,
    const std::shared_ptr<Buffers::NetworkGeometryBuffer>& buffer);

} // namespace MayaFlux::IO
