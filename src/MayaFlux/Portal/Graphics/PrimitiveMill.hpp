#pragma once

#include "ComputePress.hpp"
#include "GraphicsUtils.hpp"

#include "MayaFlux/Kakshya/NDData/VertexLayout.hpp"

namespace MayaFlux::Buffers {
class VKBuffer;
}

namespace MayaFlux::Portal::Graphics {

/**
 * @struct MillSpec
 * @brief How a PrimitiveMill shapes spans. Stable across dispatches.
 */
struct MillSpec {
    /**
     * @enum Ribbon
     * @brief Plane a line span's ribbon and a point span's quad are built in.
     *
     * Neither mode can hold a ribbon at a constant pixel width, which requires
     * expanding after the vertex shader has projected. That remains a geometry
     * shader's job and is deliberately not represented here.
     */
    enum class Ribbon : uint8_t {
        WorldFacing, ///< Turned to face the viewpoint, in world space.
        WorldPlane ///< Held in the XY plane, viewpoint ignored.
    };

    Ribbon ribbon { Ribbon::WorldFacing };

    /**
     * @brief World units of total ribbon width per unit of a vertex's own
     *        thickness.
     *
     * Extent is per-vertex: the mill reads the layout's scalar attribute, which
     * is LineVertex::thickness and PointVertex::size. Those carry pixel-era
     * values sized for setLineWidth and gl_PointSize, so this maps them into
     * world space. A caller whose scalars are already world-scale sets these
     * to 1.
     *
     * The default reproduces line.geom, which offsets by thickness * 0.005
     * either side of the segment in NDC and so spans thickness * 0.01 in
     * total. Under an identity view NDC and world units coincide, so the same
     * figure here gives the same apparent width for the same thickness values.
     *
     * It cannot be right for every scene. A geometry shader expanding in NDC
     * holds a constant pixel width at any camera distance; milling happens
     * before projection, so width is fixed in world units and shrinks on
     * screen as the camera pulls back. Scenes at other scales must set it.
     */
    /**
     * @brief Take ribbon width and point size from each vertex's own scalar.
     *
     * On, the mill reads the layout's scalar attribute, which is
     * LineVertex::thickness and PointVertex::size, so width follows the
     * geometry. Off, every vertex uses fallback_extent and the ribbon has a
     * constant width.
     *
     * A geometry shader expanding in NDC made per-vertex variation almost
     * invisible at a pixel or two of width. Milling in world space makes the
     * same variation an order of magnitude more prominent, so geometry whose
     * thickness was authored as noise looks serrated rather than textured.
     */
    bool use_vertex_extent { true };

    float width_scale { 0.01F };

    /** @brief World units per unit of a vertex's own size, on point spans. */
    float point_scale { 0.004F };

    /** @brief Extent used when the layout carries no scalar attribute. */
    float fallback_extent { 2.0F };

    /**
     * @brief Write 0..1 across each ribbon and quad rather than copying source.
     *
     * Synthesised coordinates are what let a fragment shader run a gradient or
     * a dash along a ribbon. Copying instead gives every corner of a segment
     * the same value, which is occasionally what a caller wants.
     * Ignored when the layout carries no texture coordinate attribute.
     */
    bool synthesize_uv { true };
};

/**
 * @struct MillView
 * @brief Per-dispatch viewpoint. Consumed only by MillSpec::Ribbon::WorldFacing.
 */
struct MillView {
    glm::vec3 eye {};
};

/**
 * @class PrimitiveMill
 * @brief Mills spans of mixed primitive topology into one TRIANGLE_LIST vertex
 *        buffer on the GPU.
 *
 * One job. Given a source vertex buffer, its layout, and a list of DrawRun
 * spans, it produces a single triangle buffer carrying that same vertex layout,
 * so every span can be drawn by one non-indexed draw at one topology. Points
 * become quads, line segments become ribbons, triangle spans reduce to lists.
 * Positions are rewritten and texture coordinates optionally synthesised; every
 * other attribute is copied bit-exact from the source vertex its corner belongs
 * to, so a vertex shader written against the source layout consumes the result
 * unchanged.
 *
 * Owns its kernel, pipeline, descriptor sets and destination buffer, and
 * dispatches through ComputePress directly. It is a dispatch driver in the
 * manner of Yantra's GPU executor, not a BufferProcessor: no processing token,
 * no attach, no chain membership, no per-cycle driver. Milling is demand driven
 * by whoever is about to draw, so a chain position would fix when it runs
 * relative to the geometry it consumes.
 *
 * The output is a transient draw source, not authored geometry. It is
 * regenerated whenever the spans or the viewpoint change, its vertices have no
 * correspondence back to the source, and it is not an export or readback
 * surface. The intended caller is a render processor resolving its geometry
 * immediately before recording.
 *
 * ## Known gaps
 *
 * Ribbon quality is not yet good enough for drawing work. Joins are mitred, so
 * consecutive segments share their corners, but the result still shows visible
 * unevenness along a curve. The remaining causes are not isolated: candidates
 * are the miter limit falling back to the plain segment normal at moderate
 * angles, width being resolved per vertex rather than along arc length, and the
 * absence of any round join or cap.
 *
 * Width is fixed in world units. A geometry shader expanding after projection
 * holds a constant pixel width at any camera distance; this cannot, so a ribbon
 * thins on screen as the camera pulls back. Matching a screen-space width would
 * require expanding after the vertex shader, which a compute prepass cannot do.
 *
 * mill_on_host has diverged and is no longer an oracle for the kernel. It
 * implements the unjoined form only: no miter, no width averaging across shared
 * positions. Treat it as a reference for span indexing and expansion counts,
 * not for ribbon geometry, until it is brought back into step.
 *
 * A producer emitting a polyline as duplicated vertex pairs costs twice the
 * milled vertices it needs, since every second segment is a zero-length seam
 * that collapses. Detected and skipped, but still budgeted for.
 */
class MAYAFLUX_API PrimitiveMill {
public:
    explicit PrimitiveMill(MillSpec spec = {});
    ~PrimitiveMill();

