#pragma once

#include <Eigen/Core>

namespace MayaFlux::Kinesis {

namespace detail {

    /**
     * @brief Hermite ramp from 0 at @p edge0 to 1 at @p edge1
     *
     * The same curve as glm::smoothstep, confirmed against
     * ShaderCompat.hpp's re-export of it, kept as a local double-precision
     * copy rather than calling through glm::smoothstep directly: that one
     * is float-based, and every membership computation in this file works
     * in double so that two extents differing only slightly near a
     * boundary do not lose the difference to a float round-trip.
     */
    [[nodiscard]] inline double ramp(double edge0, double edge1, double x) noexcept
    {
        if (edge1 <= edge0)
            return x < edge0 ? 0.0 : 1.0;
        const double t = std::clamp((x - edge0) / (edge1 - edge0), 0.0, 1.0);
        return t * t * (3.0 - 2.0 * t);
    }

} // namespace detail

/**
 * @struct FeatureMetric
 * @brief Per-axis weighting and wrapping for a space whose axes carry
 *        different units.
 *
 * A feature space assembled from measured quantities has no natural
 * metric. Speed in normalized units per second, pressure in 0..1, and
 * an angle in radians are three incommensurate axes, and Euclidean
 * distance across them means whichever axis happens to have the largest
 * numeric range dominates every comparison. Weights fix that by scaling
 * each axis before the difference is taken.
 *
 * Periods handle the axes that wrap. An angle at 0.01 and an angle at
 * 6.27 are adjacent, not opposite, and no amount of weighting expresses
 * that. A non-zero period on an axis makes its difference take the
 * shorter way round; a zero period leaves it linear.
 *
 * distance_sq has the signature SpatialIndex expects for its DistanceFn,
 * so an index over a feature space can be constructed with this metric
 * directly rather than defaulting to unweighted Euclidean.
 *
 * Distinct from Tendency::DistanceMetric (Euclidean, Euclidean-squared,
 * Manhattan, Chebyshev), which is a fixed choice of norm over glm::vec3
 * specifically, with no per-axis weighting and no wrapping. That enum
 * answers "which norm" for a 3D world-space query. This answers "what
 * does one unit of difference mean on each axis of an assembled feature
 * space", which a fixed norm over three hardcoded axes cannot express.
 */
struct FeatureMetric {
    Eigen::VectorXd weights; ///< Per-axis scale applied before differencing. Empty means unit weights.
    Eigen::VectorXd periods; ///< Per-axis wrap period. Zero or empty means the axis is linear.

    /**
     * @brief Componentwise difference, wrapped on any axis with a period
     * @param a Left operand
     * @param b Right operand
     * @return a - b, with each wrapping axis taken the short way round
     */
    [[nodiscard]] Eigen::VectorXd delta(const Eigen::VectorXd& a, const Eigen::VectorXd& b) const
    {
        Eigen::VectorXd d = a - b;
        if (periods.size() == d.size()) {
            for (Eigen::Index i = 0; i < d.size(); ++i) {
                const double p = periods(i);
                if (p > 0.0) {
                    d(i) = std::remainder(d(i), p);
                }
            }
        }
        return d;
    }

    /**
     * @brief Weighted squared distance under this metric
     * @param a Left operand
     * @param b Right operand
     * @return Sum over axes of (weight * wrapped difference) squared
     */
    [[nodiscard]] float distance_sq(const Eigen::VectorXd& a, const Eigen::VectorXd& b) const
    {
        const Eigen::VectorXd d = delta(a, b);
        double acc = 0.0;
        for (Eigen::Index i = 0; i < d.size(); ++i) {
            const double w = (weights.size() == d.size()) ? weights(i) : 1.0;
            const double s = w * d(i);
            acc += s * s;
        }
        return static_cast<float>(acc);
    }

