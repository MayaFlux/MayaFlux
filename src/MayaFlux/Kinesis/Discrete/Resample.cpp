#include "Resample.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

#include <execution>

namespace MayaFlux::Kinesis::Discrete {

namespace {

    constexpr size_t k_block = 2048;

    double bessel_i0(double x) noexcept
    {
        const double quarter = 0.25 * x * x;
        double term = 1.0;
        double sum = 1.0;
        for (uint32_t k = 1; k < 500; ++k) {
            term *= quarter / (static_cast<double>(k) * static_cast<double>(k));
            sum += term;
            if (term < 1e-17 * sum) {
                break;
            }
        }
        return sum;
    }

    std::vector<double> design_filter(uint32_t up, uint32_t down, const ResampleConfig& config)
    {
        const uint32_t max_rate = std::max(up, down);
        const size_t half = static_cast<size_t>(std::max<uint32_t>(config.zero_crossings, 1)) * max_rate;
        const size_t taps = 2 * half + 1;
        const double cutoff = 1.0 / static_cast<double>(max_rate);
        const double beta_norm = bessel_i0(config.kaiser_beta);

        std::vector<double> h(taps);
        double sum = 0.0;
        for (size_t n = 0; n < taps; ++n) {
            const double x = cutoff * (static_cast<double>(n) - static_cast<double>(half));
            const double sinc = x == 0.0 ? 1.0 : std::sin(std::numbers::pi * x) / (std::numbers::pi * x);
            const double r = 2.0 * static_cast<double>(n) / static_cast<double>(taps - 1) - 1.0;
            const double window = bessel_i0(config.kaiser_beta * std::sqrt(std::max(0.0, 1.0 - r * r))) / beta_norm;
            h[n] = cutoff * sinc * window;
            sum += h[n];
        }

        const double scale = static_cast<double>(up) / sum;
        for (double& tap : h) {
            tap *= scale;
        }
        return h;
    }

    struct Polyphase {
        std::vector<double> reversed;
        std::vector<size_t> offset;
        size_t half {};
    };

    Polyphase build_polyphase(const std::vector<double>& h, uint32_t up)
    {
        Polyphase table;
        table.half = (h.size() - 1) / 2;
        table.offset.assign(static_cast<size_t>(up) + 1, 0);
        table.reversed.reserve(h.size());

        for (uint32_t phase = 0; phase < up; ++phase) {
            std::vector<double> taps;
            for (size_t i = phase; i < h.size(); i += up) {
                taps.push_back(h[i]);
            }
            table.reversed.insert(table.reversed.end(), taps.rbegin(), taps.rend());
            table.offset[size_t { phase } + 1] = table.reversed.size();
        }
        return table;
    }

    double output_sample(std::span<const double> src, size_t origin, const Polyphase& table,
        size_t k, uint32_t up, uint32_t down) noexcept
    {
        const size_t base = k * down + table.half;
        const size_t phase = base % up;
        const size_t newest = base / up;

        const double* coefficients = table.reversed.data() + table.offset[phase];
        const size_t count = table.offset[phase + 1] - table.offset[phase];
        const size_t end = origin + src.size();

        double acc = 0.0;
        if (newest + 1 >= count + origin && newest < end) {
            const double* x = src.data() + (newest + 1 - count - origin);
            for (size_t i = 0; i < count; ++i) {
                acc += coefficients[i] * x[i];
            }
            return acc;
        }

        for (size_t i = 0; i < count; ++i) {
            const size_t index = newest + 1 + i;
            if (index < count) {
                continue;
            }
            const size_t sample = index - count;
            if (sample >= origin && sample < end) {
                acc += coefficients[i] * src[sample - origin];
            }
        }
        return acc;
    }

