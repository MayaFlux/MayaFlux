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
 * @brief A contiguous run of samples evaluated in one pass.
 *
 * Segments own disjoint, contiguous sample ranges because the segment
 * parameter is monotonic in the sample index. A segment longer than the
 * internal chunk size is split so the parameter buffer stays cache resident.
 */
struct CurveChunk {
    Eigen::Index segment { 0 }; ///< Owning segment index.
    Eigen::Index start_col { 0 }; ///< First control point of the owning segment.
    Eigen::Index sample_begin { 0 }; ///< First output sample.
    Eigen::Index sample_count { 0 }; ///< Output sample count.
    bool clamp_t_high { false }; ///< Segment pinned to t = 1 by the control clamp.
};

/**
 * @class CurveEvaluator
 * @brief Reusable interpolation state for callers evaluating many curves.
 *
 * The evaluation core operates on plain double buffers with no Eigen types
 * and no allocation after the first call at given dimensions. Eigen appears
 * only at the API boundary and in the basis matrix declarations.
 *
 * ## Layouts
 * Control points are point-major: the coordinate index varies fastest, so
 * point q occupies `control_points[q * dim .. q * dim + dim)`. This matches
 * the memory of a column-major Eigen matrix whose columns are control
 * points, so `matrix.data()` can be passed directly.
 *
 * Planar output is coordinate-major: `out[d * num_samples + i]` is coordinate
 * d of sample i. Consecutive samples of one coordinate are contiguous, which
 * is what lets the sample axis occupy the SIMD lanes. Callers wanting an
 * Eigen matrix pay one transpose in the boundary overloads.
 *
 * ## Evaluation
 * Mode and tension resolve once to a single basis matrix, applied to the
 * control block once per chunk rather than per sample. Each coordinate is
 * then a Horner evaluation over the sample parameter, four samples per
 * vector under AVX2 and two under NEON.
 *
 * Not thread safe. One instance per thread, or one per node.
 *
 * @code
 * Kinesis::CurveEvaluator eval(Kinesis::InterpolationMode::CATMULL_ROM, 0.5);
 * std::vector<double> out;
 *
 * eval.evaluate_planar(controls, 3, 32, out);
 * const double* x = out.data();
 * const double* y = x + 32;
 * const double* z = y + 32;
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
     * @brief Evaluate a curve into a coordinate-major buffer.
     * @param control_points Point-major, dim * control_count doubles.
     * @param dim Coordinate count per point, at least 1.
     * @param num_samples Output sample count, at least 2.
     * @param out Resized to dim * num_samples, coordinate-major.
     *
     * @p control_points and @p out must not alias.
     */
    void evaluate_planar(
        std::span<const double> control_points,
        size_t dim,
        Eigen::Index num_samples,
        std::vector<double>& out);

    /**
     * @brief Resample a polyline to uniform arc length, coordinate-major.
     * @param points Coordinate-major, dim * point_count doubles.
     * @param dim Coordinate count per point, at least 1.
     * @param point_count Input sample count.
     * @param num_samples Output sample count, at least 2.
     * @param out Resized to dim * num_samples, coordinate-major.
     *
     * @p points and @p out must not alias. A zero-length span between two
     * consecutive points yields that span's start point rather than a
     * division by zero.
     */
    void reparameterize_planar(
        std::span<const double> points,
        size_t dim,
        Eigen::Index point_count,
        Eigen::Index num_samples,
        std::vector<double>& out);

    /**
     * @brief Evaluate a curve into a caller-owned matrix.
     * @param control_points MxN matrix, columns are control points.
     * @param num_samples Output column count, at least 2.
     * @param out Resized to rows(control_points) x num_samples and overwritten.
     *
     * Boundary overload. Wraps evaluate_planar and transposes the result.
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
     * Boundary overload. Transposes in, wraps reparameterize_planar,
     * transposes out.
     */
    void reparameterize(
        const Eigen::MatrixXd& points,
        Eigen::Index num_samples,
        Eigen::MatrixXd& out);

private:
    InterpolationMode m_mode;
    double m_tension;

    std::vector<double> m_basis;
    Eigen::Index m_points_per_segment { 0 };
    Eigen::Index m_overlap { 0 };
    bool m_supports_multi { false };
    bool m_trigonometric { false };

    std::vector<double> m_extended;
    std::vector<double> m_folded;
    std::vector<double> m_tbuf;
    std::vector<double> m_arc;
    std::vector<double> m_planar;
    std::vector<double> m_planar_alt;
    std::vector<size_t> m_lower;
    std::vector<double> m_frac;

    std::vector<CurveChunk> m_chunks;
    std::vector<Eigen::Index> m_seg_first;
    std::vector<Eigen::Index> m_seg_total;

    void rebuild_kernel();

    const double* extend(
        std::span<const double> control_points,
        size_t dim,
        Eigen::Index& count);

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
