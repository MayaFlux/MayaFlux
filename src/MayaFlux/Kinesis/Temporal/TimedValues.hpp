#pragma once

#include "MayaFlux/Kinesis/Tendency/Tendency.hpp"
#include "MayaFlux/Kinesis/Temporal/TimeSampling.hpp"

#include <glm/gtc/quaternion.hpp>

namespace MayaFlux::Kinesis {

/**
 * @enum KeyInterpolation
 * @brief How the segment that starts at a key reaches the next key.
 *
 * Step holds the key. Linear blends, by spherical linear interpolation for a
 * rotation. CubicSpline follows the Hermite form of glTF, with an incoming and an
 * outgoing tangent stored per key. Only Step is available to a value type that
 * cannot be blended.
 */
enum class KeyInterpolation : uint8_t {
    Step,
    Linear,
    CubicSpline
};

/**
 * @enum ValueKind
 * @brief Whether a vec4 value is a rotation stored as a quaternion in x, y, z, w order.
 */
enum class ValueKind : uint8_t {
    Plain,
    Rotation
};

/**
 * @enum TimeLimit
 * @brief Which side of a key a sample at exactly that time answers for.
 *
 * Before gives the value an instant earlier, which differs from At only where
 * the segment before the key is a Step, the same left-hand limit USD calls PreTime.
 */
enum class TimeLimit : uint8_t {
    At,
    Before
};

namespace detail {

    template <typename T>
    struct ScalarOf {
        using type = T;
    };

    template <glm::length_t L, typename Q, glm::qualifier P>
    struct ScalarOf<glm::vec<L, Q, P>> {
        using type = Q;
    };

    template <typename T>
    using ScalarOfT = typename ScalarOf<T>::type;

    template <typename T>
    inline constexpr bool is_blendable_v = std::same_as<T, float> || std::same_as<T, double>
        || std::same_as<T, glm::vec2> || std::same_as<T, glm::vec3> || std::same_as<T, glm::vec4>;

    template <typename T>
    bool is_finite_value(const T& value) noexcept
    {
        if constexpr (std::is_floating_point_v<T>) {
            return std::isfinite(value);
        } else if constexpr (is_blendable_v<T>) {
            for (glm::length_t i = 0; i < value.length(); ++i) {
                if (!std::isfinite(value[i])) {
                    return false;
                }
            }
            return true;
        } else {
            return true;
        }
    }

}

/**
 * @class TimedValues
 * @brief Values keyed to the times of a TimeSampling, sampled at any time.
 *
 * The data of an animation channel, a sampled transform, a parameter, anything
 * that is a function of time given by keys. It is immutable once created, so
 * any number of threads can sample it at once.
 *
 * A key's interpolation applies from that key to the next. A time exactly on a
 * key returns the stored value untouched. A time outside the keys follows an
 * OutOfRange policy, and Absent leaves the choice of a rest value to the sink.
 *
 * The value type is any copyable type. float, double and glm vec2, vec3 and vec4
 * can be blended. Anything else is held, so every key must then be Step.
 * A vec4 marked as a Rotation is a unit quaternion in x, y, z, w order: Linear
 * becomes spherical linear interpolation along the short way, a cubic result is
 * renormalised, and extrapolation holds.
 */
template <typename T>
class TimedValues {
public:
    /**
     * @struct Keys
     * @brief The inputs of create().
     *
     * modes is empty to give every key default_mode, or holds one entry per key.
     * Tangents are given together, one per key, and are required whenever any key
     * is CubicSpline.
     */
    struct Keys {
        TimeSampling times;
        std::vector<T> values;
        std::vector<KeyInterpolation> modes;
        std::vector<T> in_tangents;
        std::vector<T> out_tangents;
        KeyInterpolation default_mode { KeyInterpolation::Linear };
        ValueKind kind { ValueKind::Plain };
    };