    void fill_outputs(std::span<const double> src, size_t origin, const Polyphase& table,
        size_t first, std::span<double> out, uint32_t up, uint32_t down)
    {
        const size_t blocks = (out.size() + k_block - 1) / k_block;
        const auto ids = std::views::iota(size_t { 0 }, blocks);
        std::for_each(std::execution::par_unseq, ids.begin(), ids.end(), [&](size_t block) {
            const size_t stop = std::min(out.size(), (block + 1) * k_block);
            for (size_t i = block * k_block; i < stop; ++i) {
                out[i] = output_sample(src, origin, table, first + i, up, down);
            }
        });
    }

}

size_t resampled_length(size_t samples, uint32_t from_rate, uint32_t to_rate) noexcept
{
    if (from_rate == 0 || to_rate == 0) {
        return 0;
    }
    const uint64_t scaled = static_cast<uint64_t>(samples) * to_rate;
    return static_cast<size_t>(scaled / from_rate + (scaled % from_rate != 0 ? 1U : 0U));
}

std::vector<double> resample(
    std::span<const double> src, uint32_t from_rate, uint32_t to_rate, const ResampleConfig& config)
{
    if (src.empty() || from_rate == 0 || to_rate == 0) {
        return {};
    }
    if (from_rate == to_rate) {
        return { src.begin(), src.end() };
    }

    const uint32_t divisor = std::gcd(from_rate, to_rate);
    const uint32_t up = to_rate / divisor;
    const uint32_t down = from_rate / divisor;

    const auto table = build_polyphase(design_filter(up, down, config), up);
    std::vector<double> out(resampled_length(src.size(), from_rate, to_rate));
    fill_outputs(src, 0, table, 0, out, up, down);
    return out;
}

struct Resampler::Impl {
    uint32_t from_rate {};
    uint32_t to_rate {};
    uint32_t up {};
    uint32_t down {};
    bool passthrough {};
    Polyphase table;
    size_t max_taps {};
    std::vector<double> history;
    uint64_t origin {};
    uint64_t seen {};
    uint64_t emitted {};

    [[nodiscard]] uint64_t newest_for(uint64_t k) const noexcept
    {
        return (k * down + table.half) / up;
    }

    [[nodiscard]] uint64_t ready_end() const noexcept
    {
        const uint64_t reach = seen * up;
        return reach > table.half ? (reach - table.half - 1) / down + 1 : 0;
    }

    std::vector<double> produce(uint64_t end)
    {
        std::vector<double> out(end > emitted ? end - emitted : 0);
        fill_outputs(history, origin, table, emitted, out, up, down);
        emitted += out.size();
        return out;
    }

    void trim()
    {
        const uint64_t newest = newest_for(emitted);
        const uint64_t keep_from = newest + 1 > max_taps ? newest + 1 - max_taps : 0;
        if (keep_from <= origin) {
            return;
        }
        const uint64_t drop = std::min<uint64_t>(keep_from - origin, history.size());
        history.erase(history.begin(), history.begin() + static_cast<std::ptrdiff_t>(drop));
        origin += drop;
    }

    void clear() noexcept
    {
        history.clear();
        origin = 0;
        seen = 0;
        emitted = 0;
    }
};

Resampler::Resampler(uint32_t from_rate, uint32_t to_rate, const ResampleConfig& config)
    : m_impl(std::make_unique<Impl>())
{
    if (from_rate == 0 || to_rate == 0) {
        error<std::invalid_argument>(Journal::Component::Kinesis, Journal::Context::Runtime,
            std::source_location::current(), "Resampler rates must be greater than 0");
    }
    auto& impl = *m_impl;
    impl.from_rate = from_rate;
    impl.to_rate = to_rate;
    impl.passthrough = from_rate == to_rate;
    if (impl.passthrough) {
        return;
    }
    const uint32_t divisor = std::gcd(from_rate, to_rate);
    impl.up = to_rate / divisor;
    impl.down = from_rate / divisor;
    const auto filter = design_filter(impl.up, impl.down, config);
    impl.table = build_polyphase(filter, impl.up);
    impl.max_taps = (filter.size() + impl.up - 1) / impl.up;
}

Resampler::~Resampler() = default;
Resampler::Resampler(Resampler&&) noexcept = default;
Resampler& Resampler::operator=(Resampler&&) noexcept = default;

std::vector<double> Resampler::push(std::span<const double> src)
{
    auto& impl = *m_impl;
    if (impl.passthrough) {
        impl.seen += src.size();
        return { src.begin(), src.end() };
    }
    impl.history.insert(impl.history.end(), src.begin(), src.end());
    impl.seen += src.size();

    auto out = impl.produce(impl.ready_end());
    impl.trim();
    return out;
}

std::vector<double> Resampler::flush()
{
    auto& impl = *m_impl;
    std::vector<double> out;
    if (!impl.passthrough) {
        out = impl.produce(resampled_length(impl.seen, impl.from_rate, impl.to_rate));
    }
    impl.clear();
    return out;
}

void Resampler::reset() noexcept
{
    m_impl->clear();
}

size_t Resampler::lookahead() const noexcept
{
    return m_impl->passthrough ? 0 : m_impl->table.half / m_impl->up;
}

size_t Resampler::retained() const noexcept
{
    return m_impl->history.size();
}

} // namespace MayaFlux::Kinesis::Discrete
