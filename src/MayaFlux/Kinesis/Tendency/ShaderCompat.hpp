#pragma once

#include <glm/glm.hpp>

#include <cstdint>

/**
 * @file ShaderCompat.hpp
 * @brief GLSL type and function spellings made valid as C++.
 *
 * A dual-source field body is authored once and compiled twice: by the host
 * compiler into a callable, and by shaderc into SPIR-V. GLSL has no namespace
 * qualification, so the body must be written unqualified, and this header makes
 * unqualified lookup succeed on the host side.
 *
 * Nothing is reimplemented. glm already mirrors the GLSL function set. Vector
 * arguments would resolve through ADL without any declaration here, since a
 * glm::vec3 argument brings namespace glm into the lookup set. Scalar arguments
 * would not: a float has no associated namespace. The declarations below are
 * therefore load-bearing for expressions such as clamp(d, 0.0f, 1.0f) and
 * max(d * d, 0.1f), and the type aliases are load-bearing because vec3 and uint
 * have no glm spelling that is also GLSL.
 *
 * Authoring rules for any body that must survive both compilers:
 *
 *   1. Do not author inside namespace MayaFlux::Kinesis or any namespace nested
 *      within it. A using-directive injects these names into the nearest
 *      namespace enclosing both, which is MayaFlux, so Kinesis::clamp and
 *      Kinesis::smoothstep hide the glm overloads rather than competing with
 *      them and the call fails to resolve. Author in MayaFlux::Fields or
 *      another namespace outside Kinesis; the resulting .cpu halves compose
 *      with the Kinesis factories through qualification as normal.
 *   2. Every decimal literal carries an f suffix. GLSL reads 0.1 as float; C++
 *      reads it as double, and glm's scalar overloads are templates that fail
 *      deduction on mixed types. GLSL 4.60 accepts the suffix, so 0.1f is valid
 *      in both.
 *   3. No // comments. Stringification collapses the body to one line and a
 *      line comment would swallow the remainder.
 *   4. No std::, no captures, no references, no if constexpr, no templates.
 *   5. No swizzles. GLM_FORCE_SWIZZLE yields proxy types rather than vectors.
 *
 * Opening this namespace alongside `using namespace std` produces ambiguity on
 * max, min and abs. Keep the directive scoped to the namespace where fields are
 * authored.
 *
 * Verified against glslangValidator -S comp and g++ -std=c++20 for radial
 * attraction, cross-product swirl, sphere distance, smoothstep falloff, a
 * four-iteration accumulation loop, and a ternary-guarded normalisation.
 */
namespace MayaFlux::ShaderCompat {

using vec2 = glm::vec2;
using vec3 = glm::vec3;
using vec4 = glm::vec4;
using ivec2 = glm::ivec2;
using ivec3 = glm::ivec3;
using uvec3 = glm::uvec3;
using mat3 = glm::mat3;
using mat4 = glm::mat4;
using uint = std::uint32_t;

using glm::abs;
using glm::ceil;
using glm::clamp;
using glm::floor;
using glm::fract;
using glm::max;
using glm::min;
using glm::mix;
using glm::mod;
using glm::sign;
using glm::smoothstep;
using glm::step;

using glm::exp;
using glm::exp2;
using glm::inversesqrt;
using glm::log;
using glm::log2;
using glm::pow;
using glm::sqrt;

using glm::acos;
using glm::asin;
using glm::atan;
using glm::cos;
using glm::sin;
using glm::tan;

using glm::cross;
using glm::distance;
using glm::dot;
using glm::faceforward;
using glm::length;
using glm::normalize;
using glm::reflect;
using glm::refract;

} // namespace MayaFlux::ShaderCompat
