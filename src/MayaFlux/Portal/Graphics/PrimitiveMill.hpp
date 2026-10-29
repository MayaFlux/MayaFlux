#pragma once

#include "ComputePress.hpp"
#include "GraphicsUtils.hpp"

#include "MayaFlux/Kakshya/NDData/VertexLayout.hpp"

namespace MayaFlux::Buffers {
class VKBuffer;
class SlotArena;
}

namespace MayaFlux::Portal::Graphics {

/**
 * @struct MillSpec
 * @brief How a PrimitiveMill shapes spans. Stable across dispatches.
 */
struct MillSpec {
    /**
     * @enum Ribbon
     * @brief Plane that line ribbons and point quads are built in.
     *
     * Neither mode holds a ribbon at a constant pixel width, which requires
     * expanding after projection. The mill works in world space and does not
     * do that.
     */
    enum class Ribbon : uint8_t {
        WorldFacing, ///< Turned to face the viewpoint, in world space.
        WorldPlane ///< Held in the XY plane, viewpoint ignored.
    };

    Ribbon ribbon { Ribbon::WorldFacing };

    /**
     * @brief Take ribbon width and point size from each vertex's own scalar
     *        (LineVertex::thickness, PointVertex::size) instead of
     *        fallback_extent.
     *
     * Variation between neighbouring vertices is much more visible in world
     * space than at a pixel or two of screen width, so a scalar authored as
     * noise reads as serration along the ribbon.
     */
    bool use_vertex_extent { true };

    /**
     * @brief World units of total ribbon width per unit of a vertex's scalar.
     *
     * Line scalars are usually authored as pixel widths, so this maps them into
     * world space. Set it to 1 when the scalars are already world scale. The
     * right value depends on the scene, since width does not track screen size.
     */
    float width_scale { 0.05F };

    /** @brief World units per unit of a vertex's own size, on point spans. */
    float point_scale { 0.004F };

    /** @brief Extent used when the layout carries no scalar attribute. */
    float fallback_extent { 2.0F };

    /**
     * @brief Write 0..1 UVs across each ribbon and quad instead of copying the
     *        source.
     *
     * Lets a fragment shader run a gradient, dash or edge falloff along a
     * ribbon. Ignored when the layout carries no texture coordinates.
     *
     * Point quad corners land in [MILL_POINT_UV_MARKER, +1] instead of [0, 1]
     * (data/shaders/include/mill_shape.glsl), which is how a shared fragment
     * shader tells a point quad from a ribbon or a passthrough triangle. Use
     * mill_is_point() and mill_point_local_uv() rather than testing the
     * threshold directly.
     */
    bool synthesize_uv { true };

    /**
     * @brief Keep point quads and ribbons unexpanded for an instanced draw.
     *
     * The mill places the source position and writes the expansion into the
     * tangent attribute: a point quad stores its corner offset in xy, a ribbon
     * stores its unit direction in xyz and its signed half width in w. The
     * instance vertex shader then faces each instance toward the viewpoint after
     * its own transform. Applies only to Ribbon::WorldFacing and needs a tangent
     * attribute in the vertex layout.
     */
    bool billboard_instances { false };
};

/**
 * @struct MillView
 * @brief Per-dispatch viewpoint. Used only by MillSpec::Ribbon::WorldFacing.
 */
struct MillView {
    glm::vec3 eye {};
};

/**
 * @class PrimitiveMill
 * @brief Mills spans of mixed primitive topology into one TRIANGLE_LIST vertex
 *        buffer on the GPU.
 *
 * Given a source vertex buffer, its layout and a list of DrawRun spans, produces
 * one triangle buffer with the same vertex layout, so every span can be drawn by
 * one non-indexed draw at one topology. Points become quads, line segments
 * become ribbons and triangle spans reduce to lists. Positions are rewritten and
 * texture coordinates optionally synthesised; every other attribute is copied
 * bit-exact from the source vertex its corner belongs to, so a vertex shader
 * written against the source layout consumes the result unchanged.
 *
 * Owns its kernel, pipeline and destination buffers and dispatches through
 * ComputePress. It is not a BufferProcessor: it has no processing token, attach
 * or chain position, because milling is demand driven by whoever is about to
 * draw, normally a render processor.
 *
 * The output is a transient draw source, not authored geometry. It is
 * regenerated whenever the spans or the viewpoint change, its vertices have no
 * correspondence back to the source, and it is not an export or readback
 * surface.
 *
 * ## Limits
 *
 * Ribbon width and point size are fixed in world units, so a ribbon thins on
 * screen as the camera pulls back. Joins are mitred; there are no round joins
 * or caps. A polyline emitted as duplicated vertex pairs mills twice the
 * vertices it needs, since every second segment is a zero-length seam that is
 * skipped but still counted.
 */
class MAYAFLUX_API PrimitiveMill {
public:
    /**
     * @param spec Shaping parameters.
     * @param output_ring Number of milled buffers rotated between dispatches, at
     *        least one.
     *
     * A ring of one makes each dispatch wait behind the previous frame's reads.
     * Each extra slot costs one full copy of the milled geometry, tens of
     * megabytes for dense line work.
     */
    explicit PrimitiveMill(MillSpec spec = {}, uint32_t output_ring = 2);
    ~PrimitiveMill();

    PrimitiveMill(const PrimitiveMill&) = delete;
    PrimitiveMill& operator=(const PrimitiveMill&) = delete;
    PrimitiveMill(PrimitiveMill&&) = delete;
    PrimitiveMill& operator=(PrimitiveMill&&) = delete;

