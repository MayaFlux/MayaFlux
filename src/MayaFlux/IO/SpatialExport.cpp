#include "SpatialExport.hpp"

#include "MayaFlux/Buffers/Network/NetworkGeometryBuffer.hpp"
#include "MayaFlux/Buffers/Staging/StagingUtils.hpp"
#include "MayaFlux/Buffers/State/RelaxationGridBuffer.hpp"
#include "MayaFlux/Kakshya/NDData/VertexFormats.hpp"
#include "MayaFlux/Nodes/Network/Operators/GpuFieldOperator.hpp"
#include "MayaFlux/Nodes/Network/Operators/GraphicsOperator.hpp"
#include "MayaFlux/Portal/Graphics/GraphicsUtils.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::IO {

namespace {

    /**
     * @brief Read every vertex, whole, into SpatialSample-ready columns.
     *
     * A GraphicsOperator's raw vertex buffer is Kakshya::Vertex records
     * (PointVertex/LineVertex/MeshVertex all share its exact 60-byte
     * layout, and Vertex's own doc calls it "pipeline-ready" for exactly
     * this): position, color, scalar, uv, normal, tangent, read directly by
     * field rather than rediscovered attribute-by-attribute from
     * VertexLayout metadata that only ever describes this same fixed shape.
     * position becomes SpatialSample::positions; the rest become named
     * attributes (scalar carries size/thickness/weight depending on the
     * concrete vertex type, per Vertex's own doc).
     *
     * @return False if @p layout is not this 60-byte record (no known
     *         GraphicsOperator today produces anything else); positions and
     *         attributes are left untouched in that case.
     */
    bool extract_vertex_columns(
        const Kakshya::VertexLayout& layout,
        std::span<const uint8_t> raw,
        size_t vertex_count,
        std::vector<glm::vec3>& positions,
        std::vector<SpatialAttribute>& attributes)
    {
        if (layout.stride_bytes != sizeof(Kakshya::Vertex)) {
            return false;
        }

        const auto* verts = reinterpret_cast<const Kakshya::Vertex*>(raw.data());

        positions.resize(vertex_count);
        std::vector<glm::vec3> colors(vertex_count);
        std::vector<float> scalars(vertex_count);
        std::vector<glm::vec2> uvs(vertex_count);
        std::vector<glm::vec3> normals(vertex_count);
        std::vector<glm::vec3> tangents(vertex_count);

        for (size_t i = 0; i < vertex_count; ++i) {
            positions[i] = verts[i].position;
            colors[i] = verts[i].color;
            scalars[i] = verts[i].scalar;
            uvs[i] = verts[i].uv;
            normals[i] = verts[i].normal;
            tangents[i] = verts[i].tangent;
        }

        attributes.push_back(SpatialAttribute { .name = "color", .scope = SpatialScope::Varying,
            .values = Kakshya::DataVariant { std::move(colors) } });
        attributes.push_back(SpatialAttribute { .name = "scalar", .scope = SpatialScope::Varying,
            .values = Kakshya::DataVariant { std::move(scalars) } });
        attributes.push_back(SpatialAttribute { .name = "uv", .scope = SpatialScope::Varying,
            .values = Kakshya::DataVariant { std::move(uvs) } });
        attributes.push_back(SpatialAttribute { .name = "normal", .scope = SpatialScope::Varying,
            .values = Kakshya::DataVariant { std::move(normals) } });
        attributes.push_back(SpatialAttribute { .name = "tangent", .scope = SpatialScope::Varying,
            .values = Kakshya::DataVariant { std::move(tangents) } });

        return true;
    }

} // namespace

