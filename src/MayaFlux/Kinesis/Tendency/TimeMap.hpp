#pragma once

#include "Tendency.hpp"

#include "MayaFlux/Kinesis/Scalar.hpp"

namespace MayaFlux::Kinesis::TimeMaps {

/**
 * @file TimeMap.hpp
 * @brief Factories for TimeMap: seconds since a voice started in, position in
 *        recorded material out (frames for a sample stream).
 *
 * Doubles keep sub-frame precision on long recordings. Positions outside the
 * material are allowed; the reader decides what they mean. For a fixed
 * position use constant<double, double>(position).
 */

/**
 * @brief Straight path from one position to another, then constant
 * @param from Position at time zero
 * @param to Position reached at @p duration and kept after
 * @param duration Seconds to travel, zero or less gives @p to at every time
 */
inline TimeMap linear(double from, double to, double duration)
{
    return { .fn = [from, to, duration](const double& t) -> double {
        if (duration <= 0.0) {
            return to;
        }
        return map_clamped<double>(t, 0.0, duration, from, to);
    } };
}

/**
 * @brief Constant acceleration from an initial velocity
 * @param start Position at time zero
 * @param velocity Initial velocity in position units per second, negative reverses
 * @param acceleration Change of velocity per second, negative decelerates
 *
 * Position is start + velocity * t + acceleration * t^2 / 2 for t >= 0.
 */
inline TimeMap quadratic(double start, double velocity, double acceleration)
{
    return { .fn = [start, velocity, acceleration](const double& t) -> double {
        const double s = std::max(t, 0.0);
        return start + velocity * s + 0.5 * acceleration * s * s;
    } };
}

/**
 * @brief Triangle wave between two positions
 * @param lo Lower turning point
 * @param hi Upper turning point
 * @param velocity Speed in position units per second, negative starts downward
 * @param start Position at time zero, folded into [lo, hi]
 */
inline TimeMap triangle(double lo, double hi, double velocity, double start)
{
    return { .fn = [lo, hi, velocity, start](const double& t) -> double {
        return ping_pong<double>(start + velocity * t, lo, hi);
    } };
}

/**
 * @brief Position of a moving reference minus a lag that can itself change
 * @param origin Reference position at time zero, for a recording its write head
 * @param rate Reference velocity in position units per second, the write rate
 * @param lag Distance behind the reference at each time, in position units
 *
 * Reading a ring that is still being written gives a tap that trails the
 * write head. Keep the lag above a couple of blocks so the tap stays behind
 * the frames being written.
 */
inline TimeMap lagged(double origin, double rate, TimeMap lag)
{
    return { .fn = [origin, rate, lag = std::move(lag)](const double& t) -> double {
        return origin + rate * t - lag(t);
    } };
}

/**
 * @brief Piecewise linear path through evenly spaced positions
 * @param points Positions spread across [0, duration], for example a sampled
 *        Kinesis curve. Empty gives zero, one gives a constant.
 * @param duration Seconds from the first point to the last, last point kept after
 */
inline TimeMap piecewise_linear(std::vector<double> points, double duration)
{
    return { .fn = [points = std::move(points), duration](const double& t) -> double {
        if (points.empty()) {
            return 0.0;
        }
        if (points.size() == 1 || duration <= 0.0) {
            return points.back();
        }

        const double x = std::clamp(t / duration, 0.0, 1.0) * static_cast<double>(points.size() - 1);
        const auto i = std::min(static_cast<size_t>(x), points.size() - 2);
        const double f = x - static_cast<double>(i);
        return points[i] + (points[i + 1] - points[i]) * f;
    } };
}

} // namespace MayaFlux::Kinesis::TimeMaps
