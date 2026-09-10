#pragma once

#include "MayaFlux/Kakshya/NDData/NDData.hpp"
#include <Eigen/Dense>

namespace MayaFlux::Kinesis {

/**
 * @enum InterpolationMode
 * @brief Mathematical interpolation methods
 */
enum class InterpolationMode : uint8_t {
    LINEAR,
    CATMULL_ROM,
    CUBIC_HERMITE,
    CUBIC_BEZIER,
    QUADRATIC_BEZIER,
    BSPLINE,
    COSINE,
    CUSTOM
};

/**
 * @brief Catmull-Rom spline interpolation using Eigen matrices
 * @param control_points 4xN matrix where columns are control points (p0, p1, p2, p3)
 * @param t Parameter in [0,1]
 * @param tension Tension parameter (default 0.5)
 * @return Interpolated point as Nx1 vector
 */
MAYAFLUX_API Eigen::VectorXd catmull_rom_spline(
    const Eigen::MatrixXd& control_points,
    double t,
    double tension = 0.5);

/**
 * @brief Cubic Bezier interpolation using Eigen matrices
 * @param control_points 4xN matrix where columns are control points
 * @param t Parameter in [0,1]
 * @return Interpolated point as Nx1 vector
 */
MAYAFLUX_API Eigen::VectorXd cubic_bezier(
    const Eigen::MatrixXd& control_points,
    double t);

/**
 * @brief Quadratic Bezier interpolation using Eigen matrices
 * @param control_points 3xN matrix where columns are control points
 * @param t Parameter in [0,1]
 * @return Interpolated point as Nx1 vector
 */
MAYAFLUX_API Eigen::VectorXd quadratic_bezier(
    const Eigen::MatrixXd& control_points,
    double t);

/**
 * @brief Cubic Hermite interpolation using Eigen matrices
 * @param endpoints 2xN matrix (start, end)
 * @param tangents 2xN matrix (tangent_start, tangent_end)
 * @param t Parameter in [0,1]
 * @return Interpolated point as Nx1 vector
 */
MAYAFLUX_API Eigen::VectorXd cubic_hermite(
    const Eigen::MatrixXd& endpoints,
    const Eigen::MatrixXd& tangents,
    double t);

/**
 * @brief Uniform B-spline interpolation using Eigen matrices
 * @param control_points 4xN matrix where columns are control points
 * @param t Parameter in [0,1]
 * @return Interpolated point as Nx1 vector
 */
MAYAFLUX_API Eigen::VectorXd bspline_cubic(
    const Eigen::MatrixXd& control_points,
    double t);

/**
 * @brief Generic interpolation dispatcher
 * @param control_points MxN matrix where columns are control points
 * @param t Parameter in [0,1]
 * @param mode Interpolation mode
 * @param tension Tension parameter (for applicable modes)
 * @return Interpolated point as Nx1 vector
 */
MAYAFLUX_API Eigen::VectorXd interpolate(
    const Eigen::MatrixXd& control_points,
    double t,
    InterpolationMode mode,
    double tension = 0.5);

/**
 * @struct CurveChunk
 * @brief A contiguous run of samples evaluated by one matrix product.
 *
 * Segments own disjoint, contiguous sample ranges because the segment
 * parameter is monotonic in the sample index. A segment longer than the
 * internal chunk size is split so the weight matrix stays cache resident.
 */
struct CurveChunk {
    Eigen::Index start_col { 0 }; ///< First control point column of the owning segment.
    Eigen::Index sample_begin { 0 }; ///< First output column.
    Eigen::Index sample_count { 0 }; ///< Output column count.
    bool clamp_t_high { false }; ///< Segment is pinned to t = 1 by the control clamp.
};

/**
 * @class CurveEvaluator
 * @brief Reusable interpolation state for callers evaluating many curves.
 *
 * Resolves mode and tension to a single basis matrix once, and retains the
 * extended control storage, chunk list and product scratch across calls.
 * Repeated evaluation at identical dimensions performs no heap allocation.
 *
 * Every mode is expressed as basis * monomials, including LINEAR and
 * CUBIC_HERMITE. COSINE is the one exception and builds blend weights
 * directly. The basis is folded into the control block once per chunk, so
 * the per-sample cost is a rows x pps by pps x 1 product dispatched as one
 * rows x pps by pps x count product.
 *
 * The free functions generate_interpolated_points and
 * reparameterize_by_arc_length construct one of these per call and are
 * appropriate for one-shot use. Callers in a per-frame or per-segment loop
 * should hold an instance instead.
 *
 * Not thread safe. One instance per thread, or one per node.
 *
 * @code
 * Kinesis::CurveEvaluator eval(Kinesis::InterpolationMode::CATMULL_ROM, 0.5);
 * Eigen::MatrixXd out;
 *
 * for (const auto& segment : segments) {
 *     eval.evaluate(segment, 32, out);
 * }
 * @endcode
 */
class MAYAFLUX_API CurveEvaluator {
public:
    /**
     * @brief Construct and resolve the evaluation kernel.
     * @param mode Interpolation mode.
     * @param tension Tension, consumed only by CATMULL_ROM.
     */
    explicit CurveEvaluator(
        InterpolationMode mode = InterpolationMode::CATMULL_ROM,
        double tension = 0.5);

