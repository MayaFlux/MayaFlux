#pragma once

#include "NodeNetwork.hpp"
#include "RelationSlot.hpp"

#include "Operators/RelationOperators.hpp"

namespace MayaFlux::Nodes::Network {

/**
 * @class RelationNetwork
 * @brief Host for a dynamically growable set of slots and the operators that act on them
 *
 * The network owns the slots; everything computational lives in operators:
 * advancing slot signals (AdvanceOperator), deriving one slot from others
 * (DeriveOperator), turning slots into published outputs (CombineOperator),
 * and any other slot logic (StepOperator and operators built on the same
 * base). Construction adds no operators and no slots; a network with no
 * publishing operator outputs nothing.
 *
 * Add slots with add_slot(), operators with add<Op>(...) in this order:
 * control operators first, then AdvanceOperator, then DeriveOperator, then
 * CombineOperator.
 *
 * @warning A RelationSlot's address is not stable: add_slot() may reallocate
 *          the whole slot vector. Read a slot fresh through get_slot(index)
 *          or get_node_output(index) rather than holding a pointer to it.
 *
 * add_slot() reserves each new slot's block storage, so a slot added while
 * the network is already running does not allocate on its first block.
 * process_batch() re-supplies every operator the current slot storage,
 * applies mapped parameters, and runs the chain in order. Output 0 of the
 * first publishing operator becomes the network's audio buffer.
 *
 * Structural edits (add_slot(), slot sources, callbacks, adding operators)
 * must happen outside process_batch(). Parameter mapping uses Source and
 * map() on RelationOperator (see RelationOperators.hpp).
 */
class MAYAFLUX_API RelationNetwork : public NodeNetwork {
public:
    RelationNetwork();

    /**
     * @brief Add a slot to the network
     * @param slot Fields not set take RelationSlot's own defaults. index is
     *             overwritten regardless of what is passed; a .node set
     *             directly still gets its hooks enabled, the same as
     *             RelationSlot::set_node().
     * @return Stable index of the new slot. Never changes after insertion,
     *         though the slot's address may, on this or any later add_slot().
     */
    uint32_t add_slot(RelationSlot slot = {});

    /**
     * @brief Add several slots at once
     * @param slots One slot per new entry, in order
     * @return The new slots' indices, in the same order as slots
     */
    std::vector<uint32_t> add_slots(std::initializer_list<RelationSlot> slots);

    [[nodiscard]] RelationSlot& get_slot(uint32_t index);
    [[nodiscard]] const RelationSlot& get_slot(uint32_t index) const;

    /**
     * @brief Find a slot by name
     * @return A reference to the slot, or nullopt if not found
     */
    [[nodiscard]] std::optional<std::reference_wrapper<RelationSlot>> find_slot(std::string_view name);
    [[nodiscard]] std::optional<std::reference_wrapper<const RelationSlot>> find_slot(std::string_view name) const;

    /**
     * @brief Find the index of a slot by name
     * @return Index, or nullopt if not found
     */
    [[nodiscard]] std::optional<uint32_t> find_slot_index(std::string_view name) const;

    [[nodiscard]] size_t slot_count() const noexcept { return m_slots.size(); }

    [[nodiscard]] std::vector<RelationSlot>& slots() noexcept { return m_slots; }
    [[nodiscard]] const std::vector<RelationSlot>& slots() const noexcept { return m_slots; }

    /**
     * @brief Construct an operator in place and append it to the chain
     * @tparam Op Concrete RelationOperator type
     * @param args Forwarded to the Op constructor
     * @return Shared pointer to the operator, retained by the network
     *
     * The operator receives the current slot storage before it is returned,
     * so its public methods are callable immediately. An operator whose
     * publishes_output() is true becomes a source of published outputs.
     */
    template <std::derived_from<RelationOperator> Op, typename... Args>
        requires std::constructible_from<Op, Args...>
    std::shared_ptr<Op> add(Args&&... args)
    {
        auto op = get_operator_chain()->emplace<Op>(std::forward<Args>(args)...);
        op->set_slots(m_slots);
        if (op->publishes_output()) {
            m_publishers.push_back(op);
        }
        return op;
    }

    void process_batch(unsigned int num_samples) override;

    /**
     * @brief Advance a slot's node for a block, through the snapshot sequence
     * @param node Node to advance, ordinarily from a RelationSlot's node field
     * @param buffer Filled with num_samples results
     * @param buffer_pos Reset to 0 by this call
     * @param num_samples Frames to produce
     * @note node must not be processed concurrently on another thread.
     */
    void advance_source(const std::shared_ptr<Nodes::Node>& node, std::vector<double>& buffer,
        size_t& buffer_pos, size_t num_samples)
    {
        extract_node_samples(node, buffer, buffer_pos, num_samples);
    }

    [[nodiscard]] size_t get_node_count() const override { return m_slots.size(); }

    /**
     * @brief Block of the first published output, or nullopt when none is published
     */
    [[nodiscard]] std::optional<std::vector<double>> get_audio_buffer() const override;

    /**
     * @brief Level of one slot after the last processed block
     * @note This is the slot's real underlying value, not what was published
     *       to its block: AdvanceOperator keeps level tracking the node even
     *       while the slot is inactive and its block is silenced. Read
     *       get_node_audio_buffer(index) for what was actually published.
     */
    [[nodiscard]] std::optional<double> get_node_output(size_t index) const override;

    /**
     * @brief Block of one slot from the last process_batch()
     */
    [[nodiscard]] std::optional<std::span<const double>> get_node_audio_buffer(size_t index) const override;

    /**
     * @brief Total published outputs across all publishing operators, in chain order
     */
    [[nodiscard]] size_t get_output_count() const;

    /**
     * @brief Block of one published output from the last process_batch()
     * @param index Position in the concatenation of publishing operators' outputs
     */
    [[nodiscard]] std::optional<std::span<const double>> get_output_block(size_t index) const;

    [[nodiscard]] std::unordered_map<std::string, std::string> get_metadata() const override;

private:
    std::vector<RelationSlot> m_slots;
    std::vector<std::shared_ptr<RelationOperator>> m_publishers;
};

} // namespace MayaFlux::Nodes::Network
