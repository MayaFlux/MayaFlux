#include "Kernels.hpp"

#include "MayaFlux/Kinesis/Scalar.hpp"
#include "MayaFlux/Kinesis/Spatial/Lattice.hpp"
#include "MayaFlux/Kinesis/Stochastic/Estimate.hpp"

namespace MayaFlux::Kinesis::Discrete {

namespace {

    constexpr double two_pi = 2.0 * std::numbers::pi;

    [[nodiscard]] double interpolate_table(std::span<const double> table, double x) noexcept
    {
        return sample_table(table, (std::clamp(x, -1.0, 1.0) + 1.0) * 0.5);
    }

    inline void wrap_revolution(double& phase) noexcept
    {
        if (phase >= two_pi) {
            phase -= two_pi;
        } else if (phase < 0.0) {
            phase += two_pi;
        }
    }

    [[nodiscard]] double sum_of(std::span<const double> values) noexcept
    {
        const double* data = values.data();
        const size_t count = values.size();

        double s0 = 0.0;
        double s1 = 0.0;
        double s2 = 0.0;
        double s3 = 0.0;
        size_t k = 0;
        for (; k + 4 <= count; k += 4) {
            s0 += data[k];
            s1 += data[k + 1];
            s2 += data[k + 2];
            s3 += data[k + 3];
        }

        double tail = 0.0;
        for (; k < count; ++k)
            tail += data[k];
        return ((s0 + s1) + (s2 + s3)) + tail;
    }

    void rotate_rotors(
        const double* __restrict cos_step, const double* __restrict sin_step,
        double* __restrict re, double* __restrict im, size_t n) noexcept
    {
        for (size_t i = 0; i < n; ++i) {
            const double next_re = re[i] * cos_step[i] - im[i] * sin_step[i];
            const double next_im = re[i] * sin_step[i] + im[i] * cos_step[i];
            const double correction = 1.5 - 0.5 * (next_re * next_re + next_im * next_im);

            re[i] = next_re * correction;
            im[i] = next_im * correction;
        }
    }

    void logistic_map(
        const double* __restrict cells, double* __restrict mapped, double r, size_t n) noexcept
    {
        for (size_t i = 0; i < n; ++i)
            mapped[i] = r * cells[i] * (1.0 - cells[i]);
    }

