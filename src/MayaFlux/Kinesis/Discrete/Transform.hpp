#pragma once

/**
 * @file Transform.hpp
 * @brief Discrete sequence transformation primitives for MayaFlux::Kinesis
 *
 * Pure numerical functions operating on contiguous double-precision spans.
 * No MayaFlux type dependencies. Domain-agnostic — the same primitives
 * serve audio, visual, control, and any other sampled sequence.
 *
 * All mutating functions operate in-place on the supplied span.
 * Functions that change the output size return a new vector.
 * Callers loop over channels; no multichannel wrappers are provided.
 *
 * SIMD notes:
 *   linear, clamp, reverse, fade, normalize, quantize, interpolate_linear,
 *   and interpolate_cubic are written to be auto-vectorisable under
 *   -O2 -march=native (GCC/Clang). Transcendental functions (power,
 *   exponential, logarithmic) remain scalar; they are not hot-path
 *   and require SVML or a polynomial approximation for SIMD throughput.
 */

namespace MayaFlux::Kinesis::Discrete {

// ============================================================================
// Pointwise arithmetic
// ============================================================================

/**
 * @brief Linear map y = a*x + b applied in-place
 * @param data Target span
 * @param a Scale factor
 * @param b Offset
 */
void linear(std::span<double> data, double a, double b) noexcept;

/**
 * @brief Power map y = x^exponent applied in-place
 *        Scalar transcendental — not SIMD hot-path.
 * @param data Target span
 * @param exponent Exponent
 */
void power(std::span<double> data, double exponent) noexcept;

/**
 * @brief Exponential map y = a * base^(b*x) applied in-place
 *        Scalar transcendental — not SIMD hot-path.
 * @param data Target span
 * @param a Scale factor
 * @param b Rate
 * @param base Base (default: e)
 */
void exponential(std::span<double> data, double a, double b,
    double base = std::numbers::e) noexcept;

/**
 * @brief Logarithmic map y = a * log_base(b*x + c) applied in-place
 *        Values where (b*x + c) <= 0 are mapped to 0.
 *        Scalar transcendental — not SIMD hot-path.
 * @param data Target span
 * @param a Scale factor
 * @param b Input scale
 * @param c Input offset
 * @param base Logarithm base (default: e)
 */
void logarithmic(std::span<double> data, double a, double b, double c,
    double base = std::numbers::e) noexcept;

/**
 * @brief Trigonometric map y = amplitude * f(frequency*x + phase) applied in-place
 * @tparam TrigFunc Callable matching double(double)
 * @param data Target span
 * @param func Trigonometric function (std::sin, std::cos, etc.)
 * @param frequency Frequency scaling applied to x
 * @param amplitude Amplitude scaling applied to output
 * @param phase Phase offset added to x before the function
 */
template <typename TrigFunc>
    requires std::invocable<TrigFunc, double>
    && std::same_as<std::invoke_result_t<TrigFunc, double>, double>
void apply_trig(std::span<double> data,
    TrigFunc func,
    double frequency,
    double amplitude,
    double phase) noexcept
{
    std::ranges::transform(data, data.begin(),
        [&](double x) { return amplitude * func(frequency * x + phase); });
}

/**
 * @brief Clamp values to [lo, hi] in-place
 * @param data Target span
 * @param lo Lower bound
 * @param hi Upper bound
 */
void clamp(std::span<double> data, double lo, double hi) noexcept;

/**
 * @brief Quantize to n-bit resolution in-place
 *        Input is assumed to be in [-1, 1]; values outside are clamped first.
 *        Uses lrint bias trick to avoid scalar std::round and improve
 *        auto-vectorisation.
 * @param data Target span
 * @param bits Bit depth (1–53)
 */
void quantize(std::span<double> data, uint8_t bits) noexcept;

/**
 * @brief Normalize to [target_min, target_max] in-place
 *        Single-pass min/max reduction followed by a single transform pass.
 *        No-op when all values are equal.
 * @param data Target span
 * @param target_min Output minimum
 * @param target_max Output maximum
 */
void normalize(std::span<double> data,
    double target_min = -1.0,
    double target_max = 1.0) noexcept;

// ============================================================================
// Temporal shape
// ============================================================================

/**
 * @brief Reverse temporal order in-place
 * @param data Target span
 */
void reverse(std::span<double> data) noexcept;

/**
 * @brief Apply equal-power (cosine) fade-in then fade-out envelope in-place
 *        The cosine taper maintains constant perceived loudness at the
 *        crossover point (0 dB centre gain vs −6 dB for a linear taper).
 * @param data Target span
 * @param fade_in_ratio Fraction of data length used for fade-in  [0, 1]
 * @param fade_out_ratio Fraction of data length used for fade-out [0, 1]
 */
void fade(std::span<double> data,
    double fade_in_ratio,
    double fade_out_ratio) noexcept;

/**
 * @brief Extract a contiguous slice by ratio, returning a new vector
 * @param data Source span
 * @param start_ratio Normalised start position [0, 1]
 * @param end_ratio Normalised end position   [0, 1], must be > start_ratio
 * @return Slice data; empty if parameters are degenerate
 */
[[nodiscard]] std::vector<double> slice(
    std::span<const double> data,
    double start_ratio,
    double end_ratio);

/**
 * @brief Prepend delay_samples zero-valued (or fill_value) samples, returning a new vector
 * @param data Source span
 * @param delay_samples Number of samples to prepend
 * @param fill_value Value used for the prepended region
 * @return Delayed output of size data.size() + delay_samples
 */
[[nodiscard]] std::vector<double> delay(
    std::span<const double> data,
    uint32_t delay_samples,
    double fill_value = 0.0);

// ============================================================================
// Resampling / interpolation
// ============================================================================

/**
 * @brief Linear interpolation from src into dst (caller sizes dst)
 *        Branchless inner loop with precomputed step; dst may be larger or
 *        smaller than src. The final index is clamped via std::min rather
 *        than a conditional branch, permitting auto-vectorisation.
 * @param src Source span (read-only)
 * @param dst Destination span (written in-place)
 */
void interpolate_linear(std::span<const double> src,
    std::span<double> dst) noexcept;

/**
 * @brief Catmull-Rom cubic interpolation from src into dst (caller sizes dst)
 *        Branchless boundary clamping; Horner evaluation form.
 * @param src Source span (read-only, at least 2 samples)
 * @param dst Destination span (written in-place)
 */
void interpolate_cubic(std::span<const double> src,
    std::span<double> dst) noexcept;

/**
 * @brief Time-stretch via linear interpolation resampling
 *        Fast but alias-naive: no anti-aliasing pre-filter is applied when
 *        stretch_factor < 1. Suitable for control signals and non-critical
 *        upsampling. For audio time-stretching use
 *        Discrete::phase_vocoder_stretch (PhaseVocoder.hpp).
 * @param data Source span
 * @param stretch_factor >1 = longer/slower, <1 = shorter/faster; 1 returns copy
 * @return Resampled vector
 */
[[nodiscard]] std::vector<double> time_stretch(
    std::span<const double> data,
    double stretch_factor);

} // namespace MayaFlux::Kinesis::Discrete
