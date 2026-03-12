#pragma once

/**
 * @file Spectral.hpp
 * @brief Short-time Fourier domain primitives for MayaFlux::Kinesis
 *
 * Pure functions operating on contiguous double-precision spans.
 * No MayaFlux type dependencies. Domain-agnostic: the same primitives
 * serve audio, control, and any other sampled sequence.
 *
 * All functions that modify frequency content use weighted overlap-add
 * (WOLA) synthesis with a Hann analysis window. The synthesis window is
 * the same Hann window; at 75% overlap (hop = N/4) the OLA normalisation
 * factor is constant and the reconstruction is exact up to numerical noise.
 *
 * Eigen FFT dependency is confined to the .cpp translation unit.
 */

namespace MayaFlux::Kinesis::Discrete {

/**
 * @brief Callable type for per-frame spectrum modification
 *        Receives the one-sided complex spectrum (bins 0..N/2 inclusive)
 *        and the frame index. Modifications are written back in-place.
 */
using SpectrumProcessor = std::function<void(std::vector<std::complex<double>>&, size_t)>;

// ============================================================================
// Core STFT engine
// ============================================================================

/**
 * @brief Apply a per-frame spectrum processor via WOLA analysis-synthesis
 *
 * Frames the input with a Hann window, calls @p processor on each frame's
 * one-sided spectrum, reconstructs via IFFT, and accumulates with WOLA
 * normalisation. Output length equals input length.
 *
 * @param src          Input samples
 * @param window_size  FFT frame size (power of 2, >= 64)
 * @param hop_size     Hop between analysis frames
 * @param processor    Per-frame spectrum callback
 * @return             Processed output, same length as src
 */
[[nodiscard]] std::vector<double> apply_spectral(
    std::span<const double> src,
    uint32_t window_size,
    uint32_t hop_size,
    const SpectrumProcessor& processor);

// ============================================================================
// Spectral transforms
// ============================================================================

/**
 * @brief Hard bandpass filter: zero all bins outside [lo_hz, hi_hz]
 * @param src          Input samples
 * @param lo_hz        Lower frequency bound (Hz)
 * @param hi_hz        Upper frequency bound (Hz)
 * @param sample_rate  Sample rate (Hz)
 * @param window_size  FFT frame size
 * @param hop_size     Analysis hop
 * @return             Filtered output
 */
[[nodiscard]] std::vector<double> spectral_filter(
    std::span<const double> src,
    double lo_hz,
    double hi_hz,
    double sample_rate,
    uint32_t window_size = 1024,
    uint32_t hop_size = 256);

/**
 * @brief Spectral phase inversion (conjugate all bins)
 * @param src          Input samples
 * @param window_size  FFT frame size
 * @param hop_size     Analysis hop
 * @return             Phase-inverted output
 */
[[nodiscard]] std::vector<double> spectral_invert(
    std::span<const double> src,
    uint32_t window_size = 1024,
    uint32_t hop_size = 256);

/**
 * @brief Linear spectral tilt: scale each bin by a factor that rises
 *        linearly from 1 at bin 0 to @p enhancement_factor at the Nyquist bin
 *
 * Ported from the existing SpectralHelper behaviour. This is a first-order
 * spectral emphasis, not a true harmonic detector; caller should be aware
 * that it brightens high-frequency energy uniformly regardless of harmonic
 * structure.
 *
 * @param src                Input samples
 * @param enhancement_factor Gain at the Nyquist bin (> 1 brightens, < 1 darkens)
 * @param window_size        FFT frame size
 * @param hop_size           Analysis hop
 * @return                   Enhanced output
 */
[[nodiscard]] std::vector<double> harmonic_enhance(
    std::span<const double> src,
    double enhancement_factor,
    uint32_t window_size = 1024,
    uint32_t hop_size = 256);

/**
 * @brief Hard spectral gate: zero bins whose magnitude is below the threshold
 * @param src           Input samples
 * @param threshold_db  Gate threshold in dBFS; bins below are zeroed
 * @param window_size   FFT frame size
 * @param hop_size      Analysis hop
 * @return              Gated output
 */
[[nodiscard]] std::vector<double> spectral_gate(
    std::span<const double> src,
    double threshold_db,
    uint32_t window_size = 1024,
    uint32_t hop_size = 256);

// ============================================================================
// Phase vocoder operations
// ============================================================================

/**
 * @brief Time-stretch via phase vocoder analysis-synthesis
 *
 * Produces output of length approximately src.size() * stretch_factor.
 * Spectral envelopes and pitched content are preserved without aliasing.
 * Sharp transients exhibit mild pre-echo at high stretch ratios (> 2×),
 * which is inherent to the standard phase vocoder algorithm.
 *
 * Recommended parameters for audio at 48 kHz:
 *   window_size = 2048, analysis_hop = 512 (75% overlap).
 *
 * @param src           Input samples
 * @param stretch_factor >1 = slower/longer, <1 = faster/shorter; 1 returns copy
 * @param window_size   FFT frame size (power of 2, >= 64)
 * @param analysis_hop  Analysis hop Ha; synthesis hop Hs = Ha * stretch_factor
 * @return              Time-stretched output
 */
[[nodiscard]] std::vector<double> phase_vocoder_stretch(
    std::span<const double> src,
    double stretch_factor,
    uint32_t window_size = 2048,
    uint32_t analysis_hop = 512);

/**
 * @brief Pitch-shift by resampling around a phase vocoder stretch
 *
 * Stretches by 1/pitch_ratio, then resamples back to the original length
 * using linear interpolation. This preserves duration while shifting pitch,
 * which is the standard phase vocoder pitch-shift approach.
 *
 * @param src          Input samples
 * @param semitones    Pitch shift in semitones (positive = up, negative = down)
 * @param window_size  FFT frame size
 * @param analysis_hop Analysis hop
 * @return             Pitch-shifted output, same length as src
 */
[[nodiscard]] std::vector<double> pitch_shift(
    std::span<const double> src,
    double semitones,
    uint32_t window_size = 2048,
    uint32_t analysis_hop = 512);

} // namespace MayaFlux::Kinesis::Discrete