    void blend_neighbours(
        double* __restrict cells, const double* __restrict mapped, double epsilon, size_t n) noexcept
    {
        const double keep = 1.0 - epsilon;
        const double share = 0.5 * epsilon;

        if (n == 1) {
            cells[0] = keep * mapped[0] + share * (mapped[0] + mapped[0]);
            return;
        }

        cells[0] = keep * mapped[0] + share * (mapped[n - 1] + mapped[1]);
        for (size_t i = 1; i + 1 < n; ++i)
            cells[i] = keep * mapped[i] + share * (mapped[i - 1] + mapped[i + 1]);
        cells[n - 1] = keep * mapped[n - 1] + share * (mapped[n - 2] + mapped[0]);
    }

}

double horner(std::span<const double> coefficients, double x) noexcept
{
    double result = 0.0;
    for (const double c : coefficients)
        result = result * x + c;
    return result;
}

double weighted_sum(std::span<const double> window, std::span<const double> coefs) noexcept
{
    const size_t count = std::min(window.size(), coefs.size());
    const double* a = window.data();
    const double* b = coefs.data();

    double s0 = 0.0;
    double s1 = 0.0;
    double s2 = 0.0;
    double s3 = 0.0;
    size_t k = 0;
    for (; k + 4 <= count; k += 4) {
        s0 += a[k] * b[k];
        s1 += a[k + 1] * b[k + 1];
        s2 += a[k + 2] * b[k + 2];
        s3 += a[k + 3] * b[k + 3];
    }

    double tail = 0.0;
    for (; k < count; ++k)
        tail += a[k] * b[k];
    return ((s0 + s1) + (s2 + s3)) + tail;
}

double tapped_sum(std::span<const double> window, std::span<const double> coefs) noexcept
{
    if (window.empty())
        return 0.0;

    const auto reach = static_cast<double>(window.size() - 1);
    double sum = 0.0;

    for (size_t j = 0; j + 1 < coefs.size(); j += 2) {
        const double lag = coefs[j];
        if (!(lag >= 0.0) || lag > reach)
            continue;

        const auto index = static_cast<size_t>(lag);
        const double fraction = lag - static_cast<double>(index);
        double value = window[index];
        if (fraction > 0.0 && index + 1 < window.size())
            value += (window[index + 1] - value) * fraction;

        sum += coefs[j + 1] * value;
    }
    return sum;
}

double table_lookup(std::span<const double> window, std::span<const double> coefs) noexcept
{
    return interpolate_table(coefs, window.empty() ? 0.0 : window.front());
}

double table_lookup_previous(std::span<const double> window, std::span<const double> coefs) noexcept
{
    return interpolate_table(coefs, window.size() > 1 ? window[1] : 0.0);
}

double chebyshev_series(std::span<const double> window, std::span<const double> coefs) noexcept
{
    if (coefs.empty())
        return 0.0;

    const double x = std::clamp(window.empty() ? 0.0 : window.front(), -1.0, 1.0);
    double b1 = 0.0;
    double b2 = 0.0;

    for (size_t k = coefs.size() - 1; k >= 1; --k) {
        const double b0 = coefs[k] + 2.0 * x * b1 - b2;
        b2 = b1;
        b1 = b0;
    }
    return coefs.front() + x * b1 - b2;
}

double iterated_polynomial(std::span<const double> window, std::span<const double> coefs) noexcept
{
    return horner(coefs, window.size() > 1 ? window[1] : 0.0);
}

std::vector<double> phasor_bank_state(
    std::span<const double> increments,
    std::span<const double> scales,
    std::span<const double> am_increments,
    std::span<const double> phases,
    std::span<const double> am_phases)
{
    const size_t n = increments.size();
    std::vector<double> state(1 + 5 * n, 0.0);
    state.front() = static_cast<double>(n);

    for (size_t i = 0; i < n; ++i) {
        state.at(1 + i) = increments[i];
        state.at(1 + n + i) = i < scales.size() ? scales[i] : 1.0;
        state.at(1 + 2 * n + i) = i < am_increments.size() ? am_increments[i] : 0.0;
        state.at(1 + 3 * n + i) = i < phases.size() ? phases[i] : 0.0;
        state.at(1 + 4 * n + i) = i < am_phases.size() ? am_phases[i] : 0.0;
    }
    return state;
}

std::vector<double> rotor_bank_state(
    std::span<const double> increments,
    std::span<const double> scales,
    std::span<const double> phases)
{
    const size_t n = increments.size();
    std::vector<double> state(1 + 5 * n, 0.0);
    state.front() = static_cast<double>(n);

    for (size_t i = 0; i < n; ++i) {
        const double phase = i < phases.size() ? phases[i] : 0.0;
        state.at(1 + i) = std::cos(increments[i]);
        state.at(1 + n + i) = std::sin(increments[i]);
        state.at(1 + 2 * n + i) = i < scales.size() ? scales[i] : 1.0;
        state.at(1 + 3 * n + i) = std::cos(phase);
        state.at(1 + 4 * n + i) = std::sin(phase);
    }
    return state;
}

double rotor_bank(std::span<const double>, std::span<double> coefs) noexcept
{
    if (coefs.empty())
        return 0.0;

    const auto n = static_cast<size_t>(std::max(coefs.front(), 0.0));
    if (n == 0 || coefs.size() < 1 + 5 * n)
        return 0.0;

    const auto cos_step = coefs.subspan(1, n);
    const auto sin_step = coefs.subspan(1 + n, n);
    const auto scales = coefs.subspan(1 + 2 * n, n);
    const auto re = coefs.subspan(1 + 3 * n, n);
    const auto im = coefs.subspan(1 + 4 * n, n);

    rotate_rotors(cos_step.data(), sin_step.data(), re.data(), im.data(), n);
    return weighted_sum(scales, im);
}

double phasor_bank(std::span<const double>, std::span<double> coefs) noexcept
{
    if (coefs.empty())
        return 0.0;

    const auto n = static_cast<size_t>(std::max(coefs.front(), 0.0));
    if (n == 0 || coefs.size() < 1 + 5 * n)
        return 0.0;

    const auto increments = coefs.subspan(1, n);
    const auto scales = coefs.subspan(1 + n, n);
    const auto am_increments = coefs.subspan(1 + 2 * n, n);
    const auto phases = coefs.subspan(1 + 3 * n, n);
    const auto am_phases = coefs.subspan(1 + 4 * n, n);

    double sum = 0.0;
    for (size_t i = 0; i < n; ++i) {
        phases[i] += increments[i];
        wrap_revolution(phases[i]);

        const double swell = 0.5 + 0.5 * std::sin(am_phases[i]);

        am_phases[i] += am_increments[i];
        wrap_revolution(am_phases[i]);

        sum += scales[i] * swell * std::sin(phases[i]);
    }
    return sum;
}

double polynomial_lags(std::span<const double> window, std::span<const double> coefs) noexcept
{
    if (coefs.size() < 2)
        return 0.0;

    const auto lags = static_cast<size_t>(std::max(coefs[0], 0.0));
    const auto powers = static_cast<size_t>(std::max(coefs[1], 0.0)) + 1;
    if (coefs.size() < 2 + lags * powers)
        return 0.0;

    double sum = 0.0;
    const size_t reach = std::min(lags, window.size());
    for (size_t k = 0; k < reach; ++k) {
        const double w = window[k];
        double power = 1.0;
        for (size_t j = 0; j < powers; ++j) {
            sum += coefs[2 + k * powers + j] * power;
            power *= w;
        }
    }
    return sum;
}

double fourier_series(std::span<const double> window, std::span<const double> coefs) noexcept
{
    if (coefs.empty())
        return 0.0;

    const double phase = window.empty() ? 0.0 : window.front();
    const double c1 = std::cos(phase);
    const double s1 = std::sin(phase);

    double ck = c1;
    double sk = s1;
    double sum = coefs.front();

    for (size_t i = 1; i < coefs.size(); i += 2) {
        sum += coefs[i] * ck;
        if (i + 1 < coefs.size())
            sum += coefs[i + 1] * sk;

        const double next_c = ck * c1 - sk * s1;
        sk = sk * c1 + ck * s1;
        ck = next_c;
    }
    return sum;
}

double breakpoint_curve(std::span<const double> window, std::span<const double> coefs) noexcept
{
    const size_t points = coefs.size() / 2;
    if (points == 0)
        return 0.0;

    const double x = window.empty() ? 0.0 : window.front();
    if (x <= coefs[0])
        return coefs[1];
    if (x >= coefs[2 * (points - 1)])
        return coefs[2 * (points - 1) + 1];

    size_t i = 0;
    while (i + 2 < points && x > coefs[2 * (i + 1)])
        ++i;

    const double x0 = coefs[2 * i];
    const double x1 = coefs[2 * (i + 1)];
    const double y0 = coefs[2 * i + 1];
    const double y1 = coefs[2 * (i + 1) + 1];
    if (x1 <= x0)
        return y0;
    return y0 + (y1 - y0) * (x - x0) / (x1 - x0);
}

double pattern_lookup(std::span<const double> window, std::span<const double> coefs) noexcept
{
    if (coefs.size() < 5 || !(coefs[3] > coefs[2]))
        return 0.0;

    const auto levels = static_cast<uint32_t>(std::max(coefs[0], 1.0));
    const auto count = static_cast<size_t>(std::max(coefs[1], 1.0));
    const auto table = coefs.subspan(4);

    size_t total = 1;
    for (size_t i = 0; i < count; ++i) {
        if (total > table.size() / levels)
            return 0.0;
        total *= levels;
    }

    const Lattice1D lattice { .resolution = levels, .bounds = { .min = coefs[2], .max = coefs[3] } };

    size_t index = 0;
    size_t stride = 1;
    for (size_t i = 0; i < count; ++i) {
        const double value = i < window.size() ? window[i] : 0.0;
        index += static_cast<size_t>(lattice.cell_at(value)) * stride;
        stride *= levels;
    }
    return table[index];
}

double dilate(std::span<const double> window, std::span<const double> coefs) noexcept
{
    const size_t count = std::min(window.size(), coefs.size());
    if (count == 0)
        return 0.0;

    double b0 = window[0] + coefs[0];
    double b1 = b0;
    double b2 = b0;
    double b3 = b0;
    size_t k = 1;
    for (; k + 4 <= count; k += 4) {
        b0 = std::max(b0, window[k] + coefs[k]);
        b1 = std::max(b1, window[k + 1] + coefs[k + 1]);
        b2 = std::max(b2, window[k + 2] + coefs[k + 2]);
        b3 = std::max(b3, window[k + 3] + coefs[k + 3]);
    }
    for (; k < count; ++k)
        b0 = std::max(b0, window[k] + coefs[k]);
    return std::max(std::max(b0, b1), std::max(b2, b3));
}

double erode(std::span<const double> window, std::span<const double> coefs) noexcept
{
    const size_t count = std::min(window.size(), coefs.size());
    if (count == 0)
        return 0.0;

    double b0 = window[0] - coefs[0];
    double b1 = b0;
    double b2 = b0;
    double b3 = b0;
    size_t k = 1;
    for (; k + 4 <= count; k += 4) {
        b0 = std::min(b0, window[k] - coefs[k]);
        b1 = std::min(b1, window[k + 1] - coefs[k + 1]);
        b2 = std::min(b2, window[k + 2] - coefs[k + 2]);
        b3 = std::min(b3, window[k + 3] - coefs[k + 3]);
    }
    for (; k < count; ++k)
        b0 = std::min(b0, window[k] - coefs[k]);
    return std::min(std::min(b0, b1), std::min(b2, b3));
}

double quantile(std::span<const double> window, std::span<const double> coefs)
{
    const size_t n = window.size();
    if (n == 0)
        return 0.0;

    thread_local std::vector<double> scratch;
    scratch.assign(window.begin(), window.end());

    const double q = std::clamp(coefs.empty() ? 0.5 : coefs.front(), 0.0, 1.0);
    const double position = q * static_cast<double>(n - 1);
    const auto lower = static_cast<size_t>(position);
    const double fraction = position - static_cast<double>(lower);

    const auto nth = scratch.begin() + static_cast<std::ptrdiff_t>(lower);
    std::nth_element(scratch.begin(), nth, scratch.end());
    const double low = *nth;

    if (fraction <= 0.0 || lower + 1 >= n)
        return low;

    const double high = *std::min_element(nth + 1, scratch.end());
    return low + (high - low) * fraction;
}

double zscore(std::span<const double> window, std::span<const double> coefs) noexcept
{
    if (window.size() < 3)
        return 0.0;

    const auto past = window.subspan(1);
    const double mean = sum_of(past) / static_cast<double>(past.size());

    const double floor = coefs.empty() ? 1e-12 : std::max(coefs.front(), 1e-12);
    const double deviation = std::max(Stochastic::Estimate::stddev(past), floor);
    return (window.front() - mean) / deviation;
}

double goertzel(std::span<const double> window, std::span<const double> coefs) noexcept
{
    const size_t n = window.size();
    if (n == 0)
        return 0.0;

    const double scale = 4.0 / (static_cast<double>(n) * static_cast<double>(n));
    double total = 0.0;

    for (size_t j = 0; j + 1 < coefs.size(); j += 2) {
        const double coeff = 2.0 * std::cos(two_pi * coefs[j]);
        double s1 = 0.0;
        double s2 = 0.0;
        for (const double x : window) {
            const double s = x + coeff * s1 - s2;
            s2 = s1;
            s1 = s;
        }
        total += coefs[j + 1] * scale * (s1 * s1 + s2 * s2 - coeff * s1 * s2);
    }
    return total;
}

double modular_recurrence(std::span<const double> window, std::span<const double> coefs) noexcept
{
    if (coefs.empty())
        return 0.0;

    const double sum = weighted_sum(window, coefs.subspan(1));
    const double modulus = coefs.front();
    if (modulus <= 0.0)
        return sum;
    return wrap(sum, 0.0, modulus);
}

std::vector<double> kuramoto_state(
    std::span<const double> omegas, double coupling, std::span<const double> phases)
{
    const size_t n = omegas.size();
    std::vector<double> state(2 + 2 * n, 0.0);
    state.front() = static_cast<double>(n);
    state.at(1) = coupling;

    for (size_t i = 0; i < n; ++i) {
        state.at(2 + i) = omegas[i];
        state.at(2 + n + i) = i < phases.size() ? phases[i] : 0.0;
    }
    return state;
}

double kuramoto(std::span<const double>, std::span<double> coefs) noexcept
{
    if (coefs.size() < 2)
        return 0.0;

    const auto n = static_cast<size_t>(std::max(coefs.front(), 0.0));
    if (n == 0 || coefs.size() < 2 + 2 * n)
        return 0.0;

    const double coupling = coefs[1];
    const auto omegas = coefs.subspan(2, n);
    const auto phases = coefs.subspan(2 + n, n);
    const double inverse = 1.0 / static_cast<double>(n);

    constexpr size_t cached = 256;
    std::array<double, cached> cosines;
    std::array<double, cached> sines;
    const bool reuse = n <= cached;

    double field_cos = 0.0;
    double field_sin = 0.0;
    for (size_t i = 0; i < n; ++i) {
        const double c = std::cos(phases[i]);
        const double s = std::sin(phases[i]);
        if (reuse) {
            cosines[i] = c;
            sines[i] = s;
        }
        field_cos += c;
        field_sin += s;
    }
    field_cos *= inverse;
    field_sin *= inverse;

    double mix = 0.0;
    for (size_t i = 0; i < n; ++i) {
        const double c = reuse ? cosines[i] : std::cos(phases[i]);
        const double s = reuse ? sines[i] : std::sin(phases[i]);
        const double pull = coupling * (field_sin * c - field_cos * s);
        phases[i] += omegas[i] + pull;
        wrap_revolution(phases[i]);
        mix += std::sin(phases[i]);
    }
    return mix * inverse;
}

std::vector<double> coupled_map_lattice_state(
    std::span<const double> cells, double r, double epsilon)
{
    const size_t n = cells.size();
    std::vector<double> state(3 + 2 * n, 0.0);
    state.front() = static_cast<double>(n);
    state.at(1) = r;
    state.at(2) = epsilon;

    for (size_t i = 0; i < n; ++i)
        state.at(3 + i) = cells[i];
    return state;
}

double coupled_map_lattice(std::span<const double>, std::span<double> coefs) noexcept
{
    if (coefs.size() < 3)
        return 0.0;

    const auto n = static_cast<size_t>(std::max(coefs.front(), 0.0));
    if (n == 0 || coefs.size() < 3 + 2 * n)
        return 0.0;

    const double r = coefs[1];
    const double epsilon = coefs[2];
    const auto cells = coefs.subspan(3, n);
    const auto mapped = coefs.subspan(3 + n, n);

    logistic_map(cells.data(), mapped.data(), r, n);
    blend_neighbours(cells.data(), mapped.data(), epsilon, n);
    return sum_of(cells) / static_cast<double>(n) - 0.5;
}

std::vector<double> lorenz_state(
    double dt, double sigma, double rho, double beta, double x, double y, double z)
{
    return { sigma, rho, beta, dt, x, y, z };
}

double lorenz_attractor(std::span<const double>, std::span<double> coefs) noexcept
{
    if (coefs.size() < 7)
        return 0.0;

    const double sigma = coefs[0];
    const double rho = coefs[1];
    const double beta = coefs[2];
    const double dt = coefs[3];

    using Vec = std::array<double, 3>;
    const auto derivative = [sigma, rho, beta](const Vec& p) -> Vec {
        return { sigma * (p[1] - p[0]), p[0] * (rho - p[2]) - p[1], p[0] * p[1] - beta * p[2] };
    };
    const auto advanced = [](const Vec& p, const Vec& d, double h) -> Vec {
        return { p[0] + h * d[0], p[1] + h * d[1], p[2] + h * d[2] };
    };

    const Vec start { coefs[4], coefs[5], coefs[6] };
    const Vec k1 = derivative(start);
    const Vec k2 = derivative(advanced(start, k1, 0.5 * dt));
    const Vec k3 = derivative(advanced(start, k2, 0.5 * dt));
    const Vec k4 = derivative(advanced(start, k3, dt));

    for (size_t i = 0; i < 3; ++i)
        coefs[4 + i] = start[i] + dt / 6.0 * (k1[i] + 2.0 * k2[i] + 2.0 * k3[i] + k4[i]);

    return coefs[4];
}

std::vector<double> step_sequence_state(std::span<const double> values, double threshold)
{
    std::vector<double> state(4 + values.size(), 0.0);
    state.at(0) = threshold;
    state.at(1) = 0.0;
    state.at(2) = 1.0;
    state.at(3) = static_cast<double>(values.size());

    for (size_t i = 0; i < values.size(); ++i)
        state.at(4 + i) = values[i];
    return state;
}

double step_sequence(std::span<const double> window, std::span<double> coefs) noexcept
{
    if (coefs.size() < 5)
        return 0.0;

    const auto n = static_cast<size_t>(std::max(coefs[3], 0.0));
    if (n == 0 || coefs.size() < 4 + n)
        return 0.0;

    const bool high = !window.empty() && window.front() > coefs[0];
    const bool latched = coefs[2] > 0.5;
    auto cursor = static_cast<size_t>(std::max(coefs[1], 0.0)) % n;

    if (high && !latched)
        cursor = (cursor + 1) % n;

    coefs[1] = static_cast<double>(cursor);
    coefs[2] = high ? 1.0 : 0.0;
    return coefs[4 + cursor];
}

} // namespace MayaFlux::Kinesis::Discrete