    PrimitiveMill(const PrimitiveMill&) = delete;
    PrimitiveMill& operator=(const PrimitiveMill&) = delete;
    PrimitiveMill(PrimitiveMill&&) = delete;
    PrimitiveMill& operator=(PrimitiveMill&&) = delete;

    /**
     * @brief Vertices @p runs would mill to. No dispatch, no allocation.
     *
     * Closed form over the spans, which is why nothing here needs an atomic
     * counter, a readback, or a frame of latency to learn its own output size.
     */
    [[nodiscard]] static uint32_t milled_vertex_count(std::span<const DrawRun> runs);

    /**
     * @brief Mill @p runs out of @p source into the owned buffer.
     * @param source Vertex buffer the spans index into. Must carry a vertex
     *        layout whose stride is a multiple of four bytes and which has a
     *        word aligned position attribute.
     * @param runs Spans to mill, in output order.
     * @param view Viewpoint, consumed only by Ribbon::WorldFacing.
     * @return Vertices written, or 0 when there was nothing to mill or the
     *         source could not be used.
     *
     * Does not wait on its own dispatch. It resolves the previous call's
     * submission first, then submits this one and returns. output() is
     * immediately valid to *record* a draw against, because the recorded
     * barriers order the compute write against vertex input on the same queue;
     * it is not valid to read from the host until a later mill() or release()
     * has resolved the fence.
     *
     * Waiting on the previous dispatch is not optional: the run and prefix
     * tables are host visible and rewritten here, so the prior dispatch must
     * have finished reading them.
     *
     * The destination grows to fit and never shrinks.
     */
    uint32_t mill(
        const std::shared_ptr<Buffers::VKBuffer>& source,
        std::span<const DrawRun> runs,
        const MillView& view);

    /** @brief The milled triangles. Null before the first successful mill(). */
    [[nodiscard]] const std::shared_ptr<Buffers::VKBuffer>& output() const { return m_output; }

    /** @brief Vertices written by the last mill(). */
    [[nodiscard]] uint32_t milled_count() const { return m_milled_count; }

    [[nodiscard]] const MillSpec& spec() const { return m_spec; }

    /** @brief Replace the spec. Takes effect on the next mill(). */
    void set_spec(const MillSpec& spec) { m_spec = spec; }

    /** @brief Destroy the kernel, pipeline, descriptor sets and buffers. */
    void release();

private:
    /** @brief Compiles the kernel and allocates its descriptor sets, once. */
    bool ensure_kernel();

    /** @brief Grows the destination, run and prefix buffers to fit. */
    bool ensure_buffers(
        const std::shared_ptr<Buffers::VKBuffer>& source,
        const Kakshya::VertexLayout& layout,
        uint32_t total,
        size_t run_count);

    /** @brief Points the descriptor set at the current buffer set. */
    void write_descriptors(const std::shared_ptr<Buffers::VKBuffer>& source);

    /**
     * @brief Wait on and reclaim the previous dispatch, if one is outstanding.
     *
     * Cheap in steady state: the work was submitted a frame earlier and has
     * normally completed. Releases the fence and its command buffer.
     */
    void resolve_pending();

    MillSpec m_spec;

    ShaderID m_shader { INVALID_SHADER };
    ComputePipelineID m_pipeline { INVALID_COMPUTE_PIPELINE };
    std::vector<DescriptorSetID> m_sets;
    size_t m_push_constant_size { 0 };

    std::shared_ptr<Buffers::VKBuffer> m_output;
    std::shared_ptr<Buffers::VKBuffer> m_run_buf;
    std::shared_ptr<Buffers::VKBuffer> m_prefix_buf;

    /// Source the descriptor set currently points at, for invalidation.
    std::weak_ptr<Buffers::VKBuffer> m_bound_source;

    std::vector<uint32_t> m_prefix;

    uint32_t m_milled_count { 0 };
    uint32_t m_output_capacity { 0 };

    /// Outstanding dispatch, resolved at the start of the next mill().
    FenceID m_pending_fence { INVALID_FENCE };
};

/**
 * @brief Host equivalent of PrimitiveMill::mill, over raw bytes.
 * @param src Source vertex bytes. Must be four byte aligned.
 * @param layout Describes @p src. The result carries the same attributes.
 * @param runs Spans to mill, in output order.
 * @param spec Shaping parameters, as for the device path.
 * @param view Viewpoint, as for the device path.
 * @param dst Resized to hold the result.
 * @return Layout describing @p dst, which is @p layout with a new vertex_count.
 *
 * Exists so the kernel has an oracle: identical spans, spec and view must
 * produce identical bytes on both paths. Secondarily a route for a caller with
 * no compute queue available. Not a general geometry utility, and not intended
 * for per-frame use at scale.
 */
MAYAFLUX_API Kakshya::VertexLayout mill_on_host(
    std::span<const uint8_t> src,
    const Kakshya::VertexLayout& layout,
    std::span<const DrawRun> runs,
    const MillSpec& spec,
    const MillView& view,
    std::vector<uint8_t>& dst);

} // namespace MayaFlux::Portal::Graphics
