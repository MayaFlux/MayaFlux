#include "MotionCurves.hpp"

#ifdef MAYAFLUX_ARCH_X64
#include <immintrin.h>
#endif
#ifdef MAYAFLUX_ARCH_ARM64
#include <arm_neon.h>
#endif

#include "MayaFlux/Kakshya/NDData/EigenAccess.hpp"
#include "MayaFlux/Kakshya/NDData/EigenInsertion.hpp"

#include "BasisMatrices.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Transitive/Parallel/Execution.hpp"

namespace P = MayaFlux::Parallel;

namespace MayaFlux::Kinesis {

namespace {

    /// @brief Samples evaluated per chunk.
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
     * @brief Copy a column-major Eigen basis into a row-major flat buffer.
     * @param source pps x pps matrix, weight of control q at row q.
     * @param pps Points per segment.
     * @param dst Resized to pps * pps, indexed [q * pps + p].
     */
    void flatten_basis(const double* source, Eigen::Index pps, std::vector<double>& dst)
    {
        dst.resize(static_cast<size_t>(pps * pps));
        for (Eigen::Index q = 0; q < pps; ++q) {
            for (Eigen::Index p = 0; p < pps; ++p) {
                dst[static_cast<size_t>(q * pps + p)] = source[q + p * pps];
            }
        }
    }

    /**
     * @brief Fold the basis into a segment's control block.
     * @param ctrl Point-major control storage.
     * @param start_col First control point of the segment.
     * @param dim Coordinate count.
     * @param pps Points per segment.
     * @param basis Row-major pps * pps basis.
     * @param folded Output, dim * pps, indexed [d * pps + p].
     *
     * folded(d,p) = sum over q of control(d,q) * basis(q,p), so the per-sample
     * cost collapses to one Horner evaluation per coordinate.
     */
    void fold_controls(
        const double* ctrl,
        Eigen::Index start_col,
        size_t dim,
        Eigen::Index pps,
        const double* basis,
        double* folded)
    {
        const double* block = ctrl + static_cast<size_t>(start_col) * dim;

        for (size_t d = 0; d < dim; ++d) {
            for (Eigen::Index p = 0; p < pps; ++p) {
                double sum = 0.0;
                for (Eigen::Index q = 0; q < pps; ++q) {
                    sum += block[static_cast<size_t>(q) * dim + d]
                        * basis[static_cast<size_t>(q * pps + p)];
                }
                folded[d * static_cast<size_t>(pps) + static_cast<size_t>(p)] = sum;
            }
        }
    }

    /**
     * @brief Fill the local curve parameter for every sample in a chunk.
     * @param dst Buffer of chunk.sample_count doubles.
     *
     * Every sample in a chunk shares the same segment by construction, so the
     * floor is loop invariant and the parameter is affine in the sample index.
     * Division by the sample span is kept rather than folded into a reciprocal
     * multiply so the values match the scalar formulation exactly.
     */
    void fill_parameters(
        double* dst,
        const CurveChunk& chunk,
        Eigen::Index num_samples,
        Eigen::Index num_segments)
    {
        const auto count = static_cast<size_t>(chunk.sample_count);

        if (chunk.clamp_t_high) {
            std::fill_n(dst, count, 1.0);
            return;
        }

        const double delta = static_cast<double>(num_segments)
            / static_cast<double>(num_samples - 1);
        const double origin = -static_cast<double>(chunk.segment);
        const auto begin = static_cast<double>(chunk.sample_begin);

        size_t j = 0;

#ifdef MAYAFLUX_ARCH_X64
        const __m256d v_delta = _mm256_set1_pd(delta);
        const __m256d v_origin = _mm256_set1_pd(origin);
        const __m256d v_step = _mm256_set1_pd(4.0);
        __m256d idx = _mm256_set_pd(begin + 3.0, begin + 2.0, begin + 1.0, begin);

        for (; j + 4 <= count; j += 4) {
            _mm256_storeu_pd(dst + j, _mm256_fmadd_pd(idx, v_delta, v_origin));
            idx = _mm256_add_pd(idx, v_step);
        }
#elif defined(MAYAFLUX_ARCH_ARM64)
        const float64x2_t v_delta = vdupq_n_f64(delta);
        const float64x2_t v_step = vdupq_n_f64(2.0);
        float64x2_t idx = { begin, begin + 1.0 };

        for (; j + 2 <= count; j += 2) {
            vst1q_f64(dst + j, vfmaq_f64(vdupq_n_f64(origin), idx, v_delta));
            idx = vaddq_f64(idx, v_step);
        }
#endif

        for (; j < count; ++j) {
            dst[j] = (begin + static_cast<double>(j)) * delta + origin;
        }

        if (chunk.sample_begin + chunk.sample_count == num_samples) {
            dst[count - 1] = 1.0;
        }
    }

