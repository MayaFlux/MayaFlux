#include "MotionCurves.hpp"

#include "MayaFlux/Kakshya/NDData/EigenAccess.hpp"
#include "MayaFlux/Kakshya/NDData/EigenInsertion.hpp"

#include "BasisMatrices.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Transitive/Parallel/Execution.hpp"

namespace P = MayaFlux::Parallel;

namespace MayaFlux::Kinesis {

namespace {

    /// @brief Samples evaluated per matrix product.
    constexpr Eigen::Index k_chunk_samples = 4096;

    /// @brief Below this sample count the chunk loop runs serially on evaluator scratch.
    constexpr Eigen::Index k_parallel_min_samples = 8192;

    /// @brief Below this point count the arc length pass runs serially.
    constexpr Eigen::Index k_parallel_min_points = 8192;

    Eigen::Index compute_num_segments(
        Eigen::Index num_controls,
        Eigen::Index points_per_segment,
        Eigen::Index overlap)
    {
        return (overlap == 0)
            ? num_controls / points_per_segment
            : (num_controls - overlap) / (points_per_segment - overlap);
    }

    /**
     * @brief Local curve parameter for one sample.
     * @param sample_idx Global sample index.
     * @param num_samples Total samples requested.
     * @param num_segments Segment count over the extended control set.
     */
    double local_t(Eigen::Index sample_idx, Eigen::Index num_samples, Eigen::Index num_segments)
    {
        const double t_global = static_cast<double>(sample_idx) / static_cast<double>(num_samples - 1);
        const double segment_float = t_global * static_cast<double>(num_segments);
        const auto seg_idx = static_cast<Eigen::Index>(std::floor(segment_float));

        if (sample_idx == num_samples - 1 || seg_idx >= num_segments) {
            return 1.0;
        }

        return segment_float - static_cast<double>(seg_idx);
    }

    /**
     * @brief Fill a pps x count monomial matrix, column j holding
     *        [t^(pps-1), ..., t, 1] for that sample.
     */
    void fill_monomials(
        Eigen::MatrixXd& weights,
        const CurveChunk& chunk,
        Eigen::Index pps,
        Eigen::Index num_samples,
        Eigen::Index num_segments)
    {
        for (Eigen::Index j = 0; j < chunk.sample_count; ++j) {
            const double t = chunk.clamp_t_high
                ? 1.0
                : local_t(chunk.sample_begin + j, num_samples, num_segments);

            double power = 1.0;
            for (Eigen::Index r = pps - 1; r >= 0; --r) {
                weights(r, j) = power;
                power *= t;
            }
        }
    }

