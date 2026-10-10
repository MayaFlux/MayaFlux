#pragma once

/**
 * @file Boolean.hpp
 * @brief Pure decision functions of bits and an array of numbers.
 *
 * The boolean counterpart of Kernels.hpp. Every kernel returns a bit and
 * keeps whatever it remembers in the array it is given, so the behaviour is
 * the numbers: the array can be retuned live, saved and restored with the
 * node, and cloned. A signal is a bit when it exceeds the kernel's threshold,
 * and a kernel driven by an edge counts a signal that starts high as a rise.
 *
 * Three shapes, each the function type of the matching Logic constructor:
 * - history kernels: `bool(span<const bool> history, span<const double>)`,
 *   history[0] the newest bit
 * - input kernels: `bool(double input, span<double>)`
 * - time kernels: `bool(double input, double time, span<double>)`
 *
 * `Logic(Kinesis::Discrete::truth_table, 3, truth_table_state(3, rule))` is a
 * complete node. Every kernel returns false for an array too short to hold
 * its layout.
 */

namespace MayaFlux::Kinesis::Discrete {

/**
 * @brief One step of the 32-bit xorshift generator.
 * @param state Nonzero state; zero stays zero
 *
 * Small enough to be held exactly in an element of the array, which is what
 * the random kernels do.
 */
[[nodiscard]] inline uint32_t xorshift32(uint32_t state) noexcept
{
    state ^= state << 13U;
    state ^= state >> 17U;
    state ^= state << 5U;
    return state;
}

// ---------------------------------------------------------------------------
// History kernels
// ---------------------------------------------------------------------------

/**
 * @brief Builds the array truth_table reads.
 * @param arity Number of history bits, at most 6
 * @param rule Output for each input pattern; bit i is the output when the
 *             pattern, with history[0] as the lowest bit, equals i
 *
 * Layout: [arity, outputs...], 2^arity outputs of 0 or 1. For larger arities
 * write the array directly.
 */
[[nodiscard]] MAYAFLUX_API std::vector<double> truth_table_state(size_t arity, uint64_t rule);

/**
 * @brief Any boolean function of the last bits, as a table.
 *
 * coefs: [arity, outputs...]. Reads the arity newest bits, missing ones as
 * zero, and returns the output at that pattern. A table is a universal gate,
 * so this covers AND, XOR, majority and every other combinational rule,
 * and with a rule number over three bits it is an elementary automaton run
 * through time.
 */
[[nodiscard]] MAYAFLUX_API bool truth_table(
    std::span<const bool> history, std::span<const double> coefs) noexcept;

/**
 * @brief True when at least k of the last n bits are set.
 *
 * coefs: [n, k]. Majority, burst detection and k-of-n voting.
 */
[[nodiscard]] MAYAFLUX_API bool count_at_least(
    std::span<const bool> history, std::span<const double> coefs) noexcept;

// ---------------------------------------------------------------------------
// Input kernels
// ---------------------------------------------------------------------------

/**
 * @brief Builds the array hysteresis_gate reads and writes.
 * @param low Level the input must fall below to release
 * @param high Level the input must rise above to engage
 *
 * Layout: [low, high, state].
 */
[[nodiscard]] MAYAFLUX_API std::vector<double> hysteresis_gate_state(double low, double high);

/**
 * @brief Schmitt trigger: engages above high, releases below low, holds between.
 *
 * coefs: [low, high, state].
 */
[[nodiscard]] MAYAFLUX_API bool hysteresis_gate(double input, std::span<double> coefs) noexcept;

/**
 * @brief Builds the array rising_toggle reads and writes.
 * @param threshold Level the input must rise through
 *
 * Layout: [threshold, latched, state].
 */
[[nodiscard]] MAYAFLUX_API std::vector<double> rising_toggle_state(double threshold = 0.5);

/**
 * @brief Flip-flop: the output inverts on every rise of the input.
 *
 * coefs: [threshold, latched, state]. Divide by two with a half-rate square
 * wave out.
 */
[[nodiscard]] MAYAFLUX_API bool rising_toggle(double input, std::span<double> coefs) noexcept;

/**
 * @brief Builds the array edge_divider reads and writes.
 * @param n Pass every n-th rise
 * @param offset Which of each n rises passes, counted from zero
 * @param threshold Level the input must rise through
 *
 * Layout: [threshold, latched, edges, n, offset, pass].
 */
[[nodiscard]] MAYAFLUX_API std::vector<double> edge_divider_state(
    size_t n, size_t offset = 0, double threshold = 0.5);

/**
 * @brief Gate divider: the input passes only during every n-th pulse.
 *
 * coefs: [threshold, latched, edges, n, offset, pass]. The pulse keeps its
 * own length; the pulses in between are dropped.
 */
[[nodiscard]] MAYAFLUX_API bool edge_divider(double input, std::span<double> coefs) noexcept;

/**
 * @brief Builds the array bernoulli_gate reads and writes.
 * @param probability Chance each pulse passes, 0 to 1
 * @param seed Generator state; zero selects a fixed seed
 * @param threshold Level the input must rise through
 *
 * Layout: [threshold, latched, probability, state, pass].
 */
[[nodiscard]] MAYAFLUX_API std::vector<double> bernoulli_gate_state(
    double probability, uint32_t seed = 0, double threshold = 0.5);

/**
 * @brief Probabilistic gate: each pulse passes with the given probability.
 *
 * coefs: [threshold, latched, probability, state, pass]. The draw happens
 * once per rise with xorshift32, so a clone with the same array repeats the
 * same decisions.
 */
[[nodiscard]] MAYAFLUX_API bool bernoulli_gate(double input, std::span<double> coefs) noexcept;

/**
 * @brief Builds the array galois_lfsr reads and writes.
 * @param taps Feedback mask; 0xB400 gives 16 bits and 0x80200003 gives 32 of maximal length
 * @param seed Starting register; zero selects 1
 * @param threshold Level the input must rise through
 *
 * Layout: [threshold, latched, taps, register].
 */
[[nodiscard]] MAYAFLUX_API std::vector<double> galois_lfsr_state(
    uint32_t taps, uint32_t seed = 1, double threshold = 0.5);

/**
 * @brief Linear feedback shift register clocked by the rises of the input.
 *
 * coefs: [threshold, latched, taps, register]. Each rise shifts the register
 * right and, when a bit fell out, folds the taps in. The output is the low
 * bit, a pseudo-random bit stream with a period set by the taps.
 */
[[nodiscard]] MAYAFLUX_API bool galois_lfsr(double input, std::span<double> coefs) noexcept;

/**
 * @brief Builds the array state_machine reads and writes.
 * @param table Per state, four entries: next state and output when the input
 *              bit is 0, then next state and output when it is 1
 * @param threshold Level that makes the input bit 1
 * @param initial Starting state
 *
 * Layout: [threshold, states, state, table...]. The state count is the table
 * length over four.
 */
[[nodiscard]] MAYAFLUX_API std::vector<double> state_machine_state(
    std::span<const double> table, double threshold = 0.5, size_t initial = 0);

/**
 * @brief Mealy machine: the state and the input bit pick the next state and the output.
 *
 * coefs: [threshold, states, state, table...]. Counters, debouncers, pulse
 * sequencers and every other finite controller are tables. An out of range
 * state falls back to state zero.
 */
[[nodiscard]] MAYAFLUX_API bool state_machine(double input, std::span<double> coefs) noexcept;

/**
 * @brief The maximally even placement of pulses among steps, as a 0 or 1 table.
 * @param pulses Number of ones
 * @param steps Length of the table
 * @param rotation Steps to rotate the start by
 *
 * Spreads the pulses as evenly as the grid allows, the Euclidean rhythm:
 * (3, 8) gives 1 0 0 1 0 0 1 0. Feed it to step_sequence_state or
 * step_pattern.
 */
[[nodiscard]] MAYAFLUX_API std::vector<double> maximally_even_sequence(
    size_t pulses, size_t steps, size_t rotation = 0);

/**
 * @brief A table of bits advanced by the rises of the input.
 *
 * The array is step_sequence_state's, read as bits: an entry above 0.5 is a
 * one. Each rise moves to the next entry and wraps.
 */
[[nodiscard]] MAYAFLUX_API bool step_pattern(double input, std::span<double> coefs) noexcept;

// ---------------------------------------------------------------------------
// Time kernels
// ---------------------------------------------------------------------------

/**
 * @brief Builds the array refractory_gate reads and writes.
 * @param period Seconds to stay shut after firing
 * @param threshold Level the input must exceed
 *
 * Layout: [last fire time, period, threshold].
 */
[[nodiscard]] MAYAFLUX_API std::vector<double> refractory_gate_state(double period, double threshold = 0.5);

/**
 * @brief Fires while the input is high, then ignores it for a period.
 *
 * coefs: [last fire time, period, threshold]. A held input fires again
 * once per period.
 */
[[nodiscard]] MAYAFLUX_API bool refractory_gate(
    double input, double time, std::span<double> coefs) noexcept;

/**
 * @brief Builds the array one_shot reads and writes.
 * @param duration Seconds the output stays high after a rise
 * @param threshold Level the input must rise through
 *
 * Layout: [threshold, latched, release time, duration].
 */
[[nodiscard]] MAYAFLUX_API std::vector<double> one_shot_state(double duration, double threshold = 0.5);

/**
 * @brief Monostable: every rise of the input opens the output for a fixed time.
 *
 * coefs: [threshold, latched, release time, duration]. A rise during the pulse
 * restarts it; the input's own length does not matter.
 */
[[nodiscard]] MAYAFLUX_API bool one_shot(
    double input, double time, std::span<double> coefs) noexcept;

} // namespace MayaFlux::Kinesis::Discrete