    /**
     * @brief Horner evaluation of one chunk for every coordinate.
     * @tparam Pps Points per segment, giving the polynomial degree Pps - 1.
     * @param folded dim * Pps folded control block.
     * @param params chunk parameter buffer.
     * @param dim Coordinate count.
     * @param count Sample count.
     * @param dst First output sample of coordinate 0.
     * @param stride Distance between coordinate rows in the output.
     *
     * The parameter occupies the SIMD lanes and the coefficients broadcast,
     * so four consecutive samples of one coordinate are produced per vector
     * and stored contiguously.
     */
    template <size_t Pps>
    void evaluate_rows(
        const double* folded,
        const double* params,
        size_t dim,
        size_t count,
        double* dst,
        size_t stride)
    {
        for (size_t d = 0; d < dim; ++d) {
            const double* co = folded + d * Pps;
            double* row = dst + d * stride;

            size_t j = 0;

#ifdef MAYAFLUX_ARCH_X64
            __m256d cv[Pps];
            for (size_t p = 0; p < Pps; ++p) {
                cv[p] = _mm256_set1_pd(co[p]);
            }

            for (; j + 4 <= count; j += 4) {
                const __m256d tv = _mm256_loadu_pd(params + j);
                __m256d acc = cv[0];
                for (size_t p = 1; p < Pps; ++p) {
                    acc = _mm256_fmadd_pd(acc, tv, cv[p]);
                }
                _mm256_storeu_pd(row + j, acc);
            }
#elif defined(MAYAFLUX_ARCH_ARM64)
            float64x2_t cv[Pps];
            for (size_t p = 0; p < Pps; ++p) {
                cv[p] = vdupq_n_f64(co[p]);
            }

            for (; j + 2 <= count; j += 2) {
                const float64x2_t tv = vld1q_f64(params + j);
                float64x2_t acc = cv[0];
                for (size_t p = 1; p < Pps; ++p) {
                    acc = vfmaq_f64(cv[p], acc, tv);
                }
                vst1q_f64(row + j, acc);
            }
#endif

            for (; j < count; ++j) {
                double acc = co[0];
                for (size_t p = 1; p < Pps; ++p) {
                    acc = acc * params[j] + co[p];
                }
                row[j] = acc;
            }
        }
    }

    void evaluate_chunk_polynomial(
        const double* folded,
        const double* params,
        size_t dim,
        Eigen::Index pps,
        size_t count,
        double* dst,
        size_t stride)
    {
        switch (pps) {
        case 2:
            evaluate_rows<2>(folded, params, dim, count, dst, stride);
            break;
        case 3:
            evaluate_rows<3>(folded, params, dim, count, dst, stride);
            break;
        case 4:
            evaluate_rows<4>(folded, params, dim, count, dst, stride);
            break;
        default:
            break;
        }
    }