    /**
     * @brief Vertices @p runs would mill to. No dispatch, no allocation.
     *
     * Computed from the spans alone, so the output size is known without a
     * readback.
     */
    [[nodiscard]] static uint32_t milled_vertex_count(std::span<const DrawRun> runs);

    /**
     * @brief Mill @p runs out of @p source into the owned buffer.
     * @param source Vertex buffer the spans index into. Must carry a vertex
     *        layout unless @p layout is supplied. The stride must be a multiple
     *        of four bytes and the position attribute word aligned. With an
     *        index buffer, triangle spans count and offset indices; point and
     *        line spans stay vertex addressed.
     * @param runs Spans to mill, in output order.
     * @param view Viewpoint, used only by Ribbon::WorldFacing.
     * @param layout Optional layout describing the source data. Read from
     *        @p source when omitted.
     * @return Vertices written, or 0 when there was nothing to mill or the
     *         source could not be used.
     *
     * Does not wait on its own dispatch. It resolves the previous call's
     * submission, submits this one and returns. output() can be recorded
     * against immediately, but it is not valid to read from the host until a
     * later mill() or release() has resolved the fence.
     *
     * Runs on the graphics queue, so a draw submitted afterwards is ordered
     * after the write. The destination grows to fit and never shrinks.
     */
    uint32_t mill(
        const std::shared_ptr<Buffers::VKBuffer>& source,
        std::span<const DrawRun> runs,
        const MillView& view,
        std::optional<Kakshya::VertexLayout> layout = std::nullopt);

    /**
     * @brief The milled triangles: the ring slot the last mill() wrote.
     * @return Null before the first successful mill().
     *
     * Fetch again after every mill() rather than caching it; the ring hands back
     * a different buffer each dispatch.
     */
    [[nodiscard]] std::shared_ptr<Buffers::VKBuffer> output() const;

    /** @brief Vertices written by the last mill(). */
    [[nodiscard]] uint32_t milled_count() const { return m_milled_count; }

    /** @brief The current shaping parameters. */
    [[nodiscard]] const MillSpec& spec() const { return m_spec; }

    /** @brief Replace the spec. Takes effect on the next mill(). */
    void set_spec(const MillSpec& spec) { m_spec = spec; }

    /** @brief Destroy the kernel, pipeline and buffers. */
    void release();

private:
    /** @brief Compiles the kernel and creates its pipeline with a push descriptor set, once. */
    bool ensure_kernel();

    /**
     * @brief Grows the destination ring, run and prefix arenas to fit.
     *
     * Every slot is replaced at once and the outgoing buffers are released once
     * the work submitted so far completes. The 1.5x headroom keeps geometry that
     * is being added to from re-growing on nearly every cycle.
     */
    bool ensure_buffers(
        const std::shared_ptr<Buffers::VKBuffer>& source,
        const Kakshya::VertexLayout& layout,
        uint32_t total,
        size_t run_count);

    /**
     * @brief Reclaim every dispatch that has finished, without waiting.
     *
     * Releases each finished fence and its command buffer and advances the
     * completed epoch the run and prefix arenas compare against.
     */
    void collect_completed();

    /**
     * @brief Wait on and reclaim every outstanding dispatch.
     *
     * For teardown, where the pipeline and shader are about to be destroyed.
     */
    void resolve_pending();

    MillSpec m_spec;

    ShaderID m_shader { INVALID_SHADER };
    ComputePipelineID m_pipeline { INVALID_COMPUTE_PIPELINE };
    size_t m_push_constant_size { 0 };

    /// Milled buffers rotated between dispatches, all at m_output_capacity.
    std::vector<std::shared_ptr<Buffers::VKBuffer>> m_outputs;
    std::unique_ptr<Buffers::SlotArena> m_run_arena;
    std::unique_ptr<Buffers::SlotArena> m_prefix_arena;
    size_t m_run_slot { 0 };
    size_t m_prefix_slot { 0 };

    std::vector<uint32_t> m_prefix;
    std::vector<uint32_t> m_run_words;

    uint32_t m_output_ring { 2 };
    size_t m_output_slot { 0 };

    uint32_t m_milled_count { 0 };
    uint32_t m_output_capacity { 0 };

    struct InFlight {
        uint64_t epoch;
        FenceID fence;
    };

    /// Dispatches submitted and not yet observed complete, oldest first.
    std::vector<InFlight> m_in_flight;
    uint64_t m_dispatch_epoch { 0 };
    uint64_t m_completed_epoch { 0 };
};

/**
 * @brief Host equivalent of PrimitiveMill::mill, over raw bytes.
 * @param src Source vertex bytes. Must be four byte aligned.
 * @param layout Describes @p src. The result carries the same attributes.
 * @param runs Spans to mill, in output order.
 * @param spec Shaping parameters, as for the device path.
 * @param view Viewpoint, as for the device path.
 * @param dst Resized to hold the result.
 * @param indices When non-empty, triangle spans address these indices instead of vertices.
 * @return Layout describing @p dst, which is @p layout with a new vertex_count.
 *
 * Gives the kernel a reference to be tested against, and a route for a caller
 * with no compute queue available. It builds the unjoined ribbon form only (no
 * miter, no width averaging across shared positions), so use it to check span
 * indexing and vertex counts rather than ribbon geometry. Not a general geometry
 * utility, and not intended for per-frame use at scale.
 */
MAYAFLUX_API Kakshya::VertexLayout mill_on_host(
    std::span<const uint8_t> src,
    const Kakshya::VertexLayout& layout,
    std::span<const DrawRun> runs,
    const MillSpec& spec,
    const MillView& view,
    std::vector<uint8_t>& dst,
    std::span<const uint32_t> indices = {});

} // namespace MayaFlux::Portal::Graphics