    /**
     * @brief Validate and take ownership of the keys.
     *
     * Rotation keys are normalised. A blendable value must be finite.
     *
     * @return nullopt for no values, an acyclic sampling whose count differs from the
     * values, a mode list of the wrong size, a mode a non-blendable type cannot
     * use, a cubic key without a full set of tangents, a non-finite value or
     * tangent, a Rotation of a type other than vec4, or a Rotation of zero length.
     */
    [[nodiscard]] static std::optional<TimedValues> create(Keys keys)
    {
        const size_t count = keys.values.size();
        if (count == 0 || !keys.times.time(count - 1)) {
            return std::nullopt;
        }
        if (const auto stored = keys.times.stored_count(); stored && *stored != count) {
            return std::nullopt;
        }
        if (!keys.modes.empty() && keys.modes.size() != count) {
            return std::nullopt;
        }
        if (keys.in_tangents.size() != keys.out_tangents.size()
            || (!keys.in_tangents.empty() && keys.in_tangents.size() != count)) {
            return std::nullopt;
        }

        bool any_cubic = false;
        for (size_t k = 0; k < count; ++k) {
            const KeyInterpolation mode = keys.modes.empty() ? keys.default_mode : keys.modes[k];
            if (!detail::is_blendable_v<T> && mode != KeyInterpolation::Step) {
                return std::nullopt;
            }
            any_cubic = any_cubic || mode == KeyInterpolation::CubicSpline;
        }
        if (any_cubic && keys.in_tangents.empty()) {
            return std::nullopt;
        }

        if constexpr (detail::is_blendable_v<T>) {
            for (const auto* list : { &keys.values, &keys.in_tangents, &keys.out_tangents }) {
                if (!std::ranges::all_of(*list, [](const T& v) { return detail::is_finite_value(v); })) {
                    return std::nullopt;
                }
            }
        }

        if (keys.kind == ValueKind::Rotation) {
            if constexpr (std::same_as<T, glm::vec4>) {
                for (auto& value : keys.values) {
                    const float length = glm::length(value);
                    if (!(length > 0.0F) || !std::isfinite(length)) {
                        return std::nullopt;
                    }
                    value /= length;
                }
            } else {
                return std::nullopt;
            }
        }

        TimedValues out;
        out.m_times = std::move(keys.times);
        out.m_values = std::move(keys.values);
        out.m_modes = std::move(keys.modes);
        out.m_in = std::move(keys.in_tangents);
        out.m_out = std::move(keys.out_tangents);
        out.m_default = keys.default_mode;
        out.m_kind = keys.kind;
        return out;
    }

    [[nodiscard]] size_t size() const noexcept { return m_values.size(); }
    [[nodiscard]] const std::vector<T>& values() const noexcept { return m_values; }
    [[nodiscard]] const TimeSampling& sampling() const noexcept { return m_times; }
    [[nodiscard]] ValueKind kind() const noexcept { return m_kind; }

    /** @brief Time of the first key. */
    [[nodiscard]] double first_time() const noexcept { return *m_times.time(0); }

    /** @brief Time of the last key. */
    [[nodiscard]] double last_time() const noexcept { return *m_times.time(m_values.size() - 1); }

    /** @brief Last key time minus first, 0 for a single key. */
    [[nodiscard]] double duration() const noexcept { return last_time() - first_time(); }

    /**
     * @brief The value at a time.
     * @param t Seconds. A time that is not finite answers nothing.
     * @param policy What to answer outside the keys.
     * @param limit Which side of an exact key to answer for.
     * @return nullopt for a time that is not finite, or outside the keys under Absent.
     */
    [[nodiscard]] std::optional<T> sample(
        double t, OutOfRange policy = OutOfRange::Hold, TimeLimit limit = TimeLimit::At) const
    {
        const auto where = m_times.locate(t, m_values.size());
        if (!where) {
            return std::nullopt;
        }
        if (where->placement == TimePlacement::Within) {
            return within(*where, limit);
        }
        return outside_value(t, *where, policy);
    }

    /**
     * @brief The keys as a function of time for fields and Nexus.
     *
     * A Tendency cannot be absent, so Absent holds, and a time that is not finite
     * gives the first value.
     */
    [[nodiscard]] Tendency<double, T> as_tendency(OutOfRange policy = OutOfRange::Hold) const
    {
        if (policy == OutOfRange::Absent) {
            policy = OutOfRange::Hold;
        }
        auto self = std::make_shared<const TimedValues>(*this);
        return { .fn = [self, policy](const double& t) -> T {
            const auto value = self->sample(t, policy);
            return value ? *value : self->m_values.front();
        } };
    }

private:
    TimedValues() = default;

    [[nodiscard]] KeyInterpolation mode_of(size_t key) const noexcept
    {
        return m_modes.empty() ? m_default : m_modes[key];
    }

    [[nodiscard]] double time_at(size_t key) const noexcept { return *m_times.time(key); }

    [[nodiscard]] T within(const TimeLocation& where, TimeLimit limit) const
    {
        if (where.lower == where.upper) {
            if (limit == TimeLimit::Before && where.lower > 0
                && mode_of(where.lower - 1) == KeyInterpolation::Step) {
                return m_values[where.lower - 1];
            }
            return m_values[where.lower];
        }
        return blend(where.lower, where.upper, where.fraction);
    }