bool relaxation_grid_positions(
    const std::shared_ptr<Buffers::RelaxationGridBuffer>& grid,
    float extent,
    std::vector<glm::vec3>& positions,
    std::vector<uint64_t>& ids)
{
    if (!grid) {
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "relaxation_grid_positions: null grid");
        return false;
    }

    const uint32_t width = grid->get_grid_width();
    const uint32_t height = grid->get_grid_height();
    const uint32_t cell_count = grid->get_cell_count();

    positions.resize(cell_count);
    ids.resize(cell_count);

    for (uint32_t i = 0; i < cell_count; ++i) {
        const uint32_t ix = i % width;
        const uint32_t iy = i / width;

        const float fx = (static_cast<float>(ix) + 0.5F) / static_cast<float>(width);
        const float fy = (static_cast<float>(iy) + 0.5F) / static_cast<float>(height);

        positions[i] = glm::vec3((fx * 2.0F - 1.0F) * extent, (fy * 2.0F - 1.0F) * extent, 0.0F);
        ids[i] = i;
    }

    return true;
}

bool write_operator_sample(
    SpatialCache& cache,
    const std::string& stream_name,
    const Nodes::Network::GraphicsOperator* op)
{
    if (!op) {
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "write_operator_sample: null operator");
        return false;
    }

    const size_t vertex_count = op->get_vertex_count();
    if (vertex_count == 0) {
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "write_operator_sample: operator reports zero vertices");
        return false;
    }

    const Kakshya::VertexLayout layout = op->get_vertex_layout();
    const std::span<const uint8_t> raw = op->get_vertex_data();
    if (layout.stride_bytes == 0 || raw.size() < static_cast<size_t>(layout.stride_bytes) * vertex_count) {
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "write_operator_sample: vertex buffer smaller than layout implies");
        return false;
    }

    std::vector<glm::vec3> positions;
    std::vector<SpatialAttribute> attributes;
    if (!extract_vertex_columns(layout, raw, vertex_count, positions, attributes)) {
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "write_operator_sample: vertex layout is not a Kakshya::Vertex record");
        return false;
    }

    std::vector<uint64_t> ids(vertex_count);
    std::ranges::iota(ids, uint64_t { 0 });

    std::vector<glm::vec3> velocities = op->extract_vertex_velocities();
    if (!velocities.empty() && velocities.size() != vertex_count) {
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "write_operator_sample: extract_vertex_velocities() count mismatch");
        return false;
    }

    std::vector<uint32_t> cluster_ids = op->build_cluster_ids();
    if (cluster_ids.size() != vertex_count) {
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "write_operator_sample: build_cluster_ids() count mismatch");
        return false;
    }
    attributes.push_back(SpatialAttribute {
        .name = "cluster",
        .scope = SpatialScope::Varying,
        .values = Kakshya::DataVariant { std::move(cluster_ids) } });

    for (auto& [name, values] : op->extract_vertex_attributes()) {
        const size_t count = std::visit([](const auto& v) { return v.size(); }, values);
        if (count != vertex_count) {
            MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
                "write_operator_sample: extract_vertex_attributes() '{}' count mismatch", name);
            return false;
        }
        attributes.push_back(SpatialAttribute {
            .name = name,
            .scope = SpatialScope::Varying,
            .values = std::move(values) });
    }

    return cache.write(stream_name,
        SpatialSample {
            .topology = Portal::Graphics::PrimitiveTopology::POINT_LIST,
            .positions = positions,
            .ids = ids,
            .velocities = velocities,
            .attributes = attributes });
}

namespace {

    /**
     * @brief GPU-authoritative path: download vertex bytes straight from the
     *        buffer itself, since NetworkGeometryBuffer is a VKBuffer.
     *
     * Population size and record schema come from the still-CPU-tracked
     * primary GraphicsOperator (get_vertex_count()/get_vertex_layout()
     * describe the buffer's contract, not its live GPU bytes, so they stay
     * valid even once a GpuFieldOperator owns those bytes). Attaches a
     * "cluster" attribute from the declared hash_cluster_id state field
     * when present; velocities and any other per-rule state are left out,
     * since no name or shape for them is known generically here.
     */
    bool write_network_geometry_buffer_gpu_sample(
        SpatialCache& cache,
        const std::string& stream_name,
        const std::shared_ptr<Buffers::NetworkGeometryBuffer>& buffer,
        Nodes::Network::GraphicsOperator* graphics_op)
    {
        const size_t vertex_count = graphics_op->get_vertex_count();
        if (vertex_count == 0) {
            MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
                "write_network_geometry_buffer_sample: operator reports zero vertices");
            return false;
        }