    /**
     * @brief A metric with unit weights and no wrapping
     * @param dimensions Axis count
     */
    [[nodiscard]] static FeatureMetric uniform(Eigen::Index dimensions)
    {
        return { .weights = Eigen::VectorXd::Ones(dimensions),
            .periods = Eigen::VectorXd::Zero(dimensions) };
    }
};

/**
 * @struct BoxExtent
 * @brief An axis-aligned interval per axis, with a soft outer margin.
 *
 * The conjunction case: this axis within these bounds, and that axis
 * within those bounds, and so on. Membership is governed by the axis
 * that is furthest out of range rather than by their product, so an
 * observation satisfying every axis but one does not accumulate a
 * misleadingly high score from the axes it does satisfy.
 *
 * Softness is what makes membership continuous. With softness zero the
 * extent is a hard test and membership is 0 or 1. With softness above
 * zero, membership falls from 1 at the boundary to 0 at boundary plus
 * softness, measured per axis in that axis's own units.
 */
struct BoxExtent {
    Eigen::VectorXd lower; ///< Inclusive lower bound per axis.
    Eigen::VectorXd upper; ///< Inclusive upper bound per axis.
    Eigen::VectorXd softness; ///< Outward margin per axis over which membership falls to zero. Empty means hard.
};

/**
 * @struct EllipsoidExtent
 * @brief A centre, a radius per axis, and a soft outer shell.
 *
 * The graded case: distance from a prototype rather than satisfaction
 * of independent bands. Radii are per axis so an ellipsoid derived from
 * observed spread is tighter on the axes that were consistent across
 * observations and looser on the ones that were not, which is the
 * behaviour a extent fitted from demonstrations should have.
 *
 * Axis-aligned rather than fully general: a full covariance ellipsoid
 * needs an eigendecomposition to test membership and correlated feature
 * axes are usually a sign that the axes were badly chosen rather than
 * something to model.
 *
 * The closer analog elsewhere is Nexus::Presence::FalloffFn, which also
 * maps a normalized distance to a weight. The two differ in what
 * "normalized distance" means and in whether an inside exists at all.
 * Presence radiates outward from a point in 3D world space with no hard
 * boundary, weight one at the centre falling toward zero at the falloff
 * radius. This has a genuine inside, membership exactly one for any
 * point within the per-axis radii, and softness only describes the
 * outer shell beyond that. A Presence has no analog of "fully inside".
 */
struct EllipsoidExtent {
    Eigen::VectorXd centre; ///< Prototype point.
    Eigen::VectorXd radii; ///< Per-axis extent at which normalized distance reaches one.
    double softness { 0.0 }; ///< Additional normalized distance over which membership falls to zero.
};

/**
 * @brief Graded membership of a point in a box extent
 * @param extent Extent to test against
 * @param point Observation, same dimension as the extent
 * @param metric Supplies wrapping; weights do not participate since the
 *        bounds are already stated in each axis's own units
 * @return 1.0 fully inside, 0.0 fully outside, intermediate within the
 *         soft margin, governed by the worst axis
 */
[[nodiscard]] inline double membership(
    const BoxExtent& extent,
    const Eigen::VectorXd& point,
    const FeatureMetric& metric)
{
    double worst = 1.0;
    for (Eigen::Index i = 0; i < point.size(); ++i) {
        const double centre = 0.5 * (extent.lower(i) + extent.upper(i));
        const double half = 0.5 * (extent.upper(i) - extent.lower(i));

        double offset = point(i) - centre;
        if (metric.periods.size() == point.size() && metric.periods(i) > 0.0)
            offset = std::remainder(offset, metric.periods(i));

        const double outside = std::abs(offset) - half;
        const double soft = (extent.softness.size() == point.size())
            ? extent.softness(i)
            : 0.0;

        const double axis = (outside <= 0.0)
            ? 1.0
            : 1.0 - detail::ramp(0.0, std::max(soft, 1e-12), outside);

        worst = std::min(worst, axis);
        if (worst <= 0.0)
            return 0.0;
    }
    return worst;
}

/**
 * @brief Graded membership of a point in an ellipsoid extent
 * @param extent Extent to test against
 * @param point Observation, same dimension as the extent
 * @param metric Supplies wrapping on the offset from the centre
 * @return 1.0 at normalized distance one or below, falling to 0.0 at
 *         one plus softness
 */
[[nodiscard]] inline double membership(
    const EllipsoidExtent& extent,
    const Eigen::VectorXd& point,
    const FeatureMetric& metric)
{
    const Eigen::VectorXd d = metric.delta(point, extent.centre);
    double acc = 0.0;
    for (Eigen::Index i = 0; i < d.size(); ++i) {
        const double r = (extent.radii(i) > 0.0) ? extent.radii(i) : 1e-12;
        const double s = d(i) / r;
        acc += s * s;
    }
    const double normalized = std::sqrt(acc);
    if (normalized <= 1.0)
        return 1.0;
    return 1.0 - detail::ramp(1.0, 1.0 + std::max(extent.softness, 1e-12), normalized);
}

/**
 * @brief Derive per-axis metric weights from the spread of a sample set
 * @param samples Observations, all of the same dimension
 * @param periods Per-axis wrap periods, or an empty vector for none
 * @return A metric whose weights are the reciprocal of each axis's
 *         standard deviation, so one standard deviation on any axis
 *         contributes equally to distance
 *
 * The answer to incommensurate units when a corpus is available: rather
 * than the caller guessing that pressure should count twice as much as
 * speed, the weights come from how much each axis actually varied across
 * the observations. An axis with no variation gets unit weight rather
 * than an infinite one.
 */
[[nodiscard]] inline FeatureMetric fit_metric(
    std::span<const Eigen::VectorXd> samples,
    const Eigen::VectorXd& periods = {})
{
    if (samples.empty())
        return {};

    const Eigen::Index n = samples.front().size();
    Eigen::VectorXd mean = Eigen::VectorXd::Zero(n);
    for (const auto& s : samples)
        mean += s;
    mean /= static_cast<double>(samples.size());

    Eigen::VectorXd var = Eigen::VectorXd::Zero(n);
    for (const auto& s : samples) {
        const Eigen::VectorXd d = s - mean;
        var += d.cwiseProduct(d);
    }
    var /= static_cast<double>(samples.size());

    Eigen::VectorXd weights(n);
    for (Eigen::Index i = 0; i < n; ++i) {
        const double sd = std::sqrt(var(i));
        weights(i) = (sd > 1e-12) ? (1.0 / sd) : 1.0;
    }

    return { .weights = weights,
        .periods = (periods.size() == n) ? periods : Eigen::VectorXd::Zero(n) };
}

/**
 * @brief Fit a box extent covering a central fraction of a sample set
 * @param samples Observations, all of the same dimension
 * @param coverage Fraction of observations the bounds should contain per
 *        axis, in 0..1. One takes the full min and max
 * @param softness_fraction Soft margin per axis, as a fraction of that
 *        axis's fitted width
 * @return Bounds at the symmetric quantiles implied by @p coverage
 *
 * Quantiles rather than min and max so one badly performed demonstration
 * does not widen the extent to include everything between it and the
 * others. A coverage of one recovers the min and max behaviour for
 * callers who want it.
 */
[[nodiscard]] inline BoxExtent fit_box(
    std::span<const Eigen::VectorXd> samples,
    double coverage = 0.9,
    double softness_fraction = 0.15)
{
    if (samples.empty())
        return {};

    const Eigen::Index n = samples.front().size();
    BoxExtent extent {
        .lower = Eigen::VectorXd::Zero(n),
        .upper = Eigen::VectorXd::Zero(n),
        .softness = Eigen::VectorXd::Zero(n)
    };

    const double tail = 0.5 * (1.0 - std::clamp(coverage, 0.0, 1.0));
    std::vector<double> axis;
    axis.reserve(samples.size());

    for (Eigen::Index i = 0; i < n; ++i) {
        axis.clear();
        for (const auto& s : samples)
            axis.push_back(s(i));
        std::ranges::sort(axis);

        const auto last = static_cast<double>(axis.size() - 1);
        const auto lo_idx = static_cast<size_t>(std::floor(tail * last));
        const auto hi_idx = static_cast<size_t>(std::ceil((1.0 - tail) * last));

        extent.lower(i) = axis[lo_idx];
        extent.upper(i) = axis[hi_idx];
        extent.softness(i) = std::max(
            (extent.upper(i) - extent.lower(i)) * softness_fraction, 1e-9);
    }

    return extent;
}

/**
 * @brief Fit an ellipsoid extent from the mean and spread of a sample set
 * @param samples Observations, all of the same dimension
 * @param radius_sigma Per-axis radius as a multiple of that axis's
 *        standard deviation
 * @param softness Additional normalized distance over which membership
 *        falls to zero beyond the fitted radius
 * @return Centre at the sample mean, radii at @p radius_sigma standard
 *         deviations
 *
 * The prototype form of a demonstrated meaning: the centre is what was
 * done on average and the radii are how much it varied, so an axis held
 * consistently across demonstrations constrains membership tightly while
 * one that wandered constrains it loosely.
 */
[[nodiscard]] inline EllipsoidExtent fit_ellipsoid(
    std::span<const Eigen::VectorXd> samples,
    double radius_sigma = 2.0,
    double softness = 0.5)
{
    if (samples.empty())
        return {};

    const Eigen::Index n = samples.front().size();
    Eigen::VectorXd mean = Eigen::VectorXd::Zero(n);
    for (const auto& s : samples)
        mean += s;
    mean /= static_cast<double>(samples.size());

    Eigen::VectorXd var = Eigen::VectorXd::Zero(n);
    for (const auto& s : samples) {
        const Eigen::VectorXd d = s - mean;
        var += d.cwiseProduct(d);
    }
    var /= static_cast<double>(samples.size());

    Eigen::VectorXd radii(n);
    for (Eigen::Index i = 0; i < n; ++i)
        radii(i) = std::max(radius_sigma * std::sqrt(var(i)), 1e-9);

    return { .centre = mean, .radii = radii, .softness = softness };
}

/**
 * @class ThresholdLatch
 * @brief Two-level hysteresis turning a continuous value into a held
 *        boolean.
 *
 * A single threshold on a noisy continuous membership produces a rapid
 * alternation whenever the value sits near it, which downstream reads as
 * many separate occurrences of one event. Two levels separate the point
 * at which the latch engages from the point at which it releases, so a
 * value hovering between them holds whatever state it last reached.
 *
 * The gap between the levels is the caller's decision and depends on how
 * noisy the incoming value is; there is no defensible default, so both
 * are required at construction.
 */
class ThresholdLatch {
public:
    /**
     * @brief Construct a latch
     * @param engage Value at or above which the latch turns on
     * @param release Value at or below which the latch turns off; should
     *        be below @p engage
     */
    ThresholdLatch(double engage, double release)
        : m_engage(engage)
        , m_release(release)
    {
    }

    /**
     * @brief Feed one value and report the resulting state
     * @param value Current continuous value
     * @return True if the latch is engaged after this observation
     */
    bool update(double value)
    {
        if (!m_engaged && value >= m_engage) {
            m_engaged = true;
        } else if (m_engaged && value <= m_release) {
            m_engaged = false;
        }
        return m_engaged;
    }

    /** @brief Current state without feeding a value. */
    [[nodiscard]] bool engaged() const { return m_engaged; }

    /** @brief Force the latch off, for use on a known discontinuity. */
    void reset() { m_engaged = false; }

private:
    double m_engage;
    double m_release;
    bool m_engaged { false };
};

} // namespace MayaFlux::Kinesis
