#pragma once

#include "MayaFlux/Kinesis/Differential.hpp"
#include "MayaFlux/Kinesis/HampelFilter.hpp"
#include "MayaFlux/Kinesis/Stochastic/Estimate.hpp"

namespace MayaFlux::Kinesis {

/**
 * @brief Number of scalar components Estimate must independently track for T
 *
 * Estimate is hard-typed to double: it has no notion of a vector. A
 * MotionChannel<glm::vec2> needs two independent Estimate instances (one
 * per axis), since each axis can have genuinely different noise
 * character (a tablet's X and Y sensors are not identical). This trait
 * makes that fan-out explicit rather than pretending Estimate is
 * generic when it is not.
 */
template <typename T>
struct estimate_component_count {
    static constexpr size_t value = 1;
};

template <glm::length_t L, typename U, glm::qualifier Q>
struct estimate_component_count<glm::vec<L, U, Q>> {
    static constexpr size_t value = static_cast<size_t>(L);
};

template <typename T>
inline constexpr size_t estimate_component_count_v = estimate_component_count<T>::value;

/**
 * @brief Assemble a scalar or vector T from N doubles
 *
 * Inverse of reading T's components. Plain arithmetic T constructs
 * directly from components[0]. glm vector T constructs component-wise.
 */
template <typename T>
[[nodiscard]] inline T assemble_from_components(const std::array<double, estimate_component_count_v<T>>& components) noexcept
{
    if constexpr (GlmType<T>) {
        using Comp = glm_component_type<T>;
        T result {};
        for (size_t i = 0; i < estimate_component_count_v<T>; ++i)
            result[static_cast<glm::length_t>(i)] = static_cast<Comp>(components[i]);
        return result;
    } else {
        return static_cast<T>(components[0]);
    }
}

/**
 * @brief Read T's components into an array of doubles
 */
template <typename T>
[[nodiscard]] inline std::array<double, estimate_component_count_v<T>> read_components(const T& value) noexcept
{
    std::array<double, estimate_component_count_v<T>> out {};
    if constexpr (GlmType<T>) {
        for (size_t i = 0; i < estimate_component_count_v<T>; ++i)
            out[i] = static_cast<double>(value[static_cast<glm::length_t>(i)]);
    } else {
        out[0] = static_cast<double>(value);
    }
    return out;
}

/**
 * @class MotionChannel
 * @brief Wires Estimate, Differential, and HampelFilter for one stream
 *        without hiding any of them.
 *
 * A caller tracking a raw stream through denoise -> differentiate ->
 * spike-screen has to make several real decisions: which EstimateModel
 * fits this stream's behavior, what window sizes, which derivative
 * orders (velocity, acceleration, ...) actually need Hampel screening
 * versus which can be trusted straight out of Differential. None of
 * those decisions have validated defaults; they depend on the specific
 * device and use case. MotionChannel does not guess them. It requires
 * the caller to configure each one explicitly through methods named
 * after the exact class and parameter being configured, then does the
 * mechanical wiring (per-component Estimate fan-out for vector T,
 * HistoryBuffer maintenance, routing each requested derivative order
 * through its configured filter or leaving it unfiltered) so the
 * caller is not hand-assembling that plumbing at every call site.
 *
 * Every piece MotionChannel owns is reachable: estimate_for_component(),
 * history(), filter_for_order() return direct references, so a caller
 * can inspect or override anything the builder configured rather than
 * trusting an opaque pipeline.
 *
 * @tparam T Sample type: double, glm::vec2, or glm::vec3.
 *
 * ```cpp
 * MotionChannel<glm::vec2> position(EstimateModel::QUIET_PERIOD_FLOOR, 8);
 * position.filter_order(2, 8, 3.5);  // Hampel-screen acceleration, not velocity
 *
 * for (auto raw : incoming_stream) {
 *     position.update(raw, dt);
 *     glm::vec2 vel = position.derivative<1>(dt);
 *     glm::vec2 acc = position.derivative<2>(dt);  // passes through the filter above
 * }
 * ```
 */
template <typename T>
class MotionChannel {
public:
    static constexpr size_t component_count = estimate_component_count_v<T>;

    /**
     * @brief Construct a channel
     * @param model EstimateModel applied identically to every component.
     *        A vec2's x and y each get their own Estimate instance with
     *        this model, not one shared instance, since components are
     *        independent streams.
     * @param window Window size forwarded to each Estimate
     * @param adapt_rate Adapt rate forwarded to each Estimate, ignored
     *        by models other than EWM_VARIANCE
     * @param history_capacity HistoryBuffer capacity; must be at least
     *        one more than the highest derivative order this channel
     *        will be asked to compute, per Differential's own
     *        capacity requirements
     */
    explicit MotionChannel(
        Stochastic::EstimateModel model,
        size_t window,
        double adapt_rate = 0.05,
        size_t history_capacity = 8)
        : m_history(history_capacity)
    {
        for (size_t i = 0; i < component_count; ++i)
            m_estimates.emplace_back(model, adapt_rate, window);
    }