    /**
     * @brief Re-resolve the kernel. A no-op when both arguments are unchanged.
     * @param mode Interpolation mode.
     * @param tension Tension, consumed only by CATMULL_ROM.
     */
    void configure(InterpolationMode mode, double tension);

    /** @brief Currently configured mode. */
    [[nodiscard]] InterpolationMode mode() const { return m_mode; }

    /** @brief Currently configured tension. */
    [[nodiscard]] double tension() const { return m_tension; }

    /**
     * @brief Evaluate a curve into a caller-owned matrix.
     * @param control_points MxN matrix, columns are control points.
     * @param num_samples Output column count, at least 2.
     * @param out Resized to rows(control_points) x num_samples and overwritten.
     *
     * @p control_points and @p out must not alias.
     */
    void evaluate(
        const Eigen::MatrixXd& control_points,
        Eigen::Index num_samples,
        Eigen::MatrixXd& out);

    /**
     * @brief Resample a polyline to uniform arc length into a caller-owned matrix.
     * @param points MxN matrix, columns are sequential points.
     * @param num_samples Output column count, at least 2.
     * @param out Resized to rows(points) x num_samples and overwritten.
     *
     * @p points and @p out must not alias. A zero-length span between two
     * consecutive points yields that span's start point rather than a
     * division by zero.
     */
    void reparameterize(
        const Eigen::MatrixXd& points,
        Eigen::Index num_samples,
        Eigen::MatrixXd& out);

private:
    InterpolationMode m_mode;
    double m_tension;

    Eigen::MatrixXd m_basis;
    Eigen::Index m_points_per_segment { 0 };
    Eigen::Index m_overlap { 0 };
    bool m_supports_multi { false };
    bool m_trigonometric { false };

    Eigen::MatrixXd m_extended;
    Eigen::MatrixXd m_weights;
    Eigen::MatrixXd m_folded;
    Eigen::VectorXd m_arc;

    std::vector<CurveChunk> m_chunks;
    std::vector<Eigen::Index> m_seg_first;
    std::vector<Eigen::Index> m_seg_total;

    void rebuild_kernel();

    const Eigen::MatrixXd* extend(const Eigen::MatrixXd& control_points, Eigen::Index& count);

    void build_chunks(
        Eigen::Index num_samples,
        Eigen::Index num_segments,
        Eigen::Index active_count);
};

/**
 * @brief Generate interpolated points from control points
 * @param control_points MxN matrix where columns are control points
 * @param num_samples Number of interpolated points to generate
 * @param mode Interpolation mode
 * @param tension Tension parameter
 * @return Matrix where columns are interpolated points
 */
MAYAFLUX_API Eigen::MatrixXd generate_interpolated_points(
    const Eigen::MatrixXd& control_points,
    Eigen::Index num_samples,
    InterpolationMode mode,
    double tension = 0.5);

/**
 * @brief Compute arc length of curve using trapezoidal rule
 * @param points Columns are sequential points along curve
 * @return Estimated arc length
 */
MAYAFLUX_API double compute_arc_length(const Eigen::MatrixXd& points);

/**
 * @brief Compute arc length parameterization table
 * @param points Columns are sequential points along curve
 * @return Vector of cumulative arc lengths
 */
MAYAFLUX_API Eigen::VectorXd compute_arc_length_table(const Eigen::MatrixXd& points);

/**
 * @brief Reparameterize curve by arc length
 * @param points Original points (columns)
 * @param num_samples Number of output samples
 * @return Arc-length parameterized points
 */
MAYAFLUX_API Eigen::MatrixXd reparameterize_by_arc_length(
    const Eigen::MatrixXd& points,
    Eigen::Index num_samples);

/**
 * @brief Process DataVariant through interpolation
 * @param control_points Input data as control points
 * @param num_samples Number of output samples
 * @param mode Interpolation mode
 * @param tension Tension parameter
 * @return Interpolated data
 */
MAYAFLUX_API Kakshya::DataVariant interpolate_nddata(
    const Kakshya::DataVariant& control_points,
    Eigen::Index num_samples,
    InterpolationMode mode,
    double tension = 0.5);

} // namespace MayaFlux::Kinesis
