#pragma once

namespace MayaFlux::Nodes::Generator {

enum WindowType {
    HANNING,
    HAMMING,
    BLACKMAN,
    RECTANGULAR,
};

/**
 * @brief Creates a Hann window function
 * @param length Number of samples in the window
 * @return Vector containing the window function values
 *
 * The Hann window (sometimes called Hanning) is a bell-shaped window
 * function that tapers smoothly to zero at both ends. It's commonly
 * used for:
 * - Smoothing signal transitions
 * - Reducing spectral leakage in frequency domain analysis
 * - Creating envelope shapes for synthesis
 *
 * Mathematical formula: w(n) = 0.5 * (1 - cos(2π*n/(N-1)))
 */
std::vector<double> HannWindow(size_t length);

/**
 * @brief Creates a Hamming window function
 * @param length Number of samples in the window
 * @return Vector containing the window function values
 *
 * The Hamming window is similar to the Hann window but doesn't
 * reach zero at the edges. It offers different spectral characteristics
 * and is often used in:
 * - Signal processing
 * - Filter design
 * - Spectral analysis
 *
 * Mathematical formula: w(n) = 0.54 - 0.46*cos(2π*n/(N-1))
 */
std::vector<double> HammingWindow(size_t length);

/**
 * @brief Creates a Blackman window function
 * @param length Number of samples in the window
 * @return Vector containing the window function values
 *
 * The Blackman window provides better sidelobe suppression than
 * Hamming or Hann windows, making it useful for:
 * - High-quality spectral analysis
 * - Applications requiring minimal spectral leakage
 * - Creating smooth envelopes with minimal artifacts
 *
 * Mathematical formula: w(n) = 0.42 - 0.5*cos(2π*n/(N-1)) + 0.08*cos(4π*n/(N-1))
 */
std::vector<double> BlackmanWindow(size_t length);

/**
 * @brief Creates a linear ramp function
 * @param length Number of samples in the ramp
 * @param start Starting value (default: 0.0)
 * @param end Ending value (default: 1.0)
 * @return Vector containing the ramp function values
 *
 * A linear ramp increases or decreases at a constant rate from
 * start to end value. Useful for:
 * - Creating linear transitions
 * - Parameter automation
 * - Simple envelope shapes
 */
std::vector<double> LinearRamp(size_t length, double start = 0.0, double end = 1.0);

/**
 * @brief Creates an exponential ramp function
 * @param length Number of samples in the ramp
 * @param start Starting value (default: 0.001)
 * @param end Ending value (default: 1.0)
 * @return Vector containing the ramp function values
 *
 * An exponential ramp changes at a rate proportional to its current value,
 * creating a curve that follows natural growth or decay patterns. Useful for:
 * - Perceptually balanced transitions
 * - Parameter sweeps with natural-sounding transitions
 * - Creating more organic envelope shapes
 *
 * Note: The start value defaults to 0.001 instead of 0.0 because a true
 * exponential curve cannot start at zero.
 */
std::vector<double> ExponentialRamp(size_t length, double start = 0.001, double end = 1.0);

/**
 * @brief Generate window coefficients using C++20 ranges
 * @param size Window size
 * @param window_type Type of window function
 * @return Window coefficients
 */
std::vector<double> generate_window(u_int32_t size, WindowType window_type);

}
