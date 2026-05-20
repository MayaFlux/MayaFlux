#pragma once

namespace MayaFlux::Portal::Forma::Plot {

/**
 * @struct AxisRange
 * @brief Scalar domain extent for one plot axis.
 *
 * Maps raw double values in [min, max] to the normalised render space
 * expected by the geometry function. When auto_scaling is true, min and max
 * are recomputed from the series data on every process() call, subject to
 * any predicate set via scale_if().
 *
 * Fluent setters return @c AxisRange& for chained construction:
 * @code
 * AxisRange{}.range(-1.F, 1.F)
 * AxisRange{}.auto_scale()
 * AxisRange{}.range(0.F, 440.F).auto_scale(false)
 * AxisRange{}.scale_if([&active]{ return active.load(); })
 * @endcode
 */
struct AxisRange {
    float min { -1.F };
    float max { 1.F };
    bool auto_scaling { false };

    /// @brief When set, evaluated before applying a data-derived range.
    ///        Returns true to accept the new range, false to keep the current one.
    std::function<bool()> scale_predicate;

    // =========================================================================
    // Fluent setters
    // =========================================================================

    /**
     * @brief Set the explicit [min, max] domain.
     *        Has no effect while auto_scaling is true.
     */
    AxisRange& range(float lo, float hi)
    {
        min = lo;
        max = hi;
        return *this;
    }

    /**
     * @brief Enable or disable auto-scaling.
     *
     * When enabled, min and max are recomputed from series data on every
     * process() call, subject to any predicate set via scale_if().
     */
    AxisRange& auto_scale(bool enabled = true)
    {
        auto_scaling = enabled;
        return *this;
    }

    /**
     * @brief Gate auto-scaling behind an arbitrary bool-returning callable.
     *
     * Any nullary callable returning bool is accepted: an atomic flag read,
     * a node output comparison, a MappedState check, a lambda closing over
     * external state. Implicitly enables auto_scaling.
     *
     * The predicate is evaluated once per process() call before the new
     * data-derived range is applied. Returning false keeps the current range.
     *
     * @code
     * // Only scale when signal exceeds a threshold:
     * AxisRange{}.scale_if([node]{ return node->get_last_output() > 0.5; });
     *
     * // Gate on an atomic flag set elsewhere:
     * AxisRange{}.scale_if([&active]{ return active.load(); });
     * @endcode
     */
    AxisRange& scale_if(std::function<bool()> predicate)
    {
        scale_predicate = std::move(predicate);
        auto_scaling = true;
        return *this;
    }

    // =========================================================================
    // Mapping
    // =========================================================================

    /** @brief Map a value into [0, 1] within this range. */
    [[nodiscard]] float normalise(float v) const noexcept
    {
        if (max == min)
            return 0.F;
        return (v - min) / (max - min);
    }

    /** @brief Map a value into [-1, 1] NDC within this range. */
    [[nodiscard]] float to_ndc(float v) const noexcept
    {
        return normalise(v) * 2.F - 1.F;
    }
};

} // namespace MayaFlux::Portal::Forma::Plot