        const Kakshya::VertexLayout layout = graphics_op->get_vertex_layout();
        if (layout.stride_bytes != sizeof(Kakshya::Vertex)) {
            MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
                "write_network_geometry_buffer_sample: vertex layout is not a Kakshya::Vertex record");
            return false;
        }

        std::vector<uint8_t> raw(vertex_count * layout.stride_bytes);
        std::shared_ptr<Buffers::VKBuffer> staging;
        Buffers::download_from_gpu_async(buffer, raw.data(), raw.size(), staging);

        std::vector<glm::vec3> positions;
        std::vector<SpatialAttribute> attributes;
        if (!extract_vertex_columns(layout, raw, vertex_count, positions, attributes)) {
            MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
                "write_network_geometry_buffer_sample: vertex layout is not a Kakshya::Vertex record");
            return false;
        }

        std::vector<uint64_t> ids(vertex_count);
        std::ranges::iota(ids, uint64_t { 0 });

        if (buffer->has_state("hash_cluster_id")
            && buffer->get_state_bytes("hash_cluster_id") == vertex_count * sizeof(uint32_t)) {
            std::vector<uint32_t> cluster_ids(vertex_count);
            Buffers::download_back_buffer(
                buffer->read_state_slot("hash_cluster_id"), cluster_ids.data(),
                cluster_ids.size() * sizeof(uint32_t), staging);
            attributes.push_back(SpatialAttribute {
                .name = "cluster",
                .scope = SpatialScope::Varying,
                .values = Kakshya::DataVariant { std::move(cluster_ids) } });
        }

        return cache.write(stream_name,
            SpatialSample {
                .topology = Portal::Graphics::PrimitiveTopology::POINT_LIST,
                .positions = positions,
                .ids = ids,
                .attributes = attributes });
    }

} // namespace

bool write_network_geometry_buffer_sample(
    SpatialCache& cache,
    const std::string& stream_name,
    const std::shared_ptr<Buffers::NetworkGeometryBuffer>& buffer)
{
    if (!buffer) {
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "write_network_geometry_buffer_sample: null buffer");
        return false;
    }

    auto network = buffer->get_network();
    if (!network) {
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "write_network_geometry_buffer_sample: buffer has no network");
        return false;
    }

    auto chain = network->get_operator_chain();

    if (chain && chain->find<Nodes::Network::GpuFieldOperator>()) {
        auto* graphics_op = dynamic_cast<Nodes::Network::GraphicsOperator*>(network->get_operator());
        if (!graphics_op) {
            MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
                "write_network_geometry_buffer_sample: network's primary operator is not a GraphicsOperator");
            return false;
        }
        return write_network_geometry_buffer_gpu_sample(cache, stream_name, buffer, graphics_op);
    }

    std::vector<std::pair<std::string, Nodes::Network::GraphicsOperator*>> graphics_ops;
    if (auto* primary = dynamic_cast<Nodes::Network::GraphicsOperator*>(network->get_operator())) {
        graphics_ops.emplace_back(std::string(primary->get_type_name()), primary);
    }
    if (chain) {
        for (const auto& op : chain->operators()) {
            if (auto* secondary = dynamic_cast<Nodes::Network::GraphicsOperator*>(op.get())) {
                graphics_ops.emplace_back(std::string(secondary->get_type_name()), secondary);
            }
        }
    }

    if (graphics_ops.empty()) {
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "write_network_geometry_buffer_sample: network has no GraphicsOperator");
        return false;
    }

    if (graphics_ops.size() == 1) {
        return write_operator_sample(cache, stream_name, graphics_ops.front().second);
    }

    std::unordered_map<std::string, int> seen;
    for (const auto& [type_name, op] : graphics_ops) {
        int& occurrence = seen[type_name];
        std::string set_name = stream_name;
        set_name.append("_").append(type_name);
        if (occurrence != 0) {
            set_name.append(std::to_string(occurrence));
        }
        ++occurrence;

        if (!write_operator_sample(cache, set_name, op)) {
            return false;
        }
    }

    return true;
}

} // namespace MayaFlux::IO
