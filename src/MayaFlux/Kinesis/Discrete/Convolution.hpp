#pragma once

namespace MayaFlux::Kinesis::Discrete {

/**
 * @brief Frequency-domain processor callback for apply_convolution
 *
 * Receives the one-sided FFT of the signal and the kernel, writes the
 * result into the third argument. All three vectors have length fft_size/2+1.
 */
using ConvolutionProcessor = std::function<void(
    const std::vector<std::complex<double>>& signal_fft,
    const std::vector<std::complex<double>>& kernel_fft,
    std::vector<std::complex<double>>& result_fft)>;

/**
 * @brief Core FFT convolution engine
 * @param src Input signal
 * @param kernel Kernel signal
 * @param processor Frequency-domain operation
 * @param full_size When true returns linear convolution length (N+M-1);
 *        when false returns the same number of samples as @p src
 * @return Processed output
 */
[[nodiscard]] std::vector<double> apply_convolution(
    std::span<const double> src,
    std::span<const double> kernel,
    const ConvolutionProcessor& processor,
    bool full_size = false);

/**
 * @brief Linear FFT convolution
 * @param src Input signal
 * @param ir Impulse response
 * @return Convolved output, same length as @p src
 */
[[nodiscard]] std::vector<double> convolve(
    std::span<const double> src,
    std::span<const double> ir);

/**
 * @brief Cross-correlation via FFT
 * @param src Input signal
 * @param tmpl Template signal
 * @param normalize Normalise result to peak absolute value
 * @return Cross-correlation, same length as @p src
 */
[[nodiscard]] std::vector<double> cross_correlate(
    std::span<const double> src,
    std::span<const double> tmpl,
    bool normalize = true);

/**
 * @brief Matched filter (normalised cross-correlation)
 * @param src Input signal
 * @param reference Reference signal
 * @return Matched filter output, same length as @p src
 */
[[nodiscard]] std::vector<double> matched_filter(
    std::span<const double> src,
    std::span<const double> reference);

/**
 * @brief Frequency-domain deconvolution with Tikhonov regularisation
 * @param src Input signal
 * @param ir Impulse response to invert
 * @param regularization Regularisation factor for numerical stability
 * @return Deconvolved output, same length as @p src
 */
[[nodiscard]] std::vector<double> deconvolve(
    std::span<const double> src,
    std::span<const double> ir,
    double regularization = 1e-6);

/**
 * @brief Auto-correlation via FFT
 * @param src Input signal
 * @param normalize Normalise result to zero-lag peak
 * @return Auto-correlation, same length as @p src
 */
[[nodiscard]] std::vector<double> auto_correlate(
    std::span<const double> src,
    bool normalize = true);

} // namespace MayaFlux::Kinesis::Discrete
