#include "Coefficients.hpp"

#include "Taper.hpp"

#include <Eigen/Dense>

namespace MayaFlux::Kinesis::Discrete {

namespace {

    struct BiquadTerms {
        double w0;
        double cosw0;
        double sinw0;
        double alpha;
    };

    [[nodiscard]] BiquadTerms terms(double frequency, double q, double sample_rate)
    {
        const double nyquist = sample_rate * 0.5;
        const double f = std::clamp(frequency, 1.0, nyquist - 1.0);
        const double qq = std::max(q, 1e-4);
        const double w0 = 2.0 * std::numbers::pi * f / sample_rate;
        const double sinw0 = std::sin(w0);
        return { .w0 = w0, .cosw0 = std::cos(w0), .sinw0 = sinw0, .alpha = sinw0 / (2.0 * qq) };
    }

    void emit(std::vector<double>& a, std::vector<double>& b,
        double a0, double a1, double a2,
        double b0, double b1, double b2)
    {
        a.assign({ 1.0, a1 / a0, a2 / a0 });
        b.assign({ b0 / a0, b1 / a0, b2 / a0 });
    }

    [[nodiscard]] double db_to_amplitude_sqrt(double gain_db)
    {
        return std::pow(10.0, gain_db / 40.0);
    }

    [[nodiscard]] double sinc(double x)
    {
        if (std::abs(x) < 1e-12)
            return 1.0;
        const double px = std::numbers::pi * x;
        return std::sin(px) / px;
    }

    [[nodiscard]] size_t force_odd(size_t n)
    {
        if (n < 3)
            return 3;
        return (n % 2 == 0) ? n + 1 : n;
    }

