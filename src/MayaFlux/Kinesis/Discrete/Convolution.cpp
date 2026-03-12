#include "Convolution.hpp"

#include "MayaFlux/Transitive/Parallel/Execution.hpp"

#include <Eigen/Dense>
#include <unsupported/Eigen/FFT>

namespace P = MayaFlux::Parallel;

namespace MayaFlux::Kinesis::Discrete {

std::vector<double> apply_convolution(
    std::span<const double> src,
    std::span<const double> kernel,
    const ConvolutionProcessor& processor,
    bool full_size)
{
    if (src.empty() || kernel.empty())
        return std::vector<double>(src.size(), 0.0);

    const size_t conv_len = src.size() + kernel.size() - 1;
    const size_t fft_size = std::bit_ceil(std::max(size_t { 256 }, conv_len));
    const size_t bins = fft_size / 2 + 1;

    Eigen::VectorXd padded_src = Eigen::VectorXd::Zero(static_cast<Eigen::Index>(fft_size));
    Eigen::VectorXd padded_kernel = Eigen::VectorXd::Zero(static_cast<Eigen::Index>(fft_size));
    std::ranges::copy(src, padded_src.begin());
    std::ranges::copy(kernel, padded_kernel.begin());

    Eigen::FFT<double> fft;
    Eigen::VectorXcd src_fft, ker_fft;
    fft.fwd(src_fft, padded_src);
    fft.fwd(ker_fft, padded_kernel);

    std::vector<std::complex<double>> sig(bins), ker(bins), res(bins);
    P::for_each(P::par_unseq,
        std::views::iota(size_t { 0 }, bins).begin(),
        std::views::iota(size_t { 0 }, bins).end(),
        [&](size_t b) {
            sig[b] = src_fft(static_cast<Eigen::Index>(b));
            ker[b] = ker_fft(static_cast<Eigen::Index>(b));
        });

    processor(sig, ker, res);

    Eigen::VectorXcd full_fft(static_cast<Eigen::Index>(fft_size));
    P::for_each(P::par_unseq,
        std::views::iota(size_t { 0 }, fft_size).begin(),
        std::views::iota(size_t { 0 }, fft_size).end(),
        [&](size_t b) {
            full_fft(static_cast<Eigen::Index>(b)) = (b < bins)
                ? res[b]
                : std::conj(res[fft_size - b]);
        });

    const double inv = 1.0 / static_cast<double>(fft_size);
    full_fft *= inv;

    Eigen::VectorXd time_result;
    fft.inv(time_result, full_fft);

    const size_t out_len = full_size ? conv_len : src.size();
    std::vector<double> out(out_len);
    P::for_each(P::par_unseq,
        std::views::iota(size_t { 0 }, out_len).begin(),
        std::views::iota(size_t { 0 }, out_len).end(),
        [&](size_t i) { out[i] = time_result(static_cast<Eigen::Index>(i)); });
    return out;
}

std::vector<double> convolve(std::span<const double> src, std::span<const double> ir)
{
    if (ir.size() == 1) {
        std::vector<double> out(src.size());
        const double g = ir[0];
        P::transform(P::par_unseq, src.begin(), src.end(), out.begin(),
            [g](double x) { return x * g; });
        return out;
    }

    return apply_convolution(src, ir,
        [](const auto& s, const auto& k, auto& r) {
            P::transform(P::par_unseq, s.begin(), s.end(), k.begin(), r.begin(),
                [](const std::complex<double>& a, const std::complex<double>& b) {
                    return a * b;
                });
        });
}

std::vector<double> cross_correlate(
    std::span<const double> src,
    std::span<const double> tmpl,
    bool normalize)
{
    std::vector<double> reversed(tmpl.rbegin(), tmpl.rend());

    auto out = apply_convolution(src, reversed,
        [](const auto& s, const auto& k, auto& r) {
            P::transform(P::par_unseq, s.begin(), s.end(), k.begin(), r.begin(),
                [](const std::complex<double>& a, const std::complex<double>& b) {
                    return a * std::conj(b);
                });
        });

    if (normalize && !out.empty()) {
        const auto [mn, mx] = std::ranges::minmax_element(out);
        const double peak = std::max(std::abs(*mn), std::abs(*mx));
        if (peak > 0.0) {
            P::transform(P::par_unseq, out.begin(), out.end(), out.begin(),
                [peak](double v) { return v / peak; });
        }
    }

    return out;
}

std::vector<double> matched_filter(
    std::span<const double> src,
    std::span<const double> reference)
{
    return cross_correlate(src, reference, true);
}

std::vector<double> deconvolve(
    std::span<const double> src,
    std::span<const double> ir,
    double regularization)
{
    return apply_convolution(src, ir,
        [regularization](const auto& s, const auto& k, auto& r) {
            P::transform(P::par_unseq, s.begin(), s.end(), k.begin(), r.begin(),
                [regularization](const std::complex<double>& sig, const std::complex<double>& ker) {
                    const double mag_sq = std::norm(ker);
                    if (mag_sq < regularization)
                        return std::complex<double> {};
                    return sig * std::conj(ker) / (mag_sq + regularization);
                });
        });
}

std::vector<double> auto_correlate(std::span<const double> src, bool normalize)
{
    auto out = apply_convolution(src, src,
        [](const auto& s, const auto&, auto& r) {
            P::transform(P::par_unseq, s.begin(), s.end(), s.begin(), r.begin(),
                [](const std::complex<double>& a, const std::complex<double>& b) {
                    return a * std::conj(b);
                });
        });

    if (normalize && !out.empty()) {
        const double peak = *std::max_element(out.begin(), out.end());
        if (peak > 0.0) {
            P::transform(P::par_unseq, out.begin(), out.end(), out.begin(),
                [peak](double v) { return v / peak; });
        }
    }

    return out;
}

} // namespace MayaFlux::Kinesis::Discrete