    /**
     * @brief Fill a 2 x count matrix of cosine blend weights.
     */
    void fill_cosine(
        Eigen::MatrixXd& weights,
        const CurveChunk& chunk,
        Eigen::Index num_samples,
        Eigen::Index num_segments)
    {
        for (Eigen::Index j = 0; j < chunk.sample_count; ++j) {
            const double t = chunk.clamp_t_high
                ? 1.0
                : local_t(chunk.sample_begin + j, num_samples, num_segments);
            const double mu = (1.0 - std::cos(t * M_PI)) * 0.5;
            weights(0, j) = 1.0 - mu;
            weights(1, j) = mu;
        }
    }

} // namespace

// ===========================================================================
// Single-sample entry points
// ===========================================================================

Eigen::VectorXd catmull_rom_spline(
    const Eigen::MatrixXd& control_points,
    double t,
    double tension)
{
    if (control_points.cols() != 4) {
        error<std::invalid_argument>(
            Journal::Component::Kinesis,
            Journal::Context::Runtime,
            std::source_location::current(),
            "Catmull-Rom interpolation requires 4 control points, but got {}",
            control_points.cols());
    }

    Eigen::Matrix4d basis_matrix = BasisMatrices::catmull_rom_with_tension(tension);
    Eigen::Vector4d t_vector(t * t * t, t * t, t, 1.0);
    Eigen::Vector4d coeffs = basis_matrix * t_vector;

    return control_points * coeffs;
}

Eigen::VectorXd cubic_bezier(
    const Eigen::MatrixXd& control_points,
    double t)
{
    if (control_points.cols() != 4) {
        error<std::invalid_argument>(
            Journal::Component::Kinesis,
            Journal::Context::Runtime,
            std::source_location::current(),
            "Cubic Bezier interpolation requires 4 control points, but got {}",
            control_points.cols());
    }

    Eigen::Vector4d t_vector(t * t * t, t * t, t, 1.0);
    Eigen::Vector4d coeffs = BasisMatrices::CUBIC_BEZIER * t_vector;

    return control_points * coeffs;
}

Eigen::VectorXd quadratic_bezier(
    const Eigen::MatrixXd& control_points,
    double t)
{
    if (control_points.cols() != 3) {
        error<std::invalid_argument>(
            Journal::Component::Kinesis,
            Journal::Context::Runtime,
            std::source_location::current(),
            "Quadratic Bezier interpolation requires 3 control points, but got {}",
            control_points.cols());
    }

    Eigen::Vector3d t_vector(t * t, t, 1.0);
    Eigen::Vector3d coeffs = BasisMatrices::QUADRATIC_BEZIER * t_vector;

    return control_points * coeffs;
}

Eigen::VectorXd cubic_hermite(
    const Eigen::MatrixXd& endpoints,
    const Eigen::MatrixXd& tangents,
    double t)
{
    if (endpoints.cols() != 2 || tangents.cols() != 2) {
        error<std::invalid_argument>(
            Journal::Component::Kinesis,
            Journal::Context::Runtime,
            std::source_location::current(),
            "Cubic Hermite interpolation requires 2 endpoints and 2 tangents, but got {} endpoints and {} tangents",
            endpoints.cols(), tangents.cols());
    }

    double t2 = t * t;
    double t3 = t2 * t;

    double h00 = 2.0 * t3 - 3.0 * t2 + 1.0;
    double h10 = t3 - 2.0 * t2 + t;
    double h01 = -2.0 * t3 + 3.0 * t2;
    double h11 = t3 - t2;

    return h00 * endpoints.col(0) + h10 * tangents.col(0) + h01 * endpoints.col(1) + h11 * tangents.col(1);
}

Eigen::VectorXd bspline_cubic(
    const Eigen::MatrixXd& control_points,
    double t)
{
    if (control_points.cols() != 4) {
        error<std::invalid_argument>(
            Journal::Component::Kinesis,
            Journal::Context::Runtime,
            std::source_location::current(),
            "Cubic B-spline interpolation requires 4 control points, but got {}",
            control_points.cols());
    }

    Eigen::Vector4d t_vector(t * t * t, t * t, t, 1.0);
    Eigen::Vector4d coeffs = BasisMatrices::BSPLINE_CUBIC * t_vector;

    return control_points * coeffs;
}

Eigen::VectorXd interpolate(
    const Eigen::MatrixXd& control_points,
    double t,
    InterpolationMode mode,
    double tension)
{
    switch (mode) {
    case InterpolationMode::LINEAR:
        if (control_points.cols() < 2) {
            error<std::invalid_argument>(
                Journal::Component::Kinesis,
                Journal::Context::Runtime,
                std::source_location::current(),
                "Linear interpolation requires at least 2 points, but got {}",
                control_points.cols());
        }
        return (1.0 - t) * control_points.col(0) + t * control_points.col(1);

    case InterpolationMode::CATMULL_ROM:
        return catmull_rom_spline(control_points, t, tension);

    case InterpolationMode::CUBIC_HERMITE: {
        Eigen::MatrixXd endpoints = control_points.leftCols(2);
        Eigen::MatrixXd tangents = control_points.rightCols(2);
        return cubic_hermite(endpoints, tangents, t);
    }

    case InterpolationMode::CUBIC_BEZIER:
        return cubic_bezier(control_points, t);

    case InterpolationMode::QUADRATIC_BEZIER:
        return quadratic_bezier(control_points, t);

    case InterpolationMode::BSPLINE:
        return bspline_cubic(control_points, t);

    case InterpolationMode::COSINE: {
        if (control_points.cols() < 2) {
            error<std::invalid_argument>(
                Journal::Component::Kinesis,
                Journal::Context::Runtime,
                std::source_location::current(),
                "Cosine interpolation requires at least 2 points, but got {}",
                control_points.cols());
        }
        double mu2 = (1.0 - std::cos(t * M_PI)) * 0.5;
        return (1.0 - mu2) * control_points.col(0) + mu2 * control_points.col(1);
    }

    default:
        error<std::invalid_argument>(
            Journal::Component::Kinesis,
            Journal::Context::Runtime,
            std::source_location::current(),
            "Unsupported interpolation mode: {}",
            static_cast<int>(mode));
    }
}

// ===========================================================================
// CurveEvaluator
// ===========================================================================

CurveEvaluator::CurveEvaluator(InterpolationMode mode, double tension)
    : m_mode(mode)
    , m_tension(tension)
{
    rebuild_kernel();
}

void CurveEvaluator::configure(InterpolationMode mode, double tension)
{
    if (mode == m_mode && tension == m_tension) {
        return;
    }

    m_mode = mode;
    m_tension = tension;
    rebuild_kernel();
}

void CurveEvaluator::rebuild_kernel()
{
    switch (m_mode) {
    case InterpolationMode::LINEAR:
        m_points_per_segment = 2;
        m_overlap = 1;
        m_supports_multi = true;
        m_trigonometric = false;
        m_basis.resize(2, 2);
        m_basis << -1.0, 1.0,
            1.0, 0.0;
        break;

    case InterpolationMode::COSINE:
        m_points_per_segment = 2;
        m_overlap = 1;
        m_supports_multi = true;
        m_trigonometric = true;
        m_basis.resize(0, 0);
        break;

    case InterpolationMode::CATMULL_ROM:
        m_points_per_segment = 4;
        m_overlap = 3;
        m_supports_multi = true;
        m_trigonometric = false;
        m_basis = BasisMatrices::catmull_rom_with_tension(m_tension);
        break;

    case InterpolationMode::BSPLINE:
        m_points_per_segment = 4;
        m_overlap = 3;
        m_supports_multi = true;
        m_trigonometric = false;
        m_basis = BasisMatrices::BSPLINE_CUBIC;
        break;

    case InterpolationMode::CUBIC_BEZIER:
        m_points_per_segment = 4;
        m_overlap = 1;
        m_supports_multi = true;
        m_trigonometric = false;
        m_basis = BasisMatrices::CUBIC_BEZIER;
        break;

    case InterpolationMode::QUADRATIC_BEZIER:
        m_points_per_segment = 3;
        m_overlap = 1;
        m_supports_multi = true;
        m_trigonometric = false;
        m_basis = BasisMatrices::QUADRATIC_BEZIER;
        break;

    case InterpolationMode::CUBIC_HERMITE:
        m_points_per_segment = 4;
        m_overlap = 0;
        m_supports_multi = false;
        m_trigonometric = false;
        m_basis.resize(4, 4);
        m_basis << 2.0, -3.0, 0.0, 1.0,
            -2.0, 3.0, 0.0, 0.0,
            1.0, -2.0, 1.0, 0.0,
            1.0, -1.0, 0.0, 0.0;
        break;

    default:
        m_points_per_segment = 0;
        m_overlap = 0;
        m_supports_multi = false;
        m_trigonometric = false;
        m_basis.resize(0, 0);
        break;
    }
}

const Eigen::MatrixXd* CurveEvaluator::extend(
    const Eigen::MatrixXd& control_points,
    Eigen::Index& count)
{
    count = control_points.cols();

    const bool pads = (m_mode == InterpolationMode::CATMULL_ROM
                          || m_mode == InterpolationMode::BSPLINE)
        && count > m_points_per_segment;

    if (!pads) {
        return &control_points;
    }

    m_extended.resize(control_points.rows(), count + 2);
    m_extended.col(0) = control_points.col(0);
    m_extended.col(count + 1) = control_points.col(count - 1);
    m_extended.middleCols(1, count) = control_points;

    count = count + 2;
    return &m_extended;
}

void CurveEvaluator::build_chunks(
    Eigen::Index num_samples,
    Eigen::Index num_segments,
    Eigen::Index active_count)
{
    const auto segments = static_cast<size_t>(num_segments);

    m_seg_first.assign(segments, -1);
    m_seg_total.assign(segments, 0);

    for (Eigen::Index i = 0; i < num_samples; ++i) {
        const double t_global = static_cast<double>(i) / static_cast<double>(num_samples - 1);
        const double segment_float = t_global * static_cast<double>(num_segments);
        auto seg_idx = static_cast<Eigen::Index>(std::floor(segment_float));

        if (i == num_samples - 1 || seg_idx >= num_segments) {
            seg_idx = num_segments - 1;
        }

        const auto s = static_cast<size_t>(seg_idx);
        if (m_seg_first[s] < 0) {
            m_seg_first[s] = i;
        }
        ++m_seg_total[s];
    }

    m_chunks.clear();

    for (Eigen::Index seg_idx = 0; seg_idx < num_segments; ++seg_idx) {
        const auto s = static_cast<size_t>(seg_idx);
        if (m_seg_total[s] == 0) {
            continue;
        }

        Eigen::Index start_col = seg_idx * (m_points_per_segment - m_overlap);
        const bool clamp = (start_col + m_points_per_segment > active_count);
        if (clamp) {
            start_col = active_count - m_points_per_segment;
        }

        for (Eigen::Index off = 0; off < m_seg_total[s]; off += k_chunk_samples) {
            m_chunks.push_back({ .start_col = start_col,
                .sample_begin = m_seg_first[s] + off,
                .sample_count = std::min(k_chunk_samples, m_seg_total[s] - off),
                .clamp_t_high = clamp });
        }
    }
}

void CurveEvaluator::evaluate(
    const Eigen::MatrixXd& control_points,
    Eigen::Index num_samples,
    Eigen::MatrixXd& out)
{
    if (num_samples < 2) {
        error<std::invalid_argument>(
            Journal::Component::Kinesis,
            Journal::Context::Runtime,
            std::source_location::current(),
            "num_samples must be at least 2, but got {}",
            num_samples);
    }

    if (control_points.cols() < 2) {
        error<std::invalid_argument>(
            Journal::Component::Kinesis,
            Journal::Context::Runtime,
            std::source_location::current(),
            "Need at least 2 control points, but got {}",
            control_points.cols());
    }

    if (m_points_per_segment == 0) {
        error<std::invalid_argument>(
            Journal::Component::Kinesis,
            Journal::Context::Runtime,
            std::source_location::current(),
            "Unsupported interpolation mode: {}",
            static_cast<int>(m_mode));
    }

    if (!m_supports_multi && control_points.cols() != m_points_per_segment) {
        error<std::invalid_argument>(
            Journal::Component::Kinesis,
            Journal::Context::Runtime,
            std::source_location::current(),
            "{} interpolation requires exactly {} control points, but got {}",
            static_cast<int>(m_mode), m_points_per_segment, control_points.cols());
    }

    Eigen::Index active_count = 0;
    const Eigen::MatrixXd* active = extend(control_points, active_count);

    const Eigen::Index num_segments = compute_num_segments(
        active_count, m_points_per_segment, m_overlap);

    if (num_segments < 1) {
        error<std::invalid_argument>(
            Journal::Component::Kinesis,
            Journal::Context::Runtime,
            std::source_location::current(),
            "Need sufficient control points for multi-segment {} interpolation, but got {}",
            static_cast<int>(m_mode), control_points.cols());
    }

    out.resize(control_points.rows(), num_samples);

    build_chunks(num_samples, num_segments, active_count);

    const Eigen::Index pps = m_points_per_segment;
    const bool trig = m_trigonometric;

    if (m_chunks.size() > 1 && num_samples >= k_parallel_min_samples) {
        const Eigen::MatrixXd& basis = m_basis;

        P::for_each(P::par_unseq, m_chunks.begin(), m_chunks.end(),
            [&](const CurveChunk& chunk) {
                Eigen::MatrixXd weights(pps, chunk.sample_count);

                if (trig) {
                    fill_cosine(weights, chunk, num_samples, num_segments);
                    out.middleCols(chunk.sample_begin, chunk.sample_count).noalias()
                        = active->middleCols(chunk.start_col, pps) * weights;
                    return;
                }

                fill_monomials(weights, chunk, pps, num_samples, num_segments);

                const Eigen::MatrixXd folded = active->middleCols(chunk.start_col, pps) * basis;

                out.middleCols(chunk.sample_begin, chunk.sample_count).noalias()
                    = folded * weights;
            });

        return;
    }

    for (const CurveChunk& chunk : m_chunks) {
        m_weights.resize(pps, chunk.sample_count);

        if (trig) {
            fill_cosine(m_weights, chunk, num_samples, num_segments);
            out.middleCols(chunk.sample_begin, chunk.sample_count).noalias()
                = active->middleCols(chunk.start_col, pps) * m_weights;
            continue;
        }

        fill_monomials(m_weights, chunk, pps, num_samples, num_segments);

        m_folded.noalias() = active->middleCols(chunk.start_col, pps) * m_basis;

        out.middleCols(chunk.sample_begin, chunk.sample_count).noalias()
            = m_folded * m_weights;
    }
}

void CurveEvaluator::reparameterize(
    const Eigen::MatrixXd& points,
    Eigen::Index num_samples,
    Eigen::MatrixXd& out)
{
    const Eigen::Index n = points.cols();

    if (n < 2 || num_samples < 2) {
        out = points;
        return;
    }

    m_arc.resize(n);
    m_arc(0) = 0.0;

    if (n >= k_parallel_min_points) {
        P::for_each(P::par_unseq,
            std::views::iota(Eigen::Index { 1 }, n).begin(),
            std::views::iota(Eigen::Index { 1 }, n).end(),
            [&](Eigen::Index i) {
                m_arc(i) = (points.col(i) - points.col(i - 1)).norm();
            });
    } else {
        for (Eigen::Index i = 1; i < n; ++i) {
            m_arc(i) = (points.col(i) - points.col(i - 1)).norm();
        }
    }

    std::inclusive_scan(m_arc.data() + 1, m_arc.data() + n, m_arc.data() + 1);

    const double total_length = m_arc(n - 1);
    if (total_length == 0.0) {
        out = points;
        return;
    }

    out.resize(points.rows(), num_samples);

    const double step = total_length / static_cast<double>(num_samples - 1);
    Eigen::Index upper = 1;

    for (Eigen::Index i = 0; i < num_samples; ++i) {
        const double target = static_cast<double>(i) * step;

        while (upper < n - 1 && m_arc(upper) < target) {
            ++upper;
        }

        const Eigen::Index lower = upper - 1;
        const double span = m_arc(upper) - m_arc(lower);
        const double t = (span > 0.0) ? ((target - m_arc(lower)) / span) : 0.0;

        out.col(i) = (1.0 - t) * points.col(lower) + t * points.col(upper);
    }
}

// ===========================================================================
// Free functions
// ===========================================================================

Eigen::MatrixXd generate_interpolated_points(
    const Eigen::MatrixXd& control_points,
    Eigen::Index num_samples,
    InterpolationMode mode,
    double tension)
{
    CurveEvaluator evaluator(mode, tension);
    Eigen::MatrixXd result;
    evaluator.evaluate(control_points, num_samples, result);
    return result;
}

double compute_arc_length(const Eigen::MatrixXd& points)
{
    if (points.cols() < 2) {
        return 0.0;
    }

    double length = 0.0;
    for (Eigen::Index i = 1; i < points.cols(); ++i) {
        length += (points.col(i) - points.col(i - 1)).norm();
    }

    return length;
}

Eigen::VectorXd compute_arc_length_table(const Eigen::MatrixXd& points)
{
    const Eigen::Index n = points.cols();

    Eigen::VectorXd arc_lengths(n);
    if (n == 0) {
        return arc_lengths;
    }

    arc_lengths(0) = 0.0;
    if (n == 1) {
        return arc_lengths;
    }

    if (n >= k_parallel_min_points) {
        P::for_each(P::par_unseq,
            std::views::iota(Eigen::Index { 1 }, n).begin(),
            std::views::iota(Eigen::Index { 1 }, n).end(),
            [&](Eigen::Index i) {
                arc_lengths(i) = (points.col(i) - points.col(i - 1)).norm();
            });
    } else {
        for (Eigen::Index i = 1; i < n; ++i) {
            arc_lengths(i) = (points.col(i) - points.col(i - 1)).norm();
        }
    }

    std::inclusive_scan(
        arc_lengths.data() + 1,
        arc_lengths.data() + n,
        arc_lengths.data() + 1);

    return arc_lengths;
}

Eigen::MatrixXd reparameterize_by_arc_length(
    const Eigen::MatrixXd& points,
    Eigen::Index num_samples)
{
    CurveEvaluator evaluator;
    Eigen::MatrixXd result;
    evaluator.reparameterize(points, num_samples, result);
    return result;
}

Kakshya::DataVariant interpolate_nddata(
    const Kakshya::DataVariant& control_points,
    Eigen::Index num_samples,
    InterpolationMode mode,
    double tension)
{
    Eigen::MatrixXd control_matrix = Kakshya::to_eigen_matrix(control_points);

    Eigen::MatrixXd interpolated = generate_interpolated_points(
        control_matrix, num_samples, mode, tension);

    Kakshya::EigenAccess input_access(control_points);

    if (input_access.is_complex()) {
        return Kakshya::from_eigen_matrix(interpolated, Kakshya::MatrixInterpretation::COMPLEX);
    }

    if (input_access.is_structured()) {
        switch (input_access.component_count()) {
        case 2:
            return Kakshya::from_eigen_matrix(interpolated, Kakshya::MatrixInterpretation::VEC2);
        case 3:
            return Kakshya::from_eigen_matrix(interpolated, Kakshya::MatrixInterpretation::VEC3);
        case 4:
            return Kakshya::from_eigen_matrix(interpolated, Kakshya::MatrixInterpretation::VEC4);
        default:
            return Kakshya::from_eigen_matrix(interpolated, Kakshya::MatrixInterpretation::SCALAR);
        }
    } else {
        return Kakshya::from_eigen_matrix(interpolated, Kakshya::MatrixInterpretation::SCALAR);
    }
}

} // namespace MayaFlux::Kinesis