    [[nodiscard]] T blend(size_t lower, size_t upper, double fraction) const
    {
        if constexpr (detail::is_blendable_v<T>) {
            switch (mode_of(lower)) {
            case KeyInterpolation::Step:
                return m_values[lower];
            case KeyInterpolation::Linear:
                if constexpr (std::same_as<T, glm::vec4>) {
                    if (m_kind == ValueKind::Rotation) {
                        return spherical(m_values[lower], m_values[upper], fraction);
                    }
                }
                return linear(m_values[lower], m_values[upper], fraction);
            case KeyInterpolation::CubicSpline:
                return hermite(lower, upper, fraction);
            }
        }
        return m_values[lower];
    }

    [[nodiscard]] std::optional<T> outside_value(double t, const TimeLocation& where, OutOfRange policy) const
    {
        const size_t count = m_values.size();
        const bool after = where.placement == TimePlacement::After;

        switch (policy) {
        case OutOfRange::Absent:
            return std::nullopt;
        case OutOfRange::Hold:
            return after ? m_values[count - 1] : m_values[0];
        case OutOfRange::Repeat:
        case OutOfRange::PingPong:
        case OutOfRange::RepeatOffset: {
            const double begin = first_time();
            const double end = last_time();
            if (count == 1 || !(end > begin)) {
                return m_values[0];
            }
            const auto folded = fold_time(t, begin, end,
                policy == OutOfRange::PingPong ? TimeWrap::PingPong : TimeWrap::Repeat);
            if (!folded) {
                return std::nullopt;
            }
            const auto inside = m_times.locate(folded->time, count);
            if (!inside) {
                return std::nullopt;
            }
            T value = within(*inside, TimeLimit::At);
            if (policy == OutOfRange::RepeatOffset) {
                if constexpr (detail::is_blendable_v<T>) {
                    if (m_kind == ValueKind::Plain) {
                        using S = detail::ScalarOfT<T>;
                        value = value + (m_values[count - 1] - m_values[0]) * static_cast<S>(static_cast<double>(folded->cycles));
                    }
                }
            }
            return value;
        }
        case OutOfRange::Extrapolate:
            return extrapolated(t, after);
        }
        return std::nullopt;
    }

    [[nodiscard]] T extrapolated(double t, bool after) const
    {
        const size_t count = m_values.size();
        const T& edge = after ? m_values[count - 1] : m_values[0];
        if constexpr (detail::is_blendable_v<T>) {
            if (count < 2 || m_kind != ValueKind::Plain) {
                return edge;
            }
            const size_t near_key = after ? count - 1 : 0;
            const size_t far_key = after ? count - 2 : 1;
            const size_t segment = after ? count - 2 : 0;
            if (mode_of(segment) == KeyInterpolation::Step) {
                return edge;
            }
            using S = detail::ScalarOfT<T>;
            const double span = time_at(near_key) - time_at(far_key);
            const T slope = (m_values[near_key] - m_values[far_key]) / static_cast<S>(span);
            return edge + slope * static_cast<S>(t - time_at(near_key));
        } else {
            return edge;
        }
    }

    [[nodiscard]] static T linear(const T& a, const T& b, double fraction)
    {
        using S = detail::ScalarOfT<T>;
        return a + (b - a) * static_cast<S>(fraction);
    }

    [[nodiscard]] static glm::vec4 spherical(const glm::vec4& a, const glm::vec4& b, double fraction)
    {
        const glm::quat from(a.w, a.x, a.y, a.z);
        const glm::quat to(b.w, b.x, b.y, b.z);
        const glm::quat result = glm::normalize(glm::slerp(from, to, static_cast<float>(fraction)));
        return { result.x, result.y, result.z, result.w };
    }

    [[nodiscard]] T hermite(size_t lower, size_t upper, double fraction) const
    {
        using S = detail::ScalarOfT<T>;
        const S span = static_cast<S>(time_at(upper) - time_at(lower));
        const S t = static_cast<S>(fraction);
        const S t2 = t * t;
        const S t3 = t2 * t;

        T result = m_values[lower] * (S { 2 } * t3 - S { 3 } * t2 + S { 1 })
            + m_out[lower] * (span * (t3 - S { 2 } * t2 + t))
            + m_values[upper] * (S { -2 } * t3 + S { 3 } * t2)
            + m_in[upper] * (span * (t3 - t2));

        if constexpr (std::same_as<T, glm::vec4>) {
            if (m_kind == ValueKind::Rotation) {
                const float length = glm::length(result);
                if (length > 0.0F) {
                    result /= length;
                }
            }
        }
        return result;
    }

    TimeSampling m_times { *TimeSampling::uniform(1.0) };
    std::vector<T> m_values;
    std::vector<KeyInterpolation> m_modes;
    std::vector<T> m_in;
    std::vector<T> m_out;
    KeyInterpolation m_default { KeyInterpolation::Linear };
    ValueKind m_kind { ValueKind::Plain };
};

}
