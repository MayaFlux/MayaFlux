#pragma once

#include "MayaFlux/Buffers/VKBuffer.hpp"
#include "MayaFlux/Portal/Graphics/GraphicsUtils.hpp"

namespace MayaFlux::Buffers {

/**
 * @class SliceSet
 * @brief Many vertex blocks laid out in one buffer, drawn as few runs.
 *
 * Holds a CPU mirror of the whole batch that shares offsets with the GPU
 * buffer. Blocks are identified by id, placed contiguously in the order they
 * were added, and share one stride. Each carries a topology, defaulting to the
 * set's, so a render processor that triangulates can draw mixed spans as one
 * batch; one that does not needs every slice to match its own topology. Draw
 * order is independent
 * of memory order: bring_to_front, send_to_back and set_visible only change
 * the run list, never move vertex data. Runs adjacent in both memory and draw
 * order and of the same topology are coalesced, so an in-order visible batch
 * is a single run. Strip and fan topologies are never coalesced, since joining
 * their runs would fabricate a primitive across the seam.
 *
 * Same-size updates write into the mirror and mark one dirty range. Adding
 * appends. Resizing or removing a block shifts the blocks after it, which are
 * then uploaded as one range.
 *
 * Apply runs with RenderProcessor::set_runs and pair them with
 * set_vertex_range: a single run is not honoured by the multi-run draw, which
 * otherwise draws the buffer's whole vertex count.
 *
 * Not thread safe.
 */
class MAYAFLUX_API SliceSet {
public:
    using SliceId = uint32_t;

    /**
     * @param stride_bytes Bytes per vertex; every block must be a whole multiple.
     * @param topology Topology of a block added without one.
     */
    SliceSet(uint32_t stride_bytes, Portal::Graphics::PrimitiveTopology topology);

    /**
     * @brief Append a block at the end of memory and draw order.
     * @param bytes Whole vertices.
     * @param topology Topology of this block; the set's default when omitted.
     * @return Id of the block, or 0 if @p bytes is not a whole number of vertices.
     */
    SliceId add(std::span<const uint8_t> bytes,
        std::optional<Portal::Graphics::PrimitiveTopology> topology = std::nullopt);

    /**
     * @brief Replace a block's vertices.
     *
     * Same size writes in place. A different size shifts every later block.
     */
    void update(SliceId id, std::span<const uint8_t> bytes);

    /** @brief Remove a block; later blocks shift down. */
    void remove(SliceId id);

    /** @brief Hide or show a block without moving its data. */
    void set_visible(SliceId id, bool visible);

    /** @brief Draw a block after every other. */
    void bring_to_front(SliceId id);

    /** @brief Draw a block before every other. */
    void send_to_back(SliceId id);

    /** @brief Vertices in the mirror, hidden blocks included. */
    [[nodiscard]] uint32_t vertex_count() const noexcept;

    /** @brief Bytes in the mirror. */
    [[nodiscard]] size_t size_bytes() const noexcept { return m_mirror.size(); }

    /** @brief Number of live blocks. */
    [[nodiscard]] size_t size() const noexcept { return m_slices.size(); }

    /** @brief True when @p id names a live block. */
    [[nodiscard]] bool contains(SliceId id) const noexcept { return m_slices.contains(id); }

    /**
     * @brief Runs for the visible blocks in draw order, if they changed.
     * @return The new run list once after each change, then nullopt.
     */
    [[nodiscard]] std::optional<std::vector<Portal::Graphics::DrawRun>> take_runs();

    /**
     * @brief Upload what changed since the last flush.
     * @param target Destination buffer, grown here when the mirror outgrew it.
     * @param staging Staging buffer for device-local targets, grown as needed.
     *
     * Growing discards the target's contents, so a grow uploads the whole mirror.
     */
    void flush(const std::shared_ptr<VKBuffer>& target, std::shared_ptr<VKBuffer>& staging);

private:
    struct Slice {
        size_t byte_offset {};
        size_t byte_size {};
        Portal::Graphics::PrimitiveTopology topology {};
        bool visible { true };
    };

    void repack_from(size_t memory_index);
    void mark_dirty(size_t start, size_t end);

    uint32_t m_stride;
    Portal::Graphics::PrimitiveTopology m_topology;
    SliceId m_next_id { 1 };

    std::unordered_map<SliceId, Slice> m_slices;
    std::vector<SliceId> m_memory_order;
    std::vector<SliceId> m_draw_order;
    std::vector<uint8_t> m_mirror;
    std::vector<std::pair<size_t, size_t>> m_dirty;
    bool m_runs_changed { true };
    bool m_all_dirty { false };
};

} // namespace MayaFlux::Buffers
