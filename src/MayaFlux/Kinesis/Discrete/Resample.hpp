#pragma once

/**
 * @file Resample.hpp
 * @brief Anti-aliased sample rate conversion by a rational ratio for MayaFlux::Kinesis
 *
 * Pure numerical functions on contiguous double-precision spans. Domain-agnostic:
 * a sequence is resampled by the ratio of two rates, whatever it holds.
 */

namespace MayaFlux::Kinesis::Discrete {

/**
 * @struct ResampleConfig
 * @brief Design of the anti-aliasing filter of resample
 *
 * The filter is a windowed sinc whose cutoff is the lower of the two Nyquist
 * frequencies. zero_crossings is the number of zero crossings of the sinc kept on
 * each side of its centre, so a larger value narrows the transition band and
 * lengthens the filter. kaiser_beta shapes the Kaiser window: a larger value
 * lowers the stopband ripple and widens the transition band.
 */
struct ResampleConfig {
    uint32_t zero_crossings { 10 };
    double kaiser_beta { 5.0 };
};

/**
 * @brief Number of samples resample returns
 * @param samples Input length
 * @param from_rate Rate of the input
 * @param to_rate Rate of the output
 * @return ceil(samples * to_rate / from_rate), 0 when either rate is 0
 */
[[nodiscard]] MAYAFLUX_API size_t resampled_length(
    size_t samples, uint32_t from_rate, uint32_t to_rate) noexcept;

/**
 * @brief Resample a sequence from one rate to another with an anti-aliasing filter
 *
 * The ratio is reduced to up / down by its greatest common divisor and applied as
 * a polyphase filter: the sequence is conceptually upsampled by up, lowpass
 * filtered at the lower of the two Nyquist frequencies, and decimated by down,
 * without computing the samples in between. Output sample k is at time
 * k / to_rate and the filter delay is removed, so the first output sample lines
 * up with the first input sample. Samples outside the input are zero. The same
 * rates return a copy. Output blocks are computed in parallel.
 *
 * The result matches scipy.signal.resample_poly with a Kaiser window of the same
 * beta and a filter half length of zero_crossings * max(up, down) samples.
 *
 * @param src Input samples
 * @param from_rate Rate of the input in Hz
 * @param to_rate Rate of the output in Hz
 * @param config Anti-aliasing filter design
 * @return resampled_length(src.size(), from_rate, to_rate) samples, empty when
 *         the input is empty or either rate is 0
 */
[[nodiscard]] MAYAFLUX_API std::vector<double> resample(
    std::span<const double> src,
    uint32_t from_rate,
    uint32_t to_rate,
    const ResampleConfig& config = {});

} // namespace MayaFlux::Kinesis::Discrete
