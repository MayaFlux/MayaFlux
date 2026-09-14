#pragma once

#include "GraphicsOperator.hpp"

#include "MayaFlux/Kakshya/NDData/MeshData.hpp"
#include "MayaFlux/Nodes/Graphics/GeometryLeafNode.hpp"
#include "MayaFlux/Nodes/Graphics/MeshWriterNode.hpp"

namespace MayaFlux::Nodes::Network {

/**
 * @class AssemblyOperator
 * @brief Holds an unordered set of independent geometry items, rendered as
 *        one milled draw.
 *
 * Not a specialized operator: unlike PathOperator/TopologyOperator/
 * PhysicsOperator, it interpolates, connects, or simulates nothing between
 * items. Items are stored in a plain std::vector<shared_ptr<GeometryWriterNode>>
 * in insertion order, not behind a handle: the shared_ptr each add_geometry()
 * overload returns is itself the caller's access token for later mutation or
 * removal, the same idiom PathOperator::add_node() already uses for a single
 * path. Removal is a pointer-identity scan; if that becomes a hot per-frame
 * path for thousands of items, revisit the storage, not the access API.
 *
 * apply_one_to_one() is a documented no-op here (get_data_at() always
 * returns nullptr): unrelated items added and removed independently have no
 * positional correspondence for a ONE_TO_ONE per-index mapping to land on.
 */
class MAYAFLUX_API AssemblyOperator : public GraphicsOperator {
public:
    AssemblyOperator() = default;

    /// @brief Add an item from plain positions (color/scalar/normal/uv default).
    std::shared_ptr<GpuSync::GeometryLeafNode> add_geometry(
        std::vector<glm::vec3> positions,
        Portal::Graphics::PrimitiveTopology topology = Portal::Graphics::PrimitiveTopology::POINT_LIST);

    /// @brief Add an item from fully specified vertices.
    std::shared_ptr<GpuSync::GeometryLeafNode> add_geometry(
        std::vector<Kakshya::Vertex> vertices,
        Portal::Graphics::PrimitiveTopology topology = Portal::Graphics::PrimitiveTopology::POINT_LIST);

    /// @brief Add an indexed triangle mesh. Topology is fixed TRIANGLE_LIST.
    std::shared_ptr<GpuSync::MeshWriterNode> add_geometry(const Kakshya::MeshData& mesh);

    /// @brief Add a caller-built node directly; its own topology is used as-is.
    void add_geometry(std::shared_ptr<GpuSync::GeometryWriterNode> node);

    /**
     * @brief Remove an item by identity.
     * @return true if the node was present and removed.
     */
    bool remove_geometry(const std::shared_ptr<GpuSync::GeometryWriterNode>& node);

    [[nodiscard]] size_t item_count() const { return m_items.size(); }

    void process(float dt) override;

    [[nodiscard]] std::span<const uint8_t> get_vertex_data() const override;
    [[nodiscard]] std::span<const uint8_t> get_vertex_data_for_collection(uint32_t idx) const override;

    /**
     * @brief The universal Kakshya::Vertex attribute layout, vertex_count 0
     *        when there are no items yet. Every item's record shares one
     *        60-byte offset table regardless of topology (only the scalar
     *        field's name differs), so the shape is knowable -- and a
     *        render pipeline buildable -- before the first item ever
     *        arrives; only the count is data-dependent.
     */
    [[nodiscard]] Kakshya::VertexLayout get_vertex_layout() const override;
    [[nodiscard]] size_t get_vertex_count() const override;
    [[nodiscard]] bool is_vertex_data_dirty() const override;
    void mark_vertex_data_clean() override;

    /**
     * @brief One DirtyVertexRange per item that needs a GPU update, in
     *        add_geometry() order, matching get_vertex_data().
     */
    [[nodiscard]] std::vector<DirtyVertexRange> dirty_vertex_ranges() const override;
    [[nodiscard]] bool supports_incremental_upload() const override { return true; }

    [[nodiscard]] size_t get_point_count() const override;

    /**
     * @brief One cluster id per item; coincides with get_vertex_count()
     *        since items carry no interpolation step of their own.
     */
    [[nodiscard]] std::vector<uint32_t> build_cluster_ids() const override;

    /// @brief Forwards to the first item's own get_primitive_topology().
    [[nodiscard]] std::optional<Portal::Graphics::PrimitiveTopology> declared_topology() const override;

    /**
     * @brief Coalesces adjacent items sharing a concatenable topology into
     *        one run; strip/fan items are always reported separately.
     */
    [[nodiscard]] std::vector<DrawRun> topology_runs() const override;

    void set_parameter(std::string_view param, double value) override;
    [[nodiscard]] std::optional<double> query_state(std::string_view query) const override;
    [[nodiscard]] std::string_view get_type_name() const override { return "Assembly"; }
    [[nodiscard]] const char* get_vertex_type_name() const override { return "Vertex"; }

protected:
    void* get_data_at(size_t global_index) override;

private:
    std::vector<std::shared_ptr<GpuSync::GeometryWriterNode>> m_items;
    mutable std::vector<uint8_t> m_vertex_data_aggregate;
};

} // namespace MayaFlux::Nodes::Network