    /**
     * @brief Feed one raw sample, denoise it, and push the cleaned value
     * @param raw_sample Raw value for this step
     * @param dt Elapsed time since the previous sample; stored for use
     *        by derivative<N>() and curvature() so the caller does not
     *        have to pass it again at every read
     */
    void update(const T& raw_sample, double dt)
    {
        m_last_dt = dt;
        const auto raw_components = read_components(raw_sample);
        std::array<double, component_count> clean_components {};
        for (size_t i = 0; i < component_count; ++i) {
            m_estimates[i].update(raw_components[i]);
            clean_components[i] = m_estimates[i].value();
        }
        m_history.push(assemble_from_components<T>(clean_components));
    }

    /**
     * @brief Compute the N-th derivative, routed through a configured
     *        HampelFilter for that order if one exists
     * @tparam N Derivative order, forwarded to Differential::backward_difference
     * @return The derivative, filtered if filter_order(N, ...) was called,
     *         unfiltered otherwise
     */
    template <size_t N>
    [[nodiscard]] T derivative()
    {
        const T raw = backward_difference<N>(m_history, m_last_dt);
        auto it = m_filters.find(N);
        if (it == m_filters.end())
            return raw;
        return it->second->accept(raw);
    }

    /**
     * @brief Configure Hampel screening for one derivative order
     * @param order Which derivative to filter (1 = velocity, 2 = acceleration, ...)
     * @param window Forwarded to HampelFilter
     * @param threshold_mad Forwarded to HampelFilter
     * @param max_consecutive_rejections Forwarded to HampelFilter
     *
     * Not called for every order by default: velocity is often usable
     * unfiltered, while acceleration and above amplify noise enough
     * that a spike guard is usually worth its added lag. This is the
     * caller's decision per order, not a channel-wide default.
     */
    void filter_order(size_t order, size_t window = 8, double threshold_mad = 3.5,
        size_t max_consecutive_rejections = 3)
    {
        m_filters[order] = std::make_unique<HampelFilter<T>>(window, threshold_mad, max_consecutive_rejections);
    }

    /**
     * @brief Direct access to one component's Estimate instance
     * @param component Index (0 for x/scalar, 1 for y, 2 for z)
     */
    [[nodiscard]] Stochastic::Estimate& estimate_for_component(size_t component)
    {
        return m_estimates[component];
    }

    /**
     * @brief Direct access to the underlying HistoryBuffer
     */
    [[nodiscard]] Memory::HistoryBuffer<T>& history() { return m_history; }

    /**
     * @brief Direct access to a configured filter, or nullptr if that
     *        order has no filter configured
     */
    [[nodiscard]] HampelFilter<T>* filter_for_order(size_t order)
    {
        auto it = m_filters.find(order);
        return it == m_filters.end() ? nullptr : it->second.get();
    }

    /**
     * @brief dt passed to the most recent update() call
     */
    [[nodiscard]] double last_dt() const { return m_last_dt; }

private:
    std::vector<Stochastic::Estimate> m_estimates;
    Memory::HistoryBuffer<T> m_history;
    std::map<size_t, std::unique_ptr<HampelFilter<T>>> m_filters;
    double m_last_dt { 0.0 };
};

/**
 * @brief Curvature helper for MotionChannel<glm::vec2>, self-calibrating
 *        its min_speed guard from the channel's own learned noise floor
 * @param channel A MotionChannel<glm::vec2> that has had at least one update()
 * @return curvature(velocity, acceleration, min_speed), where min_speed
 *         is derived from the combined per-axis Estimate floors divided
 *         by dt rather than a fixed constant
 *
 * A fixed min_speed constant has to assume a coordinate scale (pixels,
 * normalized 0..1, millimeters); this instead asks "how much speed
 * could this channel's own measurement noise alone produce", which
 * scales automatically with whatever device and coordinate range the
 * channel is actually receiving. Kept as a free function rather than a
 * MotionChannel method since it is glm::vec2-specific and curvature
 * itself does not generalize to vec3 the way velocity/acceleration do.
 */
[[nodiscard]] inline float channel_curvature(MotionChannel<glm::vec2>& channel)
{
    const glm::vec2 vel = channel.derivative<1>();
    const glm::vec2 accel = channel.derivative<2>();

    const glm::vec2 floor_vec {
        static_cast<float>(channel.estimate_for_component(0).floor()),
        static_cast<float>(channel.estimate_for_component(1).floor())
    };
    const auto dt = static_cast<float>(channel.last_dt());
    const float min_speed = (dt > 0.0F) ? (glm::length(floor_vec) / dt) : 0.0F;

    return curvature(vel, accel, min_speed);
}

} // namespace MayaFlux::Kinesis
