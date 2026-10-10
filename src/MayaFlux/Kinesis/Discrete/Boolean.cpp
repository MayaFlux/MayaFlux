#include "Boolean.hpp"

#include "Kernels.hpp"

namespace MayaFlux::Kinesis::Discrete {

namespace {

    constexpr size_t MAX_TABLE_ARITY = 20;
    constexpr size_t MAX_STATE_ARITY = 6;
    constexpr uint32_t DEFAULT_SEED = 2463534242U;
    constexpr double UINT32_SPAN = 4294967296.0;

    bool rises(bool high, double& latched) noexcept
    {
        const bool edge = high && latched <= 0.5;
        latched = high ? 1.0 : 0.0;
        return edge;
    }

    size_t index_of(double value) noexcept
    {
        return static_cast<size_t>(std::max(value, 0.0));
    }

    uint32_t word_of(double value) noexcept
    {
        return static_cast<uint32_t>(std::clamp(value, 0.0, UINT32_SPAN - 1.0));
    }

}

std::vector<double> truth_table_state(size_t arity, uint64_t rule)
{
    arity = std::min(arity, MAX_STATE_ARITY);

    std::vector<double> state(1 + (size_t { 1 } << arity), 0.0);
    state.front() = static_cast<double>(arity);

    for (size_t i = 1; i < state.size(); ++i)
        state[i] = static_cast<double>((rule >> (i - 1)) & 1U);
    return state;
}

bool truth_table(std::span<const bool> history, std::span<const double> coefs) noexcept
{
    if (coefs.empty())
        return false;

    const size_t arity = index_of(coefs.front());
    if (arity > MAX_TABLE_ARITY || coefs.size() < 1 + (size_t { 1 } << arity))
        return false;

    size_t pattern = 0;
    for (size_t i = 0; i < arity && i < history.size(); ++i) {
        if (history[i])
            pattern |= size_t { 1 } << i;
    }
    return coefs[1 + pattern] > 0.5;
}

bool count_at_least(std::span<const bool> history, std::span<const double> coefs) noexcept
{
    if (coefs.size() < 2)
        return false;

    const size_t n = std::min(index_of(coefs[0]), history.size());
    const auto set = static_cast<size_t>(std::count(history.begin(), history.begin() + static_cast<std::ptrdiff_t>(n), true));
    return set >= index_of(coefs[1]);
}

std::vector<double> hysteresis_gate_state(double low, double high)
{
    return { low, high, 0.0 };
}

bool hysteresis_gate(double input, std::span<double> coefs) noexcept
{
    if (coefs.size() < 3)
        return false;

    if (input > coefs[1])
        coefs[2] = 1.0;
    else if (input < coefs[0])
        coefs[2] = 0.0;
    return coefs[2] > 0.5;
}

std::vector<double> rising_toggle_state(double threshold)
{
    return { threshold, 0.0, 0.0 };
}

bool rising_toggle(double input, std::span<double> coefs) noexcept
{
    if (coefs.size() < 3)
        return false;

    if (rises(input > coefs[0], coefs[1]))
        coefs[2] = coefs[2] > 0.5 ? 0.0 : 1.0;
    return coefs[2] > 0.5;
}

std::vector<double> edge_divider_state(size_t n, size_t offset, double threshold)
{
    return { threshold, 0.0, 0.0, static_cast<double>(n), static_cast<double>(offset), 0.0 };
}

bool edge_divider(double input, std::span<double> coefs) noexcept
{
    if (coefs.size() < 6)
        return false;

    const bool high = input > coefs[0];
    if (rises(high, coefs[1])) {
        const size_t n = std::max<size_t>(index_of(coefs[3]), 1);
        const size_t edge = index_of(coefs[2]);
        coefs[5] = (edge % n) == (index_of(coefs[4]) % n) ? 1.0 : 0.0;
        coefs[2] = static_cast<double>(edge + 1);
    }
    return high && coefs[5] > 0.5;
}

std::vector<double> bernoulli_gate_state(double probability, uint32_t seed, double threshold)
{
    return { threshold, 0.0, probability, static_cast<double>(seed == 0 ? DEFAULT_SEED : seed), 0.0 };
}

bool bernoulli_gate(double input, std::span<double> coefs) noexcept
{
    if (coefs.size() < 5)
        return false;

    const bool high = input > coefs[0];
    if (rises(high, coefs[1])) {
        uint32_t state = word_of(coefs[3]);
        if (state == 0)
            state = DEFAULT_SEED;
        state = xorshift32(state);
        coefs[3] = static_cast<double>(state);
        coefs[4] = static_cast<double>(state) / UINT32_SPAN < coefs[2] ? 1.0 : 0.0;
    }
    return high && coefs[4] > 0.5;
}

std::vector<double> galois_lfsr_state(uint32_t taps, uint32_t seed, double threshold)
{
    return { threshold, 0.0, static_cast<double>(taps), static_cast<double>(seed == 0 ? 1U : seed) };
}

bool galois_lfsr(double input, std::span<double> coefs) noexcept
{
    if (coefs.size() < 4)
        return false;

    uint32_t reg = word_of(coefs[3]);
    if (rises(input > coefs[0], coefs[1])) {
        if (reg == 0)
            reg = 1;
        const bool fell_out = (reg & 1U) != 0U;
        reg >>= 1U;
        if (fell_out)
            reg ^= word_of(coefs[2]);
        coefs[3] = static_cast<double>(reg);
    }
    return (reg & 1U) != 0U;
}

std::vector<double> state_machine_state(std::span<const double> table, double threshold, size_t initial)
{
    const size_t states = table.size() / 4;

    std::vector<double> state(3 + 4 * states, 0.0);
    state[0] = threshold;
    state[1] = static_cast<double>(states);
    state[2] = static_cast<double>(initial);

    std::copy_n(table.begin(), static_cast<std::ptrdiff_t>(4 * states), state.begin() + 3);
    return state;
}

bool state_machine(double input, std::span<double> coefs) noexcept
{
    if (coefs.size() < 3)
        return false;

    const size_t states = index_of(coefs[1]);
    if (states == 0 || coefs.size() < 3 + 4 * states)
        return false;

    size_t current = index_of(coefs[2]);
    if (current >= states)
        current = 0;

    const size_t row = 3 + 4 * current + (input > coefs[0] ? 2 : 0);
    const size_t next = index_of(coefs[row]);
    coefs[2] = static_cast<double>(next < states ? next : 0);
    return coefs[row + 1] > 0.5;
}

std::vector<double> maximally_even_sequence(size_t pulses, size_t steps, size_t rotation)
{
    std::vector<double> out(steps, 0.0);
    if (steps == 0)
        return out;

    pulses = std::min(pulses, steps);
    for (size_t i = 0; i < steps; ++i) {
        const size_t position = (i + rotation) % steps;
        out[i] = (position * pulses) % steps < pulses ? 1.0 : 0.0;
    }
    return out;
}

bool step_pattern(double input, std::span<double> coefs) noexcept
{
    return step_sequence(std::span<const double>(&input, 1), coefs) > 0.5;
}

std::vector<double> refractory_gate_state(double period, double threshold)
{
    return { std::numeric_limits<double>::lowest(), period, threshold };
}

bool refractory_gate(double input, double time, std::span<double> coefs) noexcept
{
    if (coefs.size() < 3)
        return false;

    if (input > coefs[2] && time - coefs[0] >= coefs[1]) {
        coefs[0] = time;
        return true;
    }
    return false;
}

std::vector<double> one_shot_state(double duration, double threshold)
{
    return { threshold, 0.0, 0.0, duration };
}

bool one_shot(double input, double time, std::span<double> coefs) noexcept
{
    if (coefs.size() < 4)
        return false;

    if (rises(input > coefs[0], coefs[1]))
        coefs[2] = time + coefs[3];
    return time < coefs[2];
}

std::vector<double> set_reset_latch_state(double threshold)
{
    return { threshold, 0.0 };
}

bool set_reset_latch(const std::vector<double>& inputs, std::span<double> coefs) noexcept
{
    if (inputs.size() < 2 || coefs.size() < 2)
        return false;

    if (inputs[1] > coefs[0])
        coefs[1] = 0.0;
    else if (inputs[0] > coefs[0])
        coefs[1] = 1.0;
    return coefs[1] > 0.5;
}

std::vector<double> comparator_state(double margin)
{
    return { margin, 0.0 };
}

bool comparator(const std::vector<double>& inputs, std::span<double> coefs) noexcept
{
    if (inputs.size() < 2 || coefs.size() < 2)
        return false;

    const double difference = inputs[0] - inputs[1];
    if (difference > coefs[0])
        coefs[1] = 1.0;
    else if (difference < -coefs[0])
        coefs[1] = 0.0;
    return coefs[1] > 0.5;
}

bool window_comparator(const std::vector<double>& inputs, std::span<const double>) noexcept
{
    if (inputs.size() < 3)
        return false;
    return inputs[0] > inputs[1] && inputs[0] < inputs[2];
}

std::vector<double> signals_at_least_state(size_t k, double threshold)
{
    return { threshold, static_cast<double>(k) };
}

bool signals_at_least(const std::vector<double>& inputs, std::span<const double> coefs) noexcept
{
    if (coefs.size() < 2)
        return false;

    const double threshold = coefs[0];
    const auto high = static_cast<size_t>(std::count_if(inputs.begin(), inputs.end(),
        [threshold](double v) { return v > threshold; }));
    return high >= index_of(coefs[1]);
}

std::vector<double> input_truth_table_state(size_t arity, uint64_t rule, double threshold)
{
    arity = std::min(arity, MAX_STATE_ARITY);

    std::vector<double> state(2 + (size_t { 1 } << arity), 0.0);
    state[0] = threshold;
    state[1] = static_cast<double>(arity);

    for (size_t i = 2; i < state.size(); ++i)
        state[i] = static_cast<double>((rule >> (i - 2)) & 1U);
    return state;
}

bool input_truth_table(const std::vector<double>& inputs, std::span<const double> coefs) noexcept
{
    if (coefs.size() < 2)
        return false;

    const size_t arity = index_of(coefs[1]);
    if (arity > MAX_TABLE_ARITY || coefs.size() < 2 + (size_t { 1 } << arity))
        return false;

    size_t pattern = 0;
    for (size_t i = 0; i < arity && i < inputs.size(); ++i) {
        if (inputs[i] > coefs[0])
            pattern |= size_t { 1 } << i;
    }
    return coefs[2 + pattern] > 0.5;
}

std::vector<double> clocked_register_state(size_t length, size_t tap, double threshold)
{
    std::vector<double> state(4 + length, 0.0);
    state[0] = threshold;
    state[2] = static_cast<double>(length);
    state[3] = static_cast<double>(tap);
    return state;
}

bool clocked_register(const std::vector<double>& inputs, std::span<double> coefs) noexcept
{
    if (inputs.size() < 2 || coefs.size() < 4)
        return false;

    const size_t length = index_of(coefs[2]);
    if (length == 0 || coefs.size() < 4 + length)
        return false;

    const auto stages = coefs.subspan(4, length);
    if (rises(inputs[0] > coefs[0], coefs[1])) {
        for (size_t i = length - 1; i > 0; --i)
            stages[i] = stages[i - 1];
        stages[0] = inputs[1] > coefs[0] ? 1.0 : 0.0;
    }
    return stages[std::min(index_of(coefs[3]), length - 1)] > 0.5;
}

} // namespace MayaFlux::Kinesis::Discrete
