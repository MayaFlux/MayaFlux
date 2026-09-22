#pragma once

#include "RelationOperators.hpp"

namespace MayaFlux::Nodes::Network {

/**
 * @class WalkOperator
 * @brief Moves a single active slot along the rows of a weight table
 *
 * Exactly one slot is meant to be active at a time. advance() reads the row
 * of the current slot, picks a successor, deactivates the current slot, and
 * activates the successor, so the slots' leave and enter callbacks fire in
 * that order. jump() moves directly. Nothing else changes the state.
 *
 * WalkOperator can be stepped two ways, and both may be used on the same instance:
 *
 * 1. External: call advance() from a node hook, a Vruta or Kriya task, another
 *    operator, or user code. This is sample- or event-accurate, but requires
 *    something outside the network to call it.
 *
 * 2. Self-contained: construct with every_n_blocks > 0. process(), which the
 *    network's operator chain already calls once per process_batch() with no
 *    external wiring, then calls advance() itself every N blocks. This is
 *    coarser (block-resolution, not sample-accurate) but requires nothing
 *    outside the network to make the chain progress at all. every_n_blocks is
 *    0 by default, meaning process() does nothing and only external calls to
 *    advance() move the walk.
 *
 * The table is a shared pointer so other operators and writers can read or
 * edit the same weights, and any cell may be rewritten while the walk runs.
 * The draw itself uses an owned Kinesis::Stochastic::Stochastic generator
 * (UNIFORM by default).
 *
 * The first call to process(), advance(), or jump() establishes the single-
 * active-slot state: it activates slot 0 (or whichever slot m_current
 * already names) and deactivates every other slot that exists at that
 * moment, before doing anything else. A slot added by add_slot() afterward
 * starts at RelationSlot's own default (active) like any other new slot;
 * WalkOperator does not retroactively enforce its invariant on it. jump() it
 * explicitly to bring it into the walk.
 *
 * Parameters:
 * - "current": jumps to that slot index when it differs from the current one,
 *   so a mapped Source can steer the walk.
 *
 * The table must be square with a dimension equal to the slot count of the
 * network. advance() and jump() mutate the walk and must not be called
 * concurrently from more than one thread. Reads are atomic.
 */
class MAYAFLUX_API WalkOperator final : public RelationOperator {
public:
    /**
     * @brief Chooses a successor from a row of weights
     * @param row Outgoing weights of the current slot. Values need not sum to one.
     * @return Index of the successor. An index outside the row leaves the walk in place.
     */
    using Pick = std::function<size_t(std::span<const double> row)>;

    /**
     * @param table Transition weights, one row per slot
     * @param pick Selection rule. When empty, Kinesis::Stochastic::Weights::weighted_pick
     *             draws from the row using this operator's own Stochastic generator;
     *             a row that sums to zero leaves the walk in place.
     * @param every_n_blocks When nonzero, process() calls advance() every this
     *                       many process_batch() calls, with no external driver
     *                       needed. When zero (the default), process() does
     *                       nothing and only explicit advance()/jump() calls,
     *                       or a mapped "current" Source, move the walk.
     */
    explicit WalkOperator(std::shared_ptr<Kinesis::Stochastic::Weights> table, Pick pick = {}, uint32_t every_n_blocks = 0);

    /**
     * @brief Step once
     * @return The active slot after the step
     */
    size_t advance();

    /**
     * @brief Make a slot the active one
     * @param index Slot index. Out of range is ignored.
     */
    void jump(size_t index);

    [[nodiscard]] size_t current() const noexcept { return m_current.load(std::memory_order_relaxed); }
    [[nodiscard]] const std::shared_ptr<Kinesis::Stochastic::Weights>& table() const noexcept { return m_table; }

    /**
     * @brief The generator behind the default weighted draw
     * @note Not used when a custom Pick was supplied at construction.
     */
    [[nodiscard]] Kinesis::Stochastic::Stochastic& generator() noexcept { return m_generator; }

    /**
     * @brief Self-contained step
     *
     * Does nothing when every_n_blocks is 0 (the default). Otherwise counts
     * blocks and calls advance() every every_n_blocks calls, so the chain
     * progresses on its own with no code outside the network. Runs regardless
     * of whether advance() is also being called externally; the two do not
     * conflict, they simply both move the same walk.
     */
    void process(float frames) override;

    /**
     * @brief Change or disable the self-contained step rate
     * @param every_n_blocks 0 disables self-stepping; a positive value steps
     *                        that often, resetting the internal block counter
     * @note The block counter itself is not atomic. Calling this concurrently
     *       with process() (i.e. while the owning network is being processed
     *       on another thread, such as the audio thread) is a data race. Call
     *       it from the same thread that drives process_batch(), or before
     *       the network is registered for processing.
     */
    void set_every_n_blocks(uint32_t every_n_blocks) noexcept;

    void set_parameter(std::string_view param, double value) override;

    /**
     * @brief Query walk state
     * @param query "current" for the active slot index
     */
    [[nodiscard]] std::optional<double> query_state(std::string_view query) const override;

    [[nodiscard]] std::string_view get_type_name() const override { return "Walk"; }

private:
    /**
     * @brief Activate slot m_current and deactivate every other slot that
     *        currently exists. No-op after the first call, and no-op if no
     *        slots exist yet (tried again on the next call in that case).
     */
    void ensure_established();

    std::shared_ptr<Kinesis::Stochastic::Weights> m_table;
    Pick m_pick;
    Kinesis::Stochastic::Stochastic m_generator { Kinesis::Stochastic::Algorithm::UNIFORM };
    std::atomic<size_t> m_current { 0 };
    std::atomic<uint32_t> m_every_n_blocks { 0 };
    uint32_t m_blocks_until { 0 };
    bool m_established { false };
};

} // namespace MayaFlux::Nodes::Network
