#pragma once

#include "GpuSync.hpp"
#include "MayaFlux/Kakshya/NDData/VertexLayout.hpp"

namespace MayaFlux::Nodes::GpuSync {

/**
 * @class GeometryWriterNode
 * @brief Base class for nodes that generate 3D geometry data
 *
 * Analogous to TextureNode but for vertex geometry instead of pixel data.
 * Each frame (at VISUAL_RATE), compute_frame() is called to generate new vertex data.
 * Vertex data is stored as a flat array in configurable interleaved or non-interleaved format.
 *
 * Subclasses implement compute_frame() to populate m_vertex_buffer with vertex data.
 * The buffer follows a user-defined stride (bytes per vertex) allowing flexible vertex formats:
 * - Positions only (3 floats = 12 bytes per vertex)
 * - Positions + Colors (3 + 3 floats = 24 bytes per vertex)
 * - Positions + Normals + Texcoords (3 + 3 + 2 floats = 32 bytes per vertex)
 * - Any custom interleaved format
 *
 * Usage:
 * ```cpp
 * class MyParticleSystem : public GeometryWriterNode {
 *     void compute_frame() override {
 *         std::vector<glm::vec3> positions;
 *         for (int i = 0; i < 1000; i++) {
 *             positions.push_back(generate_particle(i));
 *         }
 *
 *         set_vertex_stride(sizeof(glm::vec3));
 *         resize_vertex_buffer(positions.size());
 *         memcpy(m_vertex_buffer.data(), positions.data(),
 *                positions.size() * sizeof(glm::vec3));
 *         m_vertex_count = positions.size();
 *     }
 * };
 * ```
 */
class MAYAFLUX_API GeometryWriterNode : public GpuSync {
protected:
    /// @brief Vertex data buffer (flat array of bytes)
    std::vector<uint8_t> m_vertex_buffer;

    /// @brief Number of vertices in buffer
    uint32_t m_vertex_count {};

    /// @brief Bytes per vertex (stride for vertex buffer binding)
    size_t m_vertex_stride {};

    /// @brief Cached vertex layout for descriptor binding
    std::optional<Kakshya::VertexLayout> m_vertex_layout;

    /// @brief Flag indicating if layout needs update
    bool m_needs_layout_update {};

    /**
     * @brief Flag: vertex data or layout changed since last GPU upload
     *
     * Set to true whenever compute_frame() modifies vertex buffer or layout.
     * Checked by GeometryBindingsProcessor before staging GPU transfer.
     * Cleared by processor after successful upload.
     */
    bool m_vertex_data_dirty { true };

public:
    /**
     * @brief Constructor
     * @param initial_capacity Initial number of vertices to reserve space for
     */
    GeometryWriterNode(uint32_t initial_capacity = 1024);

    ~GeometryWriterNode() override = default;

    /**
     * @brief Process batch for geometry generation
     * @param num_samples Number of frames to process
     * @return Empty vector (geometry nodes don't produce audio output)
     *
     * Calls compute_frame() once per batch.
     * Subclasses can override for custom batch behavior.
     */
    std::vector<double> process_batch(unsigned int num_samples) override;

    /**
     * @brief Get raw vertex buffer data
     * @return Span of float data (interpreted as bytes, stride-aligned)
     */
    [[nodiscard]] std::span<const uint8_t> get_vertex_data() const { return m_vertex_buffer; }

    /**
     * @brief Get vertex buffer size in bytes
     * @return Total size of vertex data buffer
     */
    [[nodiscard]] size_t get_vertex_buffer_size_bytes() const;

    /**
     * @brief Get number of vertices
     * @return Current vertex count
     */
    [[nodiscard]] uint32_t get_vertex_count() const { return m_vertex_count; }

    /**
     * @brief Get stride (bytes per vertex)
     * @return Stride in bytes
     */
    [[nodiscard]] size_t get_vertex_stride() const { return m_vertex_stride; }

    /**
     * @brief Set vertex stride (bytes per vertex)
     * @param stride Stride in bytes
     *
     * Must be called before filling buffer. Common values:
     * - 12 (vec3 positions only)
     * - 24 (vec3 positions + vec3 colors)
     * - 32 (vec3 positions + vec3 normals + vec2 texcoords)
     */
    void set_vertex_stride(size_t stride);

    /**
     * @brief Resize vertex buffer to hold specified number of vertices
     * @param vertex_count Number of vertices
     * @param preserve_data If true, keep existing data; if false, clear buffer
     *
     * Allocates/reallocates buffer to hold vertex_count vertices at current stride.
     * If stride is 0, logs error and does nothing.
     */
    void resize_vertex_buffer(uint32_t vertex_count, bool preserve_data = false);

