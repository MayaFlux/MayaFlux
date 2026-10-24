#pragma once

#include "MayaFlux/Buffers/Staging/SliceSet.hpp"
#include "MayaFlux/Buffers/VKBuffer.hpp"
#include "MayaFlux/Portal/Graphics/GraphicsUtils.hpp"

namespace MayaFlux::Buffers {

/**
 * @class FormaProcessor
 * @brief Default processor for FormaBuffer.
 *
 * Owns the CPU-to-GPU upload cycle for Forma geometry through a SliceSet.
 * Geometry is held as one or more slices of one stride and topology, laid out
 * in a single vertex buffer. set_bytes() writes the default slice, so a
 * standalone buffer is a batch of one. Each graphics tick uploads only what
 * changed, then points the RenderProcessor at the visible slices: one draw per
 * run, adjacent runs coalesced.
 *
 * The topology is fixed at construction to derive the per-vertex stride.
 *
 * Slice calls and processing run on the graphics thread; nothing here is
 * synchronised. A trailing partial vertex in submitted bytes is dropped.
 */
class MAYAFLUX_API FormaProcessor : public VKBufferProcessor {
public:
    using SliceId = SliceSet::SliceId;

    explicit FormaProcessor(Portal::Graphics::PrimitiveTopology topology);
    ~FormaProcessor() override = default;

    /**
     * @brief Replace the default slice with new vertex bytes.
     *
     * Applied on the next processing_function call. The default slice is
     * created on first use.
     *
     * @param bytes Raw interleaved vertex bytes matching the topology's
     *              vertex type (PointVertex, LineVertex, or MeshVertex).
     */
    void set_bytes(const std::vector<uint8_t>& bytes);

    /**
     * @brief Append a slice, drawn after the existing ones. Returns 0 on failure.
     * @param topology Topology of this slice; the processor's when omitted. A
     *        slice of a different topology needs a render processor that
     *        triangulates, otherwise only matching topologies draw correctly.
     */
    SliceId add_slice(const std::vector<uint8_t>& bytes,
        std::optional<Portal::Graphics::PrimitiveTopology> topology = std::nullopt);

    /** @brief Replace a slice's vertices; later slices shift if the size changes. */
    void update_slice(SliceId id, const std::vector<uint8_t>& bytes);

    /** @brief Remove a slice; later slices shift down. */
    void remove_slice(SliceId id);

    /** @brief Hide or show a slice without moving its data. */
    void set_slice_visible(SliceId id, bool visible);

    /** @brief Draw a slice after every other. */
    void bring_slice_to_front(SliceId id);

    /** @brief Draw a slice before every other. */
    void send_slice_to_back(SliceId id);

    /** @brief Number of live slices. */
    [[nodiscard]] size_t slice_count() const noexcept { return m_slices.size(); }

    /** @brief True when @p id names a live slice. */
    [[nodiscard]] bool has_slice(SliceId id) const noexcept { return m_slices.contains(id); }

    /** @brief True when geometry or slice changes await the next tick. */
    [[nodiscard]] bool has_pending() const noexcept;

    /**
     * @brief Supply a texture to bind on the next graphics tick.
     *
     * Stores the image and binding name behind an atomic dirty flag.
     * On the next processing_function call the image is bound to the
     * RenderProcessor via bind_texture(). Calling again replaces the
     * pending binding before it has been consumed.
     *
     * @param image   GPU image. nullptr clears the binding.
     * @param binding Descriptor name matching the fragment shader.
     */
    void set_texture(std::shared_ptr<Core::VKImage> image, std::string binding);

protected:
    void on_attach(const std::shared_ptr<Buffer>& buffer) override;
    void on_detach(const std::shared_ptr<Buffer>& buffer) override;
    void processing_function(const std::shared_ptr<Buffer>& buffer) override;

private:
    struct PendingTexture {
        std::shared_ptr<Core::VKImage> image;
        std::string binding;
    };

    Portal::Graphics::PrimitiveTopology m_topology;
    uint32_t m_stride { 0 };

    SliceSet m_slices;
    SliceId m_default_slice { 0 };
    bool m_changed { false };
    std::optional<std::vector<Portal::Graphics::DrawRun>> m_pending_runs;

    std::optional<PendingTexture> m_pending_texture;
    std::atomic_flag m_texture_dirty;

    std::shared_ptr<VKBuffer> m_staging;

    [[nodiscard]] std::span<const uint8_t> whole_vertices(const std::vector<uint8_t>& bytes) const noexcept;
    void apply_runs(const std::shared_ptr<VKBuffer>& buffer);
};

} // namespace MayaFlux::Buffers
