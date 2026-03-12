
#include "Analysis.hpp"

#include "MayaFlux/Transitive/Parallel/Execution.hpp"

#include <Eigen/Dense>
#include <unsupported/Eigen/FFT>

namespace MayaFlux::Kinesis::Discrete {

namespace {

    constexpr double k_epsilon = 1e-10;

    /**
     * @brief Pre-computed Hann window coefficients for a given size
     */
    [[nodiscard]] Eigen::VectorXd hann_window(uint32_t size)
    {
        Eigen::VectorXd w(size);
        for (uint32_t i = 0; i < size; ++i)
            w(i) = 0.5 * (1.0 - std::cos(2.0 * std::numbers::pi * i / (size - 1)));
        return w;
    }

} // namespace

// ============================================================================
// Energy
// ============================================================================

std::vector<double> rms(std::span<const double> data, size_t n_windows, uint32_t hop_size, uint32_t window_size)
{
    std::vector<double> out(n_windows);
    std::vector<size_t> idx(n_windows);
    std::ranges::iota(idx, 0);

    Parallel::for_each(Parallel::par_unseq, idx.begin(), idx.end(),
        [&](size_t i) {
            const size_t start = i * hop_size;
            auto w = data.subspan(start, std::min<size_t>(window_size, data.size() - start));
            double sq = 0.0;
            for (double s : w)
                sq += s * s;
            out[i] = std::sqrt(sq / static_cast<double>(w.size()));
        });

    return out;
}

std::vector<double> peak(std::span<const double> data, size_t n_windows, uint32_t hop_size, uint32_t window_size)
{
    std::vector<double> out(n_windows);
    std::vector<size_t> idx(n_windows);
    std::ranges::iota(idx, 0);

    Parallel::for_each(Parallel::par_unseq, idx.begin(), idx.end(),
        [&](size_t i) {
            const size_t start = i * hop_size;
            auto w = data.subspan(start, std::min<size_t>(window_size, data.size() - start));
            double mx = 0.0;
            for (double s : w)
                mx = std::max(mx, std::abs(s));
            out[i] = mx;
        });

    return out;
}

std::vector<double> power(std::span<const double> data, size_t n_windows, uint32_t hop_size, uint32_t window_size)
{
    std::vector<double> out(n_windows);
    std::vector<size_t> idx(n_windows);
    std::ranges::iota(idx, 0);

    Parallel::for_each(Parallel::par_unseq, idx.begin(), idx.end(),
        [&](size_t i) {
            const size_t start = i * hop_size;
            auto w = data.subspan(start, std::min<size_t>(window_size, data.size() - start));
            double sq = 0.0;
            for (double s : w)
                sq += s * s;
            out[i] = sq;
        });

    return out;
}

std::vector<double> dynamic_range(std::span<const double> data, size_t n_windows, uint32_t hop_size, uint32_t window_size)
{
    std::vector<double> out(n_windows);
    std::vector<size_t> idx(n_windows);
    std::ranges::iota(idx, 0);

    Parallel::for_each(Parallel::par_unseq, idx.begin(), idx.end(),
        [&](size_t i) {
            const size_t start = i * hop_size;
            auto w = data.subspan(start, std::min<size_t>(window_size, data.size() - start));
            double mn = std::numeric_limits<double>::max();
            double mx = std::numeric_limits<double>::lowest();
            for (double s : w) {
                double a = std::abs(s);
                mn = std::min(mn, a);
                mx = std::max(mx, a);
            }
            out[i] = 20.0 * std::log10(mx / std::max(mn, k_epsilon));
        });

    return out;
}

std::vector<double> zero_crossing_rate(std::span<const double> data, size_t n_windows, uint32_t hop_size, uint32_t window_size)
{
    std::vector<double> out(n_windows);
    std::vector<size_t> idx(n_windows);
    std::ranges::iota(idx, 0);

    Parallel::for_each(Parallel::par_unseq, idx.begin(), idx.end(),
        [&](size_t i) {
            const size_t start = i * hop_size;
            auto w = data.subspan(start, std::min<size_t>(window_size, data.size() - start));
            int zc = 0;
            for (size_t j = 1; j < w.size(); ++j) {
                if ((w[j] >= 0.0) != (w[j - 1] >= 0.0))
                    ++zc;
            }
            out[i] = static_cast<double>(zc) / static_cast<double>(w.size() - 1);
        });

    return out;
}

std::vector<double> spectral_energy(std::span<const double> data, size_t n_windows, uint32_t hop_size, uint32_t window_size)
{
    std::vector<double> out(n_windows);
    std::vector<size_t> idx(n_windows);
    std::ranges::iota(idx, 0);

    const Eigen::VectorXd hw = hann_window(window_size);

    Parallel::for_each(Parallel::par_unseq, idx.begin(), idx.end(),
        [&](size_t i) {
            const size_t start = i * hop_size;
            auto w = data.subspan(start, std::min<size_t>(window_size, data.size() - start));

            Eigen::VectorXd buf = Eigen::VectorXd::Zero(window_size);
            for (size_t j = 0; j < w.size(); ++j)
                buf(static_cast<Eigen::Index>(j)) = w[j] * hw(static_cast<Eigen::Index>(j));

            Eigen::FFT<double> fft;
            Eigen::VectorXcd spec;
            fft.fwd(spec, buf);

            double e = 0.0;
            for (Eigen::Index j = 0; j < spec.size(); ++j)
                e += std::norm(spec(j));

            out[i] = e / static_cast<double>(window_size);
        });

    return out;
}

std::vector<double> low_frequency_energy(std::span<const double> data, size_t n_windows, uint32_t hop_size, uint32_t window_size, double low_bin_fraction)
{
    std::vector<double> out(n_windows);
    std::vector<size_t> idx(n_windows);
    std::ranges::iota(idx, 0);

    const Eigen::VectorXd hw = hann_window(window_size);
    const int low_bins = std::max(1, static_cast<int>(static_cast<double>((double)window_size / 2) * low_bin_fraction));

    Parallel::for_each(Parallel::par_unseq, idx.begin(), idx.end(),
        [&](size_t i) {
            const size_t start = i * hop_size;
            auto w = data.subspan(start, std::min<size_t>(window_size, data.size() - start));

            Eigen::VectorXd buf = Eigen::VectorXd::Zero(window_size);
            for (size_t j = 0; j < w.size(); ++j)
                buf(static_cast<Eigen::Index>(j)) = w[j] * hw(static_cast<Eigen::Index>(j));

            Eigen::FFT<double> fft;
            Eigen::VectorXcd spec;
            fft.fwd(spec, buf);

            double e = 0.0;
            for (int j = 1; j < low_bins; ++j)
                e += std::norm(spec(j));

            out[i] = e / static_cast<double>(low_bins);
        });

    return out;
}

// ============================================================================
// Statistics
// ============================================================================

std::vector<double> mean(std::span<const double> data, size_t n_windows, uint32_t hop_size, uint32_t window_size)
{
    std::vector<double> out(n_windows);
    std::vector<size_t> idx(n_windows);
    std::ranges::iota(idx, 0);

    Parallel::for_each(Parallel::par_unseq, idx.begin(), idx.end(),
        [&](size_t i) {
            const size_t start = i * hop_size;
            auto w = data.subspan(start, std::min<size_t>(window_size, data.size() - start));
            double s = 0.0;
            for (double v : w)
                s += v;
            out[i] = s / static_cast<double>(w.size());
        });

    return out;
}

std::vector<double> variance(std::span<const double> data, size_t n_windows, uint32_t hop_size, uint32_t window_size, bool sample_variance)
{
    std::vector<double> out(n_windows);
    std::vector<size_t> idx(n_windows);
    std::ranges::iota(idx, 0);

    Parallel::for_each(Parallel::par_unseq, idx.begin(), idx.end(),
        [&](size_t i) {
            const size_t start = i * hop_size;
            auto w = data.subspan(start, std::min<size_t>(window_size, data.size() - start));
            if (w.size() <= 1) {
                out[i] = 0.0;
                return;
            }
            double s = 0.0;
            for (double v : w)
                s += v;
            const double m = s / static_cast<double>(w.size());
            double sq = 0.0;
            for (double v : w) {
                double d = v - m;
                sq += d * d;
            }
            const double div = sample_variance
                ? static_cast<double>(w.size() - 1)
                : static_cast<double>(w.size());
            out[i] = sq / div;
        });

    return out;
}

std::vector<double> std_dev(std::span<const double> data, size_t n_windows, uint32_t hop_size, uint32_t window_size, bool sample_variance)
{
    auto v = variance(data, n_windows, hop_size, window_size, sample_variance);
    Parallel::transform(Parallel::par_unseq, v.begin(), v.end(), v.begin(),
        [](double x) { return std::sqrt(x); });
    return v;
}

std::vector<double> skewness(std::span<const double> data, size_t n_windows, uint32_t hop_size, uint32_t window_size)
{
    std::vector<double> out(n_windows);
    std::vector<size_t> idx(n_windows);
    std::ranges::iota(idx, 0);

    Parallel::for_each(Parallel::par_unseq, idx.begin(), idx.end(),
        [&](size_t i) {
            const size_t start = i * hop_size;
            auto w = data.subspan(start, std::min<size_t>(window_size, data.size() - start));
            if (w.size() < 2) {
                out[i] = 0.0;
                return;
            }
            double s = 0.0;
            for (double v : w)
                s += v;
            const double m = s / static_cast<double>(w.size());
            double sq = 0.0, cb = 0.0;
            for (double v : w) {
                const double d = v - m;
                const double d2 = d * d;
                sq += d2;
                cb += d2 * d;
            }
            const double var = sq / static_cast<double>(w.size());
            const double sd = std::sqrt(std::max(var, k_epsilon));
            out[i] = (cb / static_cast<double>(w.size())) / (sd * sd * sd);
        });

    return out;
}

std::vector<double> kurtosis(std::span<const double> data, size_t n_windows, uint32_t hop_size, uint32_t window_size)
{
    std::vector<double> out(n_windows);
    std::vector<size_t> idx(n_windows);
    std::ranges::iota(idx, 0);

    Parallel::for_each(Parallel::par_unseq, idx.begin(), idx.end(),
        [&](size_t i) {
            const size_t start = i * hop_size;
            auto w = data.subspan(start, std::min<size_t>(window_size, data.size() - start));
            if (w.size() < 2) {
                out[i] = 0.0;
                return;
            }
            double s = 0.0;
            for (double v : w)
                s += v;
            const double m = s / static_cast<double>(w.size());
            double sq = 0.0, fo = 0.0;
            for (double v : w) {
                const double d = v - m;
                const double d2 = d * d;
                sq += d2;
                fo += d2 * d2;
            }
            const double var = sq / static_cast<double>(w.size());
            const double var2 = std::max(var * var, k_epsilon);
            out[i] = (fo / static_cast<double>(w.size())) / var2 - 3.0;
        });

    return out;
}

std::vector<double> median(std::span<const double> data, size_t n_windows, uint32_t hop_size, uint32_t window_size)
{
    std::vector<double> out(n_windows);
    std::vector<size_t> idx(n_windows);
    std::ranges::iota(idx, 0);

    Parallel::for_each(Parallel::par_unseq, idx.begin(), idx.end(),
        [&](size_t i) {
            const size_t start = i * hop_size;
            auto w = data.subspan(start, std::min<size_t>(window_size, data.size() - start));
            std::vector<double> buf(w.begin(), w.end());
            const size_t mid = buf.size() / 2;
            std::nth_element(buf.begin(), buf.begin() + static_cast<ptrdiff_t>(mid), buf.end());
            if (buf.size() % 2 != 0) {
                out[i] = buf[mid];
            } else {
                const double upper = buf[mid];
                std::nth_element(buf.begin(), buf.begin() + static_cast<ptrdiff_t>(mid - 1), buf.begin() + static_cast<ptrdiff_t>(mid));
                out[i] = (buf[mid - 1] + upper) / 2.0;
            }
        });

    return out;
}

std::vector<double> percentile(std::span<const double> data, size_t n_windows, uint32_t hop_size, uint32_t window_size, double percentile_value)
{
    std::vector<double> out(n_windows);
    std::vector<size_t> idx(n_windows);
    std::ranges::iota(idx, 0);

    Parallel::for_each(Parallel::par_unseq, idx.begin(), idx.end(),
        [&](size_t i) {
            const size_t start = i * hop_size;
            auto w = data.subspan(start, std::min<size_t>(window_size, data.size() - start));
            if (w.empty()) {
                out[i] = 0.0;
                return;
            }
            std::vector<double> buf(w.begin(), w.end());
            std::ranges::sort(buf);
            const double pos = (percentile_value / 100.0) * static_cast<double>(buf.size() - 1);
            const auto lo = static_cast<size_t>(std::floor(pos));
            const auto hi = static_cast<size_t>(std::ceil(pos));
            out[i] = (lo == hi) ? buf[lo] : buf[lo] * (1.0 - (pos - static_cast<double>(lo))) + buf[hi] * (pos - static_cast<double>(lo));
        });

    return out;
}

std::vector<double> entropy(std::span<const double> data, size_t n_windows, uint32_t hop_size, uint32_t window_size, size_t num_bins)
{
    std::vector<double> out(n_windows);
    std::vector<size_t> idx(n_windows);
    std::ranges::iota(idx, 0);

    Parallel::for_each(Parallel::par_unseq, idx.begin(), idx.end(),
        [&](size_t i) {
            const size_t start = i * hop_size;
            auto w = data.subspan(start, std::min<size_t>(window_size, data.size() - start));
            if (w.empty()) {
                out[i] = 0.0;
                return;
            }
            size_t bins = num_bins;
            if (bins == 0) {
                bins = std::clamp(
                    static_cast<size_t>(std::ceil(std::log2(static_cast<double>(w.size())) + 1.0)),
                    size_t { 1 }, w.size());
            }
            const auto [mn, mx] = std::ranges::minmax(w);
            if (mx <= mn) {
                out[i] = 0.0;
                return;
            }
            const double bw = (mx - mn) / static_cast<double>(bins);
            std::vector<size_t> counts(bins, 0);
            for (double v : w) {
                auto b = static_cast<size_t>((v - mn) / bw);
                counts[std::min(b, bins - 1)]++;
            }
            double e = 0.0;
            const auto n = static_cast<double>(w.size());
            for (size_t c : counts) {
                if (c > 0) {
                    const double p = static_cast<double>(c) / n;
                    e -= p * std::log2(p);
                }
            }
            out[i] = e;
        });

    return out;
}

std::vector<double> min(std::span<const double> data, size_t n_windows, uint32_t hop_size, uint32_t window_size)
{
    std::vector<double> out(n_windows);
    std::vector<size_t> idx(n_windows);
    std::ranges::iota(idx, 0);

    Parallel::for_each(Parallel::par_unseq, idx.begin(), idx.end(),
        [&](size_t i) {
            const size_t start = i * hop_size;
            auto w = data.subspan(start, std::min<size_t>(window_size, data.size() - start));
            out[i] = *std::ranges::min_element(w);
        });

    return out;
}

std::vector<double> max(std::span<const double> data, size_t n_windows, uint32_t hop_size, uint32_t window_size)
{
    std::vector<double> out(n_windows);
    std::vector<size_t> idx(n_windows);
    std::ranges::iota(idx, 0);

    Parallel::for_each(Parallel::par_unseq, idx.begin(), idx.end(),
        [&](size_t i) {
            const size_t start = i * hop_size;
            auto w = data.subspan(start, std::min<size_t>(window_size, data.size() - start));
            out[i] = *std::ranges::max_element(w);
        });

    return out;
}

std::vector<double> range(std::span<const double> data, size_t n_windows, uint32_t hop_size, uint32_t window_size)
{
    std::vector<double> out(n_windows);
    std::vector<size_t> idx(n_windows);
    std::ranges::iota(idx, 0);

    Parallel::for_each(Parallel::par_unseq, idx.begin(), idx.end(),
        [&](size_t i) {
            const size_t start = i * hop_size;
            auto w = data.subspan(start, std::min<size_t>(window_size, data.size() - start));
            const auto [mn, mx] = std::ranges::minmax(w);
            out[i] = mx - mn;
        });

    return out;
}

std::vector<double> sum(std::span<const double> data, size_t n_windows, uint32_t hop_size, uint32_t window_size)
{
    std::vector<double> out(n_windows);
    std::vector<size_t> idx(n_windows);
    std::ranges::iota(idx, 0);

    Parallel::for_each(Parallel::par_unseq, idx.begin(), idx.end(),
        [&](size_t i) {
            const size_t start = i * hop_size;
            auto w = data.subspan(start, std::min<size_t>(window_size, data.size() - start));
            out[i] = std::accumulate(w.begin(), w.end(), 0.0);
        });

    return out;
}

std::vector<double> count(std::span<const double> data, size_t n_windows, uint32_t hop_size, uint32_t window_size)
{
    std::vector<double> out(n_windows);
    std::vector<size_t> idx(n_windows);
    std::ranges::iota(idx, 0);

    Parallel::for_each(Parallel::par_unseq, idx.begin(), idx.end(),
        [&](size_t i) {
            const size_t start = i * hop_size;
            const size_t end = std::min(start + window_size, data.size());
            out[i] = static_cast<double>(end - start);
        });

    return out;
}

std::vector<double> mad(std::span<const double> data, size_t n_windows, uint32_t hop_size, uint32_t window_size)
{
    std::vector<double> out(n_windows);
    std::vector<size_t> idx(n_windows);
    std::ranges::iota(idx, 0);

    Parallel::for_each(Parallel::par_unseq, idx.begin(), idx.end(),
        [&](size_t i) {
            const size_t start = i * hop_size;
            auto w = data.subspan(start, std::min<size_t>(window_size, data.size() - start));
            if (w.empty()) {
                out[i] = 0.0;
                return;
            }
            std::vector<double> buf(w.begin(), w.end());
            const size_t mid = buf.size() / 2;
            std::nth_element(buf.begin(), buf.begin() + static_cast<ptrdiff_t>(mid), buf.end());
            const double med = (buf.size() % 2 != 0)
                ? buf[mid]
                : [&] {
                      const double upper = buf[mid];
                      std::nth_element(buf.begin(), buf.begin() + static_cast<ptrdiff_t>(mid - 1), buf.begin() + static_cast<ptrdiff_t>(mid));
                      return (buf[mid - 1] + upper) / 2.0;
                  }();

            std::vector<double> dev;
            dev.reserve(w.size());
            for (double v : w)
                dev.push_back(std::abs(v - med));

            const size_t dm = dev.size() / 2;
            std::nth_element(dev.begin(), dev.begin() + static_cast<ptrdiff_t>(dm), dev.end());
            out[i] = (dev.size() % 2 != 0)
                ? dev[dm]
                : [&] {
                      const double upper = dev[dm];
                      std::nth_element(dev.begin(), dev.begin() + static_cast<ptrdiff_t>(dm - 1), dev.begin() + static_cast<ptrdiff_t>(dm));
                      return (dev[dm - 1] + upper) / 2.0;
                  }();
        });

    return out;
}

std::vector<double> coefficient_of_variation(std::span<const double> data, size_t n_windows, uint32_t hop_size, uint32_t window_size, bool sample_variance)
{
    auto m = mean(data, n_windows, hop_size, window_size);
    auto s = std_dev(data, n_windows, hop_size, window_size, sample_variance);

    std::vector<double> out(n_windows);
    Parallel::transform(Parallel::par_unseq, m.begin(), m.end(), s.begin(), out.begin(),
        [](double mv, double sv) {
            return (std::abs(mv) > 1e-15) ? sv / mv : 0.0;
        });

    return out;
}

std::vector<double> mode(std::span<const double> data, size_t n_windows, uint32_t hop_size, uint32_t window_size)
{
    std::vector<double> out(n_windows);
    std::vector<size_t> idx(n_windows);
    std::ranges::iota(idx, 0);

    constexpr double tol = 1e-10;

    Parallel::for_each(Parallel::par_unseq, idx.begin(), idx.end(),
        [&](size_t i) {
            const size_t start = i * hop_size;
            auto w = data.subspan(start, std::min<size_t>(window_size, data.size() - start));
            if (w.empty()) {
                out[i] = 0.0;
                return;
            }
            std::map<int64_t, std::pair<double, size_t>> freq;
            for (double v : w) {
                const auto bucket = static_cast<int64_t>(std::round(v / tol));
                auto& [sum_v, cnt] = freq[bucket];
                sum_v = (sum_v * static_cast<double>(cnt) + v) / static_cast<double>(cnt + 1);
                ++cnt;
            }
            const auto it = std::ranges::max_element(freq,
                [](const auto& a, const auto& b) { return a.second.second < b.second.second; });
            out[i] = it->second.first;
        });

    return out;
}

std::vector<double> mean_zscore(std::span<const double> data, size_t n_windows, uint32_t hop_size, uint32_t window_size, bool sample_variance)
{
    std::vector<double> out(n_windows);
    std::vector<size_t> idx(n_windows);
    std::ranges::iota(idx, 0);

    Parallel::for_each(Parallel::par_unseq, idx.begin(), idx.end(),
        [&](size_t i) {
            const size_t start = i * hop_size;
            auto w = data.subspan(start, std::min<size_t>(window_size, data.size() - start));
            if (w.empty()) {
                out[i] = 0.0;
                return;
            }
            double s = 0.0;
            for (double v : w)
                s += v;
            const double m = s / static_cast<double>(w.size());
            double sq = 0.0;
            for (double v : w) {
                const double d = v - m;
                sq += d * d;
            }
            const double div = sample_variance
                ? static_cast<double>(w.size() - 1)
                : static_cast<double>(w.size());
            const double sd = std::sqrt(sq / div);
            if (sd <= 0.0) {
                out[i] = 0.0;
                return;
            }
            double zs = 0.0;
            for (double v : w)
                zs += (v - m) / sd;
            out[i] = zs / static_cast<double>(w.size());
        });

    return out;
}

// ============================================================================
// Position finders
// ============================================================================

std::vector<size_t> zero_crossing_positions(std::span<const double> data, double threshold)
{
    std::vector<size_t> pos;
    pos.reserve(data.size() / 4);
    for (size_t i = 1; i < data.size(); ++i) {
        if ((data[i] >= threshold) != (data[i - 1] >= threshold))
            pos.push_back(i);
    }
    pos.shrink_to_fit();
    return pos;
}

std::vector<size_t> peak_positions(std::span<const double> data, double threshold, size_t min_distance)
{
    if (data.size() < 3)
        return {};

    std::vector<size_t> pos;
    pos.reserve(data.size() / 100);
    size_t last = 0;

    for (size_t i = 1; i + 1 < data.size(); ++i) {
        const double a = std::abs(data[i]);
        if (a > threshold
            && a >= std::abs(data[i - 1])
            && a >= std::abs(data[i + 1])
            && (pos.empty() || (i - last) >= min_distance)) {
            pos.push_back(i);
            last = i;
        }
    }

    pos.shrink_to_fit();
    return pos;
}

std::vector<size_t> onset_positions(std::span<const double> data, uint32_t window_size, uint32_t hop_size, double threshold)
{
    const size_t nw = num_windows(data.size(), window_size, hop_size);
    if (nw < 2)
        return {};

    const Eigen::VectorXd hw = hann_window(window_size);
    std::vector<double> flux(nw - 1, 0.0);
    Eigen::VectorXcd prev_spec;

    for (size_t i = 0; i < nw; ++i) {
        const size_t start = i * hop_size;
        auto w = data.subspan(start, std::min<size_t>(window_size, data.size() - start));

        Eigen::VectorXd buf = Eigen::VectorXd::Zero(window_size);
        for (size_t j = 0; j < w.size(); ++j)
            buf(static_cast<Eigen::Index>(j)) = w[j] * hw(static_cast<Eigen::Index>(j));

        Eigen::FFT<double> fft;
        Eigen::VectorXcd spec;
        fft.fwd(spec, buf);

        if (i > 0) {
            double f = 0.0;
            for (Eigen::Index j = 0; j < spec.size(); ++j) {
                const double diff = std::abs(spec(j)) - std::abs(prev_spec(j));
                if (diff > 0.0)
                    f += diff;
            }
            flux[i - 1] = f;
        }
        prev_spec = spec;
    }

    const double mx = *std::ranges::max_element(flux);
    if (mx > 0.0) {
        for (double& f : flux) {
            f /= mx;
        }
    }

    std::vector<size_t> pos;
    for (size_t i = 1; i + 1 < flux.size(); ++i) {
        if (flux[i] > threshold && flux[i] > flux[i - 1] && flux[i] >= flux[i + 1])
            pos.push_back((i + 1) * hop_size);
    }

    return pos;
}

} // namespace MayaFlux::Kinesis::Discrete
