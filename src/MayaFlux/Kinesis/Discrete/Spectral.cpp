#include "Spectral.hpp"

#include <Eigen/Dense>
#include <unsupported/Eigen/FFT>

namespace MayaFlux::Kinesis::Discrete {

namespace {

    constexpr double k_pi = std::numbers::pi;
    constexpr double k_tau = 2.0 * std::numbers::pi;

    /**
     * @brief Pre-computed Hann window, not normalised
     *        The WOLA loop divides by sum-of-squares so normalisation
     *        is applied analytically at reconstruction time.
     */
    [[nodiscard]] std::vector<double> make_hann(uint32_t n)
    {
        std::vector<double> w(n);
        const double inv = 1.0 / static_cast<double>(n - 1);
        for (uint32_t i = 0; i < n; ++i)
            w[i] = 0.5 * (1.0 - std::cos(k_tau * i * inv));
        return w;
    }

    /**
     * @brief Wrap phase to (-pi, pi]
     */
    [[nodiscard]] inline double wrap_phase(double p) noexcept
    {
        p -= k_tau * std::floor((p + k_pi) / k_tau);
        return p;
    }

    /**
     * @brief Build a full N-point conjugate-symmetric spectrum from the
     *        one-sided representation (bins 0..bins-1, bins = N/2+1)
     */
    [[nodiscard]] Eigen::VectorXcd to_full_spectrum(
        const std::vector<std::complex<double>>& onesided,
        uint32_t N)
    {
        const uint32_t bins = N / 2 + 1;
        Eigen::VectorXcd full(static_cast<Eigen::Index>(N));
        for (uint32_t b = 0; b < bins; ++b)
            full(static_cast<Eigen::Index>(b)) = onesided[b];
        for (uint32_t b = bins; b < N; ++b)
            full(static_cast<Eigen::Index>(b)) = std::conj(onesided[N - b]);
        return full;
    }

} // namespace

// ============================================================================
// Core STFT engine
// ============================================================================

std::vector<double> apply_spectral(
    std::span<const double> src,
    uint32_t window_size,
    uint32_t hop_size,
    const SpectrumProcessor& processor)
{
    if (src.empty())
        return {};

    const uint32_t N = window_size;
    const uint32_t bins = N / 2 + 1;
    const std::vector<double> win = make_hann(N);

    const size_t n_frames = (src.size() >= N)
        ? (src.size() - N) / hop_size + 1
        : 1;

    std::vector<double> output(src.size(), 0.0);
    std::vector<double> norm(src.size(), 0.0);

    Eigen::FFT<double> fft;
    Eigen::VectorXd frame(N);
    Eigen::VectorXcd spectrum;
    Eigen::VectorXd ifft_out;

    for (size_t f = 0; f < n_frames; ++f) {
        const size_t pos = f * hop_size;

        for (uint32_t k = 0; k < N; ++k) {
            const size_t idx = pos + k;
            frame(static_cast<Eigen::Index>(k)) = (idx < src.size()) ? src[idx] * win[k] : 0.0;
        }

        fft.fwd(spectrum, frame);

        std::vector<std::complex<double>> onesided(bins);
        for (uint32_t b = 0; b < bins; ++b)
            onesided[b] = spectrum(static_cast<Eigen::Index>(b));

        processor(onesided, f);

        Eigen::VectorXcd full = to_full_spectrum(onesided, N);
        fft.inv(ifft_out, full);

        for (uint32_t k = 0; k < N && pos + k < src.size(); ++k) {
            const double w = win[k];
            output[pos + k] += ifft_out(static_cast<Eigen::Index>(k)) * w;
            norm[pos + k] += w * w;
        }
    }

    for (size_t i = 0; i < src.size(); ++i) {
        if (norm[i] > 1e-10)
            output[i] /= norm[i];
    }

    return output;
}

// ============================================================================
// Spectral transforms
// ============================================================================

std::vector<double> spectral_filter(
    std::span<const double> src,
    double lo_hz,
    double hi_hz,
    double sample_rate,
    uint32_t window_size,
    uint32_t hop_size)
{
    const double bin_hz = sample_rate / static_cast<double>(window_size);

    return apply_spectral(src, window_size, hop_size,
        [lo_hz, hi_hz, bin_hz](std::vector<std::complex<double>>& spec, size_t) {
            for (size_t b = 0; b < spec.size(); ++b) {
                const double f = static_cast<double>(b) * bin_hz;
                if (f < lo_hz || f > hi_hz)
                    spec[b] = {};
            }
        });
}

std::vector<double> spectral_invert(
    std::span<const double> src,
    uint32_t window_size,
    uint32_t hop_size)
{
    return apply_spectral(src, window_size, hop_size,
        [](std::vector<std::complex<double>>& spec, size_t) {
            for (auto& bin : spec)
                bin = std::conj(bin);
        });
}

std::vector<double> harmonic_enhance(
    std::span<const double> src,
    double enhancement_factor,
    uint32_t window_size,
    uint32_t hop_size)
{
    const auto nyquist_bin = static_cast<double>((double)window_size / 2);

    return apply_spectral(src, window_size, hop_size,
        [enhancement_factor, nyquist_bin](std::vector<std::complex<double>>& spec, size_t) {
            for (size_t b = 1; b < spec.size(); ++b) {
                const double t = static_cast<double>(b) / nyquist_bin;
                spec[b] *= 1.0 + (enhancement_factor - 1.0) * t;
            }
        });
}

std::vector<double> spectral_gate(
    std::span<const double> src,
    double threshold_db,
    uint32_t window_size,
    uint32_t hop_size)
{
    const double lin = std::pow(10.0, threshold_db / 20.0);

    return apply_spectral(src, window_size, hop_size,
        [lin](std::vector<std::complex<double>>& spec, size_t) {
            for (auto& bin : spec) {
                if (std::abs(bin) < lin)
                    bin = {};
            }
        });
}

// ============================================================================
// Phase vocoder operations
// ============================================================================

std::vector<double> phase_vocoder_stretch(
    std::span<const double> src,
    double stretch_factor,
    uint32_t window_size,
    uint32_t analysis_hop)
{
    if (src.empty() || stretch_factor <= 0.0)
        return {};

    if (stretch_factor == 1.0)
        return { src.begin(), src.end() };

    const uint32_t N = window_size;
    const uint32_t Ha = analysis_hop;
    const auto Hs = static_cast<uint32_t>(std::round(Ha * stretch_factor));
    const uint32_t bins = N / 2 + 1;

    const std::vector<double> win = make_hann(N);

    const double omega_factor = k_tau * static_cast<double>(Ha) / static_cast<double>(N);

    const size_t n_frames = (src.size() >= N)
        ? (src.size() - N) / Ha + 1
        : 1;

    const size_t out_len = static_cast<size_t>(static_cast<double>(src.size()) * stretch_factor) + N;

    std::vector<double> output(out_len, 0.0);
    std::vector<double> norm(out_len, 0.0);

    std::vector<double> phase_accum(bins, 0.0);
    std::vector<double> prev_phase(bins, 0.0);

    Eigen::FFT<double> fft;
    Eigen::VectorXd frame(N);
    Eigen::VectorXcd spectrum;
    Eigen::VectorXd ifft_out;

    for (size_t f = 0; f < n_frames; ++f) {
        const size_t src_pos = f * Ha;

        for (uint32_t k = 0; k < N; ++k) {
            const size_t idx = src_pos + k;
            frame(static_cast<Eigen::Index>(k)) = (idx < src.size()) ? src[idx] * win[k] : 0.0;
        }

        fft.fwd(spectrum, frame);

        std::vector<std::complex<double>> synth(bins);
        for (uint32_t b = 0; b < bins; ++b) {
            const std::complex<double> bin = spectrum(static_cast<Eigen::Index>(b));
            const double mag = std::abs(bin);
            const double phase = std::arg(bin);

            const double delta = phase - prev_phase[b]
                - omega_factor * static_cast<double>(b);
            const double true_f = omega_factor * static_cast<double>(b)
                + wrap_phase(delta);

            phase_accum[b] += static_cast<double>(Hs) * true_f
                / static_cast<double>(Ha);
            prev_phase[b] = phase;

            synth[b] = std::polar(mag, phase_accum[b]);
        }

        Eigen::VectorXcd full = to_full_spectrum(synth, N);
        fft.inv(ifft_out, full);

        const size_t out_pos = f * Hs;
        for (uint32_t k = 0; k < N && out_pos + k < out_len; ++k) {
            const double w = win[k];
            output[out_pos + k] += ifft_out(static_cast<Eigen::Index>(k)) * w;
            norm[out_pos + k] += w * w;
        }
    }

    for (size_t i = 0; i < out_len; ++i) {
        if (norm[i] > 1e-10)
            output[i] /= norm[i];
    }

    output.resize(
        static_cast<size_t>(static_cast<double>(src.size()) * stretch_factor));

    return output;
}

std::vector<double> pitch_shift(
    std::span<const double> src,
    double semitones,
    uint32_t window_size,
    uint32_t analysis_hop)
{
    if (src.empty() || semitones == 0.0)
        return { src.begin(), src.end() };

    const double pitch_ratio = std::pow(2.0, semitones / 12.0);
    const double stretch_ratio = 1.0 / pitch_ratio;

    std::vector<double> stretched = phase_vocoder_stretch(src, stretch_ratio, window_size, analysis_hop);

    if (stretched.empty())
        return {};

    const size_t out_n = src.size();
    std::vector<double> out(out_n);
    const size_t in_n = stretched.size();
    const double step = static_cast<double>(in_n - 1) / static_cast<double>(out_n - 1);

    for (size_t i = 0; i < out_n; ++i) {
        const double pos = static_cast<double>(i) * step;
        const auto idx = static_cast<size_t>(pos);
        const size_t idx1 = std::min(idx + 1, in_n - 1);
        const double frac = pos - static_cast<double>(idx);
        out[i] = stretched[idx] + frac * (stretched[idx1] - stretched[idx]);
    }

    return out;
}

} // namespace MayaFlux::Kinesis::Discrete
