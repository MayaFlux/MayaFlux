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

/**
 * @class Resampler
 * @brief resample as a stream: block by block, with the filter history kept between calls
 *
 * Concatenating everything push returns and then flush returns gives exactly
 * what resample returns for the concatenated input, however the input was cut
 * into blocks, so no block edge restarts the filter. An output sample is
 * produced as soon as the input samples it reads have arrived: each reads
 * lookahead() input samples past its own time, which is the delay a live
 * caller sees. flush ends the stream, supplying zeros past the last sample
 * as resample does, and the next push starts a new stream. Equal rates pass
 * the input through unchanged and with no delay.
 *
 * Only the input the next output still needs is retained, so memory is the
 * length of the filter however long the stream runs. Not for concurrent use.
 */
class MAYAFLUX_API Resampler {
public:
    /**
     * @brief Construct a resampler
     * @param from_rate Rate of the input in Hz
     * @param to_rate Rate of the output in Hz
     * @param config Anti-aliasing filter design, as for resample
     * @throws std::invalid_argument When either rate is 0
     */
    Resampler(uint32_t from_rate, uint32_t to_rate, const ResampleConfig& config = {});
    ~Resampler();

    Resampler(const Resampler&) = delete;
    Resampler& operator=(const Resampler&) = delete;
    Resampler(Resampler&&) noexcept;
    Resampler& operator=(Resampler&&) noexcept;

    /**
     * @brief Continue the stream with more input
     * @return The output samples that are complete, possibly none
     */
    [[nodiscard]] std::vector<double> push(std::span<const double> src);

    /**
     * @brief End the stream
     * @return The remaining output samples, so that the stream's total is resampled_length of its input
     */
    [[nodiscard]] std::vector<double> flush();

    /** @brief Start a new stream without releasing anything. */
    void reset() noexcept;

    /** @brief Input samples an output reads beyond its own time, 0 for equal rates. */
    [[nodiscard]] size_t lookahead() const noexcept;

    /** @brief Input samples currently held. */
    [[nodiscard]] size_t retained() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace MayaFlux::Kinesis::Discrete