    /**
     * @brief Copy raw vertex data into buffer
     * @param data Pointer to vertex data
     * @param size_bytes Number of bytes to copy
     *
     * Direct memory copy into m_vertex_buffer.
     * User is responsible for ensuring data format matches stride.
     */
    void set_vertex_data(const void* data, size_t size_bytes);

    /**
     * @brief Set a single vertex by index
     * @param vertex_index Index of vertex to set
     * @param data Pointer to vertex data
     * @param size_bytes Size of vertex data (should match stride)
     *
     * Copies size_bytes starting at (vertex_index * stride).
     */
    void set_vertex(uint32_t vertex_index, const void* data, size_t size_bytes);

    /**
     * @brief Get a single vertex by index
     * @param vertex_index Index of vertex
     * @return Span covering this vertex's data
     */
    [[nodiscard]] std::span<const uint8_t> get_vertex(uint32_t vertex_index) const;

    /**
     * @brief Set multiple vertices from typed array
     * @tparam T Vertex type
     * @param vertices Span of vertex data
     *
     * Sets vertex stride to sizeof(T), resizes buffer, and copies data.
     */
    template <typename T>
    void set_vertices(std::span<const T> vertices)
    {
        set_vertex_stride(sizeof(T));
        resize_vertex_buffer(vertices.size());
        std::memcpy(m_vertex_buffer.data(), vertices.data(),
            vertices.size() * sizeof(T));
        m_vertex_count = static_cast<uint32_t>(vertices.size());
        m_needs_layout_update = true;
        m_vertex_data_dirty = true;
    }

    /**
     * @brief Set a single vertex by index from typed data
     * @tparam T Vertex type
     * @param index Index of vertex to set
     * @param vertex Vertex data
     */
    template <typename T>
    void set_vertex_typed(uint32_t index, const T& vertex)
    {
        set_vertex(index, &vertex, sizeof(T));
    }

    /**
     * @brief Get a single vertex by index as typed data
     * @tparam T Vertex type
     * @param index Index of vertex
     * @return Vertex data of type T
     */
    template <typename T>
    T get_vertex_typed(uint32_t index) const
    {
        auto data = get_vertex(index);
        if (data.size_bytes() < sizeof(T)) {
            return T {};
        }
        T result;
        std::memcpy(&result, data.data(), sizeof(T));
        return result;
    }

    /**
     * @brief Clear vertex buffer and reset count
     */
    void clear();

    /**
     * @brief Clear vertex buffer and resize to specified count
     * @param vertex_count Number of vertices
     */
    void clear_and_resize(uint32_t vertex_count);

    /**
     * @brief Set cached vertex layout
     * @param layout VertexLayout describing attribute structure
     *
     * Called internally or by GeometryBindingsProcessor to describe
     * the semantic meaning of vertex data (positions, colors, normals, etc.)
     */
    void set_vertex_layout(const Kakshya::VertexLayout& layout) { m_vertex_layout = layout; }

    /**
     * @brief Get cached vertex layout
     * @return Optional containing layout if set
     */
    [[nodiscard]] std::optional<Kakshya::VertexLayout> get_vertex_layout() const { return m_vertex_layout; }

    /**
     * @brief Check if layout needs update
     * @return True if stride or vertex count changed
     */
    [[nodiscard]] bool needs_layout_update() const { return m_needs_layout_update; }

    /**
     * @brief Clear layout update flag
     */
    void clear_layout_update_flag() { m_needs_layout_update = false; }

    /**
     * @brief Save current geometry state
     *
     * Saves vertex buffer, count, stride, and layout.
     * Subclasses can override to save additional state.
     */
    void save_state() override;

    /**
     * @brief Restore saved geometry state
     *
     * Restores vertex buffer, count, stride, and layout.
     * Subclasses can override to restore additional state.
     */
    void restore_state() override;

    /**
     * @brief Check if vertex data or layout changed since last GPU sync
     * @return True if m_vertex_buffer or m_vertex_layout has been modified
     *
     * For geometry, this combines data mutation (vertex_buffer changed) and
     * structural change (stride or layout changed).
     */
    [[nodiscard]] bool needs_gpu_update() const override
    {
        return m_vertex_data_dirty || m_needs_layout_update;
    }

    /**
     * @brief Clear the dirty flag after GPU upload completes
     *
     * Called by GeometryBindingsProcessor after it stages the vertex data
     * into a GPU transfer buffer and submits the command.
     */
    void clear_gpu_update_flag() override
    {
        m_vertex_data_dirty = false;
        m_needs_layout_update = false;
    }

private:
    struct GeometryState {
        std::vector<uint8_t> vertex_buffer;
        uint32_t vertex_count;
        size_t vertex_stride;
        std::optional<Kakshya::VertexLayout> vertex_layout;
    };

    std::optional<GeometryState> m_saved_state;
};

} // namespace MayaFlux::Nodes
