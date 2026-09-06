#pragma once

#include "FieldSource.hpp"
#include "ShaderCompat.hpp"
#include "Tendency.hpp"

#include <type_traits>

namespace MayaFlux::Kinesis {

namespace detail {

    /**
     * @struct FieldSignature
     * @brief Extracts domain and range types from a non-generic lambda.
     *
     * Only const-qualified single-argument call operators are matched, which is
     * the shape MF_FIELD accepts. A mutable lambda, a multi-argument lambda or a
     * generic lambda produces no specialisation and fails against the incomplete
     * primary template.
     */
    template <typename T>
    struct FieldSignature : FieldSignature<decltype(&T::operator())> { };

    template <typename C, typename R, typename A>
    struct FieldSignature<R (C::*)(A) const> {
        using domain = std::decay_t<A>;
        using range = R;
    };

    template <typename C, typename R, typename A, typename B>
    struct FieldSignature<R (C::*)(A, B) const> {
        using domain = std::decay_t<A>;
        using range = R;
        using aux = std::decay_t<B>;
    };

} // namespace detail

/**
 * @struct DualField
 * @brief One authored expression carried as both a host callable and shader text.
 * @tparam D Domain type.
 * @tparam R Range type.
 *
 * The cpu member is an ordinary Tendency and is accepted anywhere a Tendency is,
 * including FieldOperator::bind. It composes with combine, chain, scale, clamp,
 * threshold, invert, lerp and select as any Tendency does. Those free functions
 * build closures, so a composed Tendency has no shader half: composition on the
 * GPU is textual and happens in the kernel body, where several emitted functions
 * are called with whatever arithmetic is wanted.
 *
 * Field bodies must be authored outside namespace Kinesis. See the authoring
 * rules in ShaderCompat.hpp.
 *
 * @code
 * namespace MayaFlux::Fields {
 * using namespace MayaFlux::ShaderCompat;
 *
 * const auto swirl = MF_FIELD(swirl, [](vec3 p) -> vec3 {
 *     return cross(vec3(0.0f, 1.0f, 0.0f), p) * 2.0f;
 * });
 * }
 *
 * field_op->bind(FieldTarget::POSITION, Fields::swirl.cpu);
 * @endcode
 */
template <typename D, typename R>
struct DualField {
    Tendency<D, R> cpu;
    FieldSource source;

    /**
     * @brief Evaluate the host half.
     */
    R operator()(const D& d) const { return cpu.fn(d); }

    /**
     * @brief Whether the shader half parsed into a usable function definition.
     */
    [[nodiscard]] bool has_source() const noexcept { return source.valid(); }
};

using DualScalarField = DualField<float, float>;
using DualSpatialField = DualField<glm::vec3, float>;
using DualVectorField = DualField<glm::vec3, glm::vec3>;
using DualUVField = DualField<glm::vec3, glm::vec2>;

/**
 * @struct TemporalField
 * @brief An authored expression of position and time, shader-only.
 * @tparam D Domain type.
 * @tparam R Range type.
 *
 * Carries no Tendency. Tendency<D, R> is a function of the domain alone, so a
 * two-argument callable has no CPU half to hand to FieldOperator: the value
 * depends on when it is asked, which a stateless Tendency cannot express.
 *
 * The second argument is supplied by whoever dispatches the shader. For
 * VertexFieldProcessor it is seconds since the processor attached.
 *
 * @code
 * namespace MayaFlux::Fields {
 * using namespace MayaFlux::ShaderCompat;
 *
 * const auto breathe = MF_FIELD_T(breathe, [](vec3 p, float t) -> vec3 {
 *     float d = length(p);
 *     if (d < 0.001f) { return vec3(0.0f); }
 *     return (p / d) * (sin(d * 2.5f - t * 2.0f) * 0.25f);
 * });
 * }
 * @endcode
 */
template <typename D, typename R>
struct TemporalField {
    FieldSource source;

    /**
     * @brief Whether the shader half parsed into a usable function definition.
     */
    [[nodiscard]] bool has_source() const noexcept { return source.valid(); }
};

using TemporalSpatialField = TemporalField<glm::vec3, float>;
using TemporalVectorField = TemporalField<glm::vec3, glm::vec3>;
using TemporalUVField = TemporalField<glm::vec3, glm::vec2>;

/**
 * @brief Build a DualField from a lambda and its stringified text.
 * @param name Name the emitted GLSL function will carry.
 * @param fn   The lambda itself, compiled by the host.
 * @param text The same lambda stringified, parsed into GLSL.
 *
 * Domain and range are deduced from the lambda's call operator, so the returned
 * type follows the authored signature rather than being spelled at the call
 * site. Prefer MF_FIELD: this function cannot check that fn and text correspond,
 * and passing a mismatched pair produces a field whose halves disagree silently.
 */
template <typename Lambda>
[[nodiscard]] auto dual_field(std::string name, Lambda&& fn, std::string_view text)
{
    using Sig = detail::FieldSignature<std::decay_t<Lambda>>;
    using D = typename Sig::domain;
    using R = typename Sig::range;

    return DualField<D, R> {
        .cpu = Tendency<D, R> { .fn = std::forward<Lambda>(fn) },
        .source = FieldSource::parse(std::move(name), text),
    };
}

/**
 * @brief Build a TemporalField from a lambda and its stringified text.
 *
 * The auxiliary parameter must be float. Domain and range are deduced from the
 * call operator, so the returned type follows the authored signature. Prefer
 * MF_FIELD_T: this cannot check that fn and text correspond.
 */
template <typename Lambda>
[[nodiscard]] auto temporal_field(std::string name, Lambda&&, std::string_view text)
{
    using Sig = detail::FieldSignature<std::decay_t<Lambda>>;
    using D = typename Sig::domain;
    using R = typename Sig::range;

    static_assert(std::is_same_v<typename Sig::aux, float>,
        "MF_FIELD_T: the second parameter must be float");

    return TemporalField<D, R> {
        .source = FieldSource::parse(std::move(name), text),
    };
}

} // namespace MayaFlux::Kinesis

/**
 * @brief Bind one authored expression to both the host and shader backends.
 *
 * The lambda is compiled by the host compiler and its text is parsed into a GLSL
 * function definition. Both halves come from the same characters and cannot
 * drift.
 *
 * The lambda must carry a trailing return type, take its argument by value, and
 * stay inside the intersection documented in ShaderCompat.hpp.
 *
 * @code
 * const auto falloff = MF_FIELD(falloff, [](vec3 p) -> float {
 *     float d = length(p) / 3.0f;
 *     return 1.0f - smoothstep(0.2f, 0.9f, clamp(d, 0.0f, 1.0f));
 * });
 * @endcode
 */
#define MF_FIELD(name, lambda) \
    MayaFlux::Kinesis::dual_field(#name, lambda, #lambda)

/**
 * @brief Bind an authored expression of position and time to the shader backend.
 *
 * Same authoring rules as MF_FIELD, with one addition: the lambda takes a
 * second float parameter and has no CPU half. Use MF_FIELD when the expression
 * depends on position alone, so it remains bindable to FieldOperator.
 */
#define MF_FIELD_T(name, lambda) \
    MayaFlux::Kinesis::temporal_field(#name, lambda, #lambda)