    [[nodiscard]] std::vector<double> lowpass_kernel(double frequency, double sample_rate, size_t taps)
    {
        const double nyquist = sample_rate * 0.5;
        const double f = std::clamp(frequency, 1.0, nyquist - 1.0);
        const double fc = f / sample_rate;
        const double centre = static_cast<double>(taps - 1) * 0.5;

        std::vector<double> h(taps);
        for (size_t i = 0; i < taps; ++i)
            h[i] = 2.0 * fc * sinc(2.0 * fc * (static_cast<double>(i) - centre));

        apply_hann(h);

        double total = 0.0;
        for (double v : h)
            total += v;
        if (std::abs(total) > 1e-12) {
            for (double& v : h)
                v /= total;
        }
        return h;
    }

} // namespace

void biquad_lowpass(double frequency, double q, double sample_rate,
    std::vector<double>& a, std::vector<double>& b)
{
    const auto t = terms(frequency, q, sample_rate);
    const double k = (1.0 - t.cosw0) * 0.5;
    emit(a, b,
        1.0 + t.alpha, -2.0 * t.cosw0, 1.0 - t.alpha,
        k, 1.0 - t.cosw0, k);
}

void biquad_highpass(double frequency, double q, double sample_rate,
    std::vector<double>& a, std::vector<double>& b)
{
    const auto t = terms(frequency, q, sample_rate);
    const double k = (1.0 + t.cosw0) * 0.5;
    emit(a, b,
        1.0 + t.alpha, -2.0 * t.cosw0, 1.0 - t.alpha,
        k, -(1.0 + t.cosw0), k);
}

void biquad_bandpass(double frequency, double q, double sample_rate,
    std::vector<double>& a, std::vector<double>& b)
{
    const auto t = terms(frequency, q, sample_rate);
    emit(a, b,
        1.0 + t.alpha, -2.0 * t.cosw0, 1.0 - t.alpha,
        t.alpha, 0.0, -t.alpha);
}

void biquad_notch(double frequency, double q, double sample_rate,
    std::vector<double>& a, std::vector<double>& b)
{
    const auto t = terms(frequency, q, sample_rate);
    emit(a, b,
        1.0 + t.alpha, -2.0 * t.cosw0, 1.0 - t.alpha,
        1.0, -2.0 * t.cosw0, 1.0);
}

void biquad_allpass(double frequency, double q, double sample_rate,
    std::vector<double>& a, std::vector<double>& b)
{
    const auto t = terms(frequency, q, sample_rate);
    emit(a, b,
        1.0 + t.alpha, -2.0 * t.cosw0, 1.0 - t.alpha,
        1.0 - t.alpha, -2.0 * t.cosw0, 1.0 + t.alpha);
}

void biquad_peaking(double frequency, double q, double gain_db,
    double sample_rate, std::vector<double>& a, std::vector<double>& b)
{
    const auto t = terms(frequency, q, sample_rate);
    const double amp = db_to_amplitude_sqrt(gain_db);
    emit(a, b,
        1.0 + t.alpha / amp, -2.0 * t.cosw0, 1.0 - t.alpha / amp,
        1.0 + t.alpha * amp, -2.0 * t.cosw0, 1.0 - t.alpha * amp);
}

void biquad_low_shelf(double frequency, double slope, double gain_db,
    double sample_rate, std::vector<double>& a, std::vector<double>& b)
{
    const double amp = db_to_amplitude_sqrt(gain_db);
    const double nyquist = sample_rate * 0.5;
    const double f = std::clamp(frequency, 1.0, nyquist - 1.0);
    const double w0 = 2.0 * std::numbers::pi * f / sample_rate;
    const double cosw0 = std::cos(w0);
    const double s = std::clamp(slope, 1e-4, 1.0);
    const double alpha = std::sin(w0) * 0.5
        * std::sqrt((amp + 1.0 / amp) * (1.0 / s - 1.0) + 2.0);
    const double sq = 2.0 * std::sqrt(amp) * alpha;

    emit(a, b,
        (amp + 1.0) + (amp - 1.0) * cosw0 + sq,
        -2.0 * ((amp - 1.0) + (amp + 1.0) * cosw0),
        (amp + 1.0) + (amp - 1.0) * cosw0 - sq,
        amp * ((amp + 1.0) - (amp - 1.0) * cosw0 + sq),
        2.0 * amp * ((amp - 1.0) - (amp + 1.0) * cosw0),
        amp * ((amp + 1.0) - (amp - 1.0) * cosw0 - sq));
}

void biquad_high_shelf(double frequency, double slope, double gain_db,
    double sample_rate, std::vector<double>& a, std::vector<double>& b)
{
    const double amp = db_to_amplitude_sqrt(gain_db);
    const double nyquist = sample_rate * 0.5;
    const double f = std::clamp(frequency, 1.0, nyquist - 1.0);
    const double w0 = 2.0 * std::numbers::pi * f / sample_rate;
    const double cosw0 = std::cos(w0);
    const double s = std::clamp(slope, 1e-4, 1.0);
    const double alpha = std::sin(w0) * 0.5
        * std::sqrt((amp + 1.0 / amp) * (1.0 / s - 1.0) + 2.0);
    const double sq = 2.0 * std::sqrt(amp) * alpha;

    emit(a, b,
        (amp + 1.0) - (amp - 1.0) * cosw0 + sq,
        2.0 * ((amp - 1.0) - (amp + 1.0) * cosw0),
        (amp + 1.0) - (amp - 1.0) * cosw0 - sq,
        amp * ((amp + 1.0) + (amp - 1.0) * cosw0 + sq),
        -2.0 * amp * ((amp - 1.0) + (amp + 1.0) * cosw0),
        amp * ((amp + 1.0) + (amp - 1.0) * cosw0 - sq));
}

std::vector<double> sinc_lowpass(double frequency, double sample_rate, size_t taps)
{
    return lowpass_kernel(frequency, sample_rate, force_odd(taps));
}

std::vector<double> sinc_highpass(double frequency, double sample_rate, size_t taps)
{
    const size_t n = force_odd(taps);
    std::vector<double> h = lowpass_kernel(frequency, sample_rate, n);

    for (double& v : h)
        v = -v;
    h[(n - 1) / 2] += 1.0;

    return h;
}

std::vector<double> sinc_bandpass(double low_hz, double high_hz, double sample_rate, size_t taps)
{
    double lo = low_hz;
    double hi = high_hz;
    if (lo > hi)
        std::swap(lo, hi);

    const size_t n = force_odd(taps);
    std::vector<double> upper = lowpass_kernel(hi, sample_rate, n);
    const std::vector<double> lower = lowpass_kernel(lo, sample_rate, n);

    for (size_t i = 0; i < n; ++i)
        upper[i] -= lower[i];

    return upper;
}

std::vector<double> savitzky_golay(size_t window, size_t poly_order, size_t derivative)
{
    const size_t n = force_odd(window);
    if (n <= poly_order || derivative > poly_order)
        return {};

    const auto rows = static_cast<Eigen::Index>(n);
    const auto cols = static_cast<Eigen::Index>(poly_order) + 1;
    const double half = static_cast<double>(n - 1) * 0.5;

    Eigen::MatrixXd vandermonde(rows, cols);
    for (Eigen::Index i = 0; i < rows; ++i) {
        const double z = static_cast<double>(i) - half;
        double p = 1.0;
        for (Eigen::Index j = 0; j < cols; ++j) {
            vandermonde(i, j) = p;
            p *= z;
        }
    }

    const Eigen::MatrixXd pseudo = vandermonde
                                       .completeOrthogonalDecomposition()
                                       .pseudoInverse();

    double factorial = 1.0;
    for (size_t k = 2; k <= derivative; ++k)
        factorial *= static_cast<double>(k);

    const Eigen::VectorXd row = pseudo.row(static_cast<Eigen::Index>(derivative)) * factorial;

    std::vector<double> h(n);
    for (size_t i = 0; i < n; ++i)
        h[i] = row(static_cast<Eigen::Index>(n - 1 - i));

    return h;
}

std::vector<double> lagrange_delay(double delay, size_t order)
{
    const size_t taps = order + 1;
    const double d = std::clamp(delay, 0.0, static_cast<double>(order));

    std::vector<double> h(taps, 1.0);
    for (size_t k = 0; k < taps; ++k) {
        for (size_t j = 0; j < taps; ++j) {
            if (j == k)
                continue;
            h[k] *= (d - static_cast<double>(j))
                / (static_cast<double>(k) - static_cast<double>(j));
        }
    }
    return h;
}

void resonator(double pole_radius, double pole_angle,
    std::vector<double>& a, std::vector<double>& b)
{
    const double r = std::clamp(pole_radius, 0.0, 0.9999);
    const double theta = std::clamp(pole_angle, 0.0, std::numbers::pi);
    const double cos_theta = std::cos(theta);

    a.assign({ 1.0, -2.0 * r * cos_theta, r * r });

    const double gain = (1.0 - r)
        * std::sqrt(1.0 - 2.0 * r * std::cos(2.0 * theta) + r * r);
    b.assign({ gain });
}

void one_pole(double pole, std::vector<double>& a, std::vector<double>& b)
{
    const double p = std::clamp(pole, -0.9999, 0.9999);
    a.assign({ 1.0, -p });
    b.assign({ 1.0 - std::abs(p) });
}

void dc_block(double pole, std::vector<double>& a, std::vector<double>& b)
{
    const double p = std::clamp(pole, 0.0, 0.9999);
    a.assign({ 1.0, -p });
    b.assign({ 1.0, -1.0 });
}

std::vector<double> cascade(std::span<const double> lhs, std::span<const double> rhs)
{
    if (lhs.empty() || rhs.empty())
        return {};

    std::vector<double> out(lhs.size() + rhs.size() - 1, 0.0);
    for (size_t i = 0; i < lhs.size(); ++i) {
        for (size_t j = 0; j < rhs.size(); ++j)
            out[i + j] += lhs[i] * rhs[j];
    }
    return out;
}

double max_pole_magnitude(std::span<const double> a)
{
    size_t order = a.size();
    while (order > 1 && a[order - 1] == 0.0)
        --order;

    if (order <= 1)
        return 0.0;

    const double a0 = a[0];

    if (order == 2)
        return std::abs(a[1] / a0);

    if (order == 3) {
        const double p = a[1] / a0;
        const double q = a[2] / a0;
        const double disc = p * p - 4.0 * q;
        if (disc >= 0.0) {
            const double r = std::sqrt(disc);
            return std::max(std::abs((-p + r) * 0.5), std::abs((-p - r) * 0.5));
        }
        return std::sqrt(q);
    }

    const Eigen::Index n = static_cast<Eigen::Index>(order) - 1;
    Eigen::MatrixXd companion = Eigen::MatrixXd::Zero(n, n);
    for (Eigen::Index i = 0; i < n; ++i)
        companion(0, i) = -a[static_cast<size_t>(i) + 1] / a0;
    for (Eigen::Index i = 1; i < n; ++i)
        companion(i, i - 1) = 1.0;

    Eigen::EigenSolver<Eigen::MatrixXd> solver(companion, false);
    double largest = 0.0;
    for (Eigen::Index i = 0; i < solver.eigenvalues().size(); ++i)
        largest = std::max(largest, std::abs(solver.eigenvalues()(i)));
    return largest;
}

} // namespace MayaFlux::Kinesis::Discrete
