#include "Coefficients.hpp"

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