    /**
     * @brief Cosine blend of a two point segment.
     *
     * Not polynomial in the parameter, so it takes the scalar path. AVX2
     * carries no transcendental, matching the treatment of the trigonometric
     * maps in Discrete/Transform.
     */
    void evaluate_chunk_cosine(
        const double* ctrl,
        Eigen::Index start_col,
        const double* params,
        size_t dim,
        size_t count,
        double* dst,
        size_t stride)
    {
        const double* c0 = ctrl + static_cast<size_t>(start_col) * dim;
        const double* c1 = c0 + dim;

        for (size_t d = 0; d < dim; ++d) {
            double* row = dst + d * stride;
            const double a = c0[d];
            const double b = c1[d];

            for (size_t j = 0; j < count; ++j) {
                const double mu = (1.0 - std::cos(params[j] * M_PI)) * 0.5;
                row[j] = (1.0 - mu) * a + mu * b;
            }
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
    m_trigonometric = false;

    switch (m_mode) {
    case InterpolationMode::LINEAR: {
        m_points_per_segment = 2;
        m_overlap = 1;
        m_supports_multi = true;
        m_basis = { -1.0, 1.0, 1.0, 0.0 };
        break;
    }

    case InterpolationMode::COSINE:
        m_points_per_segment = 2;
        m_overlap = 1;
        m_supports_multi = true;
        m_trigonometric = true;
        m_basis.clear();
        break;

    case InterpolationMode::CATMULL_ROM: {
        m_points_per_segment = 4;
        m_overlap = 3;
        m_supports_multi = true;
        const Eigen::Matrix4d m = BasisMatrices::catmull_rom_with_tension(m_tension);
        flatten_basis(m.data(), 4, m_basis);
        break;
    }

    case InterpolationMode::BSPLINE:
        m_points_per_segment = 4;
        m_overlap = 3;
        m_supports_multi = true;
        flatten_basis(BasisMatrices::BSPLINE_CUBIC.data(), 4, m_basis);
        break;

    case InterpolationMode::CUBIC_BEZIER:
        m_points_per_segment = 4;
        m_overlap = 1;
        m_supports_multi = true;
        flatten_basis(BasisMatrices::CUBIC_BEZIER.data(), 4, m_basis);
        break;

    case InterpolationMode::QUADRATIC_BEZIER:
        m_points_per_segment = 3;
        m_overlap = 1;
        m_supports_multi = true;
        flatten_basis(BasisMatrices::QUADRATIC_BEZIER.data(), 3, m_basis);
        break;

    case InterpolationMode::CUBIC_HERMITE:
        m_points_per_segment = 4;
        m_overlap = 0;
        m_supports_multi = false;
        m_basis = {
            2.0, -3.0, 0.0, 1.0,
            -2.0, 3.0, 0.0, 0.0,
            1.0, -2.0, 1.0, 0.0,
            1.0, -1.0, 0.0, 0.0
        };
        break;

    default:
        m_points_per_segment = 0;
        m_overlap = 0;
        m_supports_multi = false;
        m_basis.clear();
        break;
    }
}

const double* CurveEvaluator::extend(
    std::span<const double> control_points,
    size_t dim,
    Eigen::Index& count)
{
    count = static_cast<Eigen::Index>(control_points.size() / dim);

    const bool pads = (m_mode == InterpolationMode::CATMULL_ROM
                          || m_mode == InterpolationMode::BSPLINE)
        && count > m_points_per_segment;

    if (!pads) {
        return control_points.data();
    }

    const auto n = static_cast<size_t>(count);
    m_extended.resize((n + 2) * dim);

    const double* src = control_points.data();
    double* dst = m_extended.data();

    std::copy_n(src, dim, dst);
    std::copy_n(src, n * dim, dst + dim);
    std::copy_n(src + (n - 1) * dim, dim, dst + (n + 1) * dim);

    count = static_cast<Eigen::Index>(n + 2);
    return dst;
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
            m_chunks.push_back({ .segment = seg_idx,
                .start_col = start_col,
                .sample_begin = m_seg_first[s] + off,
                .sample_count = std::min(k_chunk_samples, m_seg_total[s] - off),
                .clamp_t_high = clamp });
        }
    }
}

void CurveEvaluator::evaluate_planar(
    std::span<const double> control_points,
    size_t dim,
    Eigen::Index num_samples,
    std::vector<double>& out)
{
    if (num_samples < 2) {
        error<std::invalid_argument>(
            Journal::Component::Kinesis,
            Journal::Context::Runtime,
            std::source_location::current(),
            "num_samples must be at least 2, but got {}",
            num_samples);
    }

    if (dim == 0) {
        error<std::invalid_argument>(
            Journal::Component::Kinesis,
            Journal::Context::Runtime,
            std::source_location::current(),
            "dim must be at least 1");
    }

    const auto control_count = static_cast<Eigen::Index>(control_points.size() / dim);

    if (control_count < 2) {
        error<std::invalid_argument>(
            Journal::Component::Kinesis,
            Journal::Context::Runtime,
            std::source_location::current(),
            "Need at least 2 control points, but got {}",
            control_count);
    }

    if (m_points_per_segment == 0) {
        error<std::invalid_argument>(
            Journal::Component::Kinesis,
            Journal::Context::Runtime,
            std::source_location::current(),
            "Unsupported interpolation mode: {}",
            static_cast<int>(m_mode));
    }

    if (!m_supports_multi && control_count != m_points_per_segment) {
        error<std::invalid_argument>(
            Journal::Component::Kinesis,
            Journal::Context::Runtime,
            std::source_location::current(),
            "{} interpolation requires exactly {} control points, but got {}",
            static_cast<int>(m_mode), m_points_per_segment, control_count);
    }

    Eigen::Index active_count = 0;
    const double* active = extend(control_points, dim, active_count);

    const Eigen::Index num_segments = compute_num_segments(
        active_count, m_points_per_segment, m_overlap);

    if (num_segments < 1) {
        error<std::invalid_argument>(
            Journal::Component::Kinesis,
            Journal::Context::Runtime,
            std::source_location::current(),
            "Need sufficient control points for multi-segment {} interpolation, but got {}",
            static_cast<int>(m_mode), control_count);
    }

    const auto stride = static_cast<size_t>(num_samples);
    out.resize(dim * stride);

    build_chunks(num_samples, num_segments, active_count);

    const Eigen::Index pps = m_points_per_segment;
    const bool trig = m_trigonometric;
    const double* basis = m_basis.data();
    double* out_base = out.data();

    if (m_chunks.size() > 1 && num_samples >= k_parallel_min_samples) {
        P::for_each(P::par_unseq, m_chunks.begin(), m_chunks.end(),
            [&](const CurveChunk& chunk) {
                const auto count = static_cast<size_t>(chunk.sample_count);
                std::vector<double> params(count);
                fill_parameters(params.data(), chunk, num_samples, num_segments);

                double* dst = out_base + static_cast<size_t>(chunk.sample_begin);

                if (trig) {
                    evaluate_chunk_cosine(active, chunk.start_col, params.data(),
                        dim, count, dst, stride);
                    return;
                }

                std::vector<double> folded(dim * static_cast<size_t>(pps));
                fold_controls(active, chunk.start_col, dim, pps, basis, folded.data());

                evaluate_chunk_polynomial(folded.data(), params.data(),
                    dim, pps, count, dst, stride);
            });

        return;
    }

    for (const CurveChunk& chunk : m_chunks) {
        const auto count = static_cast<size_t>(chunk.sample_count);

        m_tbuf.resize(count);
        fill_parameters(m_tbuf.data(), chunk, num_samples, num_segments);

        double* dst = out_base + static_cast<size_t>(chunk.sample_begin);

        if (trig) {
            evaluate_chunk_cosine(active, chunk.start_col, m_tbuf.data(),
                dim, count, dst, stride);
            continue;
        }

        m_folded.resize(dim * static_cast<size_t>(pps));
        fold_controls(active, chunk.start_col, dim, pps, basis, m_folded.data());

        evaluate_chunk_polynomial(m_folded.data(), m_tbuf.data(),
            dim, pps, count, dst, stride);
    }
}

void CurveEvaluator::reparameterize_planar(
    std::span<const double> points,
    size_t dim,
    Eigen::Index point_count,
    Eigen::Index num_samples,
    std::vector<double>& out)
{
    const auto n = static_cast<size_t>(point_count);
    const auto samples = static_cast<size_t>(num_samples);

    if (point_count < 2 || num_samples < 2) {
        out.assign(points.begin(), points.end());
        return;
    }

    const double* src = points.data();

    m_arc.assign(n, 0.0);

    for (size_t d = 0; d < dim; ++d) {
        const double* row = src + d * n;
        for (size_t i = 1; i < n; ++i) {
            const double delta = row[i] - row[i - 1];
            m_arc[i] += delta * delta;
        }
    }

    for (size_t i = 1; i < n; ++i) {
        m_arc[i] = std::sqrt(m_arc[i]);
    }

    std::inclusive_scan(m_arc.data() + 1, m_arc.data() + n, m_arc.data() + 1);

    const double total_length = m_arc[n - 1];
    if (total_length == 0.0) {
        out.assign(points.begin(), points.end());
        return;
    }

    m_lower.resize(samples);
    m_frac.resize(samples);

    const double step = total_length / static_cast<double>(num_samples - 1);
    size_t upper = 1;

    for (size_t i = 0; i < samples; ++i) {
        const double target = static_cast<double>(i) * step;

        while (upper < n - 1 && m_arc[upper] < target) {
            ++upper;
        }

        const size_t lower = upper - 1;
        const double span = m_arc[upper] - m_arc[lower];

        m_lower[i] = lower;
        m_frac[i] = (span > 0.0) ? ((target - m_arc[lower]) / span) : 0.0;
    }

    out.resize(dim * samples);

    for (size_t d = 0; d < dim; ++d) {
        const double* row = src + d * n;
        double* dst = out.data() + d * samples;

        for (size_t i = 0; i < samples; ++i) {
            const size_t lower = m_lower[i];
            const double t = m_frac[i];
            dst[i] = (1.0 - t) * row[lower] + t * row[lower + 1];
        }
    }
}

void CurveEvaluator::evaluate(
    const Eigen::MatrixXd& control_points,
    Eigen::Index num_samples,
    Eigen::MatrixXd& out)
{
    const auto dim = static_cast<size_t>(control_points.rows());

    evaluate_planar(
        std::span<const double>(control_points.data(),
            static_cast<size_t>(control_points.size())),
        dim, num_samples, m_planar);

    out.resize(control_points.rows(), num_samples);

    const auto samples = static_cast<size_t>(num_samples);
    double* dst = out.data();

    for (size_t d = 0; d < dim; ++d) {
        const double* row = m_planar.data() + d * samples;
        for (size_t i = 0; i < samples; ++i) {
            dst[d + i * dim] = row[i];
        }
    }
}

void CurveEvaluator::reparameterize(
    const Eigen::MatrixXd& points,
    Eigen::Index num_samples,
    Eigen::MatrixXd& out)
{
    const auto dim = static_cast<size_t>(points.rows());
    const auto n = static_cast<size_t>(points.cols());

    m_planar.resize(dim * n);
    const double* src = points.data();

    for (size_t d = 0; d < dim; ++d) {
        double* row = m_planar.data() + d * n;
        for (size_t i = 0; i < n; ++i) {
            row[i] = src[d + i * dim];
        }
    }

    reparameterize_planar(m_planar, dim, points.cols(), num_samples, m_planar_alt);

    const auto samples = m_planar_alt.size() / std::max<size_t>(dim, 1);
    out.resize(points.rows(), static_cast<Eigen::Index>(samples));

    double* dst = out.data();
    for (size_t d = 0; d < dim; ++d) {
        const double* row = m_planar_alt.data() + d * samples;
        for (size_t i = 0; i < samples; ++i) {
            dst[d + i * dim] = row[i];
        }
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
