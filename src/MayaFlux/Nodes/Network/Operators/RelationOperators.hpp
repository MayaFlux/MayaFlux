#pragma once

#include "NetworkOperator.hpp"

#include "MayaFlux/Kinesis/Stochastic/Weights.hpp"
#include "MayaFlux/Nodes/Network/NodeNetwork.hpp"
#include "MayaFlux/Nodes/Network/RelationSlot.hpp"

namespace MayaFlux::Nodes::Network {

class RelationNetwork;

/**
 * @class Source
 * @brief Anything that can supply a number when evaluated
 *
 * The right-hand side of a mapping wherever an operator parameter should be
 * drivable by whatever is at hand:
 * - A constant returns itself.
 * - A node returns its get_last_output().
 * - A NodeNetwork plus an index returns that network's
 *   get_node_output(index), or 0.0 if it has nothing there. Works the same
 *   whether the network is this RelationNetwork or any other NodeNetwork.
 * - A callable is invoked on whichever thread evaluates the source, so it
 *   follows the same rules as a node hook on the audio thread.
 */
class Source {
public:
    using Function = std::function<double()>;

    Source(double constant)
        : m_value(constant)
    {
    }

    template <std::derived_from<Node> NodeType>
    Source(std::shared_ptr<NodeType> node)
        : m_value(std::shared_ptr<Node>(std::move(node)))
    {
    }

    /**
     * @brief One indexed output of a NodeNetwork
     * @param network Read through get_node_output(index) on every evaluate()
     * @param index Position within network, the same index
     *              NodeNetwork::map_parameter's ONE_TO_ONE mode and
     *              apply_one_to_one() read
     */
    template <std::derived_from<NodeNetwork> NetworkType>
    Source(std::shared_ptr<NetworkType> network, size_t index)
        : m_value(NetworkIndex { std::shared_ptr<NodeNetwork>(std::move(network)), index })
    {
    }

    template <typename Callable>
        requires(!std::same_as<std::remove_cvref_t<Callable>, Source>
            && std::is_invocable_r_v<double, Callable&>)
    Source(Callable&& callable)
        : m_value(Function(std::forward<Callable>(callable)))
    {
    }

    /**
     * @brief Current value
     * @return The constant, the node's last output, the network index's
     *         output, or the callable's result. Zero for an empty node, a
     *         network index the network has no value for, or an empty
     *         callable.
     */
    [[nodiscard]] double evaluate() const
    {
        if (const auto* constant = std::get_if<double>(&m_value)) {
            return *constant;
        }
        if (const auto* node = std::get_if<std::shared_ptr<Node>>(&m_value)) {
            return *node ? (*node)->get_last_output() : 0.0;
        }
        if (const auto* index = std::get_if<NetworkIndex>(&m_value)) {
            return index->network ? index->network->get_node_output(index->index).value_or(0.0) : 0.0;
        }
        const auto& function = std::get<Function>(m_value);
        return function ? function() : 0.0;
    }

private:
    struct NetworkIndex {
        std::shared_ptr<NodeNetwork> network;
        size_t index;
    };

    std::variant<double, std::shared_ptr<Node>, NetworkIndex, Function> m_value;
};

/**
 * @class RelationOperator
 * @brief Operator that acts on a network's slots over one block at a time
 *
 * The network supplies the current slot storage through set_slots(), called
 * once when the operator is added and again at the start of every
 * process_batch(). An operator never caches a slot address past the current
 * cycle, since the network's slot storage can grow (and reallocate) between
 * cycles.
 *
 * A concrete operator has no generic initialize()/reset() lifecycle: it
 * sizes any internal state from its own constructor arguments.
 *
 * Once per block the host calls begin_block(), which applies mapped
 * parameters, then process(frames).
 *
 * An operator has no clock of its own; process_batch() is its only cadence.
 * Anything finer (per-sample or event-driven behavior) is composed from
 * outside: node hooks, tasks, or direct calls to the operator's own public
 * methods. An operator that acts only when called leaves process() empty.
 * Operators communicate through slots: level and activity writes, and
 * rendered blocks.
 *
 * An operator that publishes signals overrides publishes_output(),
 * get_output_count(), and get_output_block().
 */
class MAYAFLUX_API RelationOperator : public NetworkOperator {
public:
    /**
     * @brief Supply the current slot storage
     * @param slots RelationNetwork's slot storage, valid only for the caller's
     *              current cycle
     * @note Called by RelationNetwork::add() and at the start of every
     *       process_batch(). An operator is not usable before the first call.
     */
    void set_slots(std::vector<RelationSlot>& slots) noexcept { m_slots = &slots; }

    /**
     * @brief Apply mapped parameters at the start of a block
     */
    void begin_block()
    {
        for (const auto& binding : m_bindings) {
            set_parameter(binding.parameter, binding.source.evaluate());
        }
    }

    /**
     * @brief Drive an operator parameter from a source
     * @param parameter Name passed to set_parameter
     * @param source Evaluated once per block, at begin_block()
     *
     * A parameter has at most one mapping. Mapping it again replaces the
     * source. Must not run concurrently with begin_block().
     */
    RelationOperator& map(std::string parameter, Source source)
    {
        auto it = std::ranges::find(m_bindings, parameter, &Binding::parameter);
        if (it != m_bindings.end()) {
            it->source = std::move(source);
        } else {
            m_bindings.push_back({ std::move(parameter), std::move(source) });
        }
        return *this;
    }

    /**
     * @brief Remove the mapping of a parameter. No-op when absent.
     * @note Must not run concurrently with begin_block().
     */
    void unmap(std::string_view parameter)
    {
        std::erase_if(m_bindings, [&](const Binding& b) { return b.parameter == parameter; });
    }

    [[nodiscard]] virtual bool publishes_output() const { return false; }
    [[nodiscard]] virtual size_t get_output_count() const { return 0; }
    [[nodiscard]] virtual std::span<const double> get_output_block(size_t /*index*/) const { return {}; }

protected:
    [[nodiscard]] std::span<RelationSlot> slots() const noexcept
    {
        return m_slots ? std::span<RelationSlot>(*m_slots) : std::span<RelationSlot> {};
    }

private:
    struct Binding {
        std::string parameter;
        Source source;
    };

    std::vector<Binding> m_bindings;
    std::vector<RelationSlot>* m_slots { nullptr };
};

/**
 * @class StepOperator
 * @brief General operator whose body is a callable, run once per block
 *
 * Mapped parameters are stored by name and readable from the body through
 * parameter().
 *
 * @code
 * net->add<StepOperator>([](StepOperator& self, std::span<RelationSlot> slots, uint32_t frames) {
 *     for (auto& slot : slots) {
 *         if (slot.level > self.parameter("limit")) {
 *             slot.set_active(false);
 *         }
 *     }
 * });
 * @endcode
 */
class MAYAFLUX_API StepOperator final : public RelationOperator {
public:
    using Body = std::function<void(StepOperator& self, std::span<RelationSlot> slots, uint32_t frames)>;

    explicit StepOperator(Body body)
        : m_body(std::move(body))
    {
    }

    void process(float frames) override;
    void set_parameter(std::string_view param, double value) override;
    [[nodiscard]] std::optional<double> query_state(std::string_view query) const override;
    [[nodiscard]] std::string_view get_type_name() const override { return "Step"; }

    /**
     * @brief Value of a mapped or set parameter
     * @return The value, or 0.0 when the parameter was never set
     */
    [[nodiscard]] double parameter(std::string_view name) const;

private:
    Body m_body;
    std::map<std::string, double, std::less<>> m_parameters;
};

/**
 * @class AdvanceOperator
 * @brief Produces every slot's signal for the block
 *
 * For each slot with a node, advances it for the whole block through
 * RelationNetwork::advance_source(). A slot without a node keeps its stored
 * level for every frame.
 *
 * @note Muting (slot.active == false) zeroes what gets published to the
 *       slot's block, but never its level: level always reflects the slot's
 *       real current value, muted or not. Gating a slot from its own level
 *       (an envelope follower, a threshold trigger) works as expected
 *       because of this.
 *
 * A node's hooks (on_impulse, on_phase_wrap, on_tick, ...) fire during this
 * advance, since set_node() enables them.
 *
 * Activity is read once per slot per block, at the start of process(). Place
 * AdvanceOperator after operators whose slot writes should apply this block,
 * and before DeriveOperator and CombineOperator, which read rendered blocks.
 *
 * Constructed with the owning network, which it needs to reach the protected
 * NodeNetwork::extract_node_samples().
 */
class MAYAFLUX_API AdvanceOperator final : public RelationOperator {
public:
    /**
     * @param host Network whose advance_source() advances slot nodes.
     *             Must outlive this operator.
     */
    explicit AdvanceOperator(RelationNetwork& host)
        : m_host(host)
    {
    }

    void process(float frames) override;

    /** @note AdvanceOperator has no parameters. */
    void set_parameter(std::string_view /*param*/, double /*value*/) override { }
    [[nodiscard]] std::optional<double> query_state(std::string_view /*query*/) const override { return std::nullopt; }
    [[nodiscard]] std::string_view get_type_name() const override { return "Advance"; }

private:
    RelationNetwork& m_host;
    std::vector<double> m_scratch;
    size_t m_scratch_pos {};
};

/**
 * @class DeriveOperator
 * @brief Defines one slot's block as a function of other slots' blocks
 *
 * For each frame, calls the function with the input slots' values in the order
 * given and writes the result to the target slot's block. An input equal to
 * the target reads that slot's previous frame, or its previous block's last
 * value at frame 0. Place after AdvanceOperator.
 */
class MAYAFLUX_API DeriveOperator final : public RelationOperator {
public:
    using Function = std::function<double(std::span<const double>)>;

    /**
     * @param target Index of the slot written
     * @param inputs Indices of the slots read
     * @param function Receives the input values of one frame
     */
    DeriveOperator(size_t target, std::vector<size_t> inputs, Function function);

    void process(float frames) override;

    /** @note DeriveOperator has no parameters. */
    void set_parameter(std::string_view /*param*/, double /*value*/) override { }
    [[nodiscard]] std::optional<double> query_state(std::string_view /*query*/) const override { return std::nullopt; }
    [[nodiscard]] std::string_view get_type_name() const override { return "Derive"; }

private:
    size_t m_target;
    std::vector<size_t> m_inputs;
    std::vector<double> m_values;
    Function m_function;
};

/**
 * @class CombineOperator
 * @brief Turns slot blocks into published outputs
 *
 * Owns a slots-by-outputs table of weights and one block per output. Each
 * output is the weighted sum of slot values in that frame, or, when a combine
 * function is set on it, the function's result over all slot values of the
 * frame. Place after AdvanceOperator and DeriveOperator.
 *
 * Construction sets weight 1.0 from every slot to output 0 and zero
 * elsewhere. Weight cells may be written from any thread.
 *
 * @note weights() keeps the row count given at construction; it does not
 *       grow when the network's slot count does. A slot beyond that row
 *       count is simply left out of every output until weights().assign()
 *       is called to match, which re-seeds every cell.
 */
class MAYAFLUX_API CombineOperator final : public RelationOperator {
public:
    /**
     * @class Output
     * @brief One published signal
     */
    class Output {
    public:
        using Function = std::function<double(std::span<const double>)>;

        /**
         * @brief Replace the weighted sum with a function of all slot values
         * @param function Called once per frame with the frame's slot values
         */
        Output& combine(Function function)
        {
            m_function = std::move(function);
            return *this;
        }

        /** @brief Return to the weighted sum */
        Output& clear_combine()
        {
            m_function = nullptr;
            return *this;
        }

        [[nodiscard]] bool has_combine() const noexcept { return static_cast<bool>(m_function); }
        [[nodiscard]] std::span<const double> block() const noexcept { return m_block; }

    private:
        friend class CombineOperator;

        Function m_function;
        std::vector<double> m_block;
    };

    /**
     * @param slot_count Number of slots of the host network
     * @param output_count Number of outputs published
     */
    explicit CombineOperator(size_t slot_count, size_t output_count = 1);

    void process(float frames) override;

    /** @note CombineOperator has no parameters. */
    void set_parameter(std::string_view /*param*/, double /*value*/) override { }
    [[nodiscard]] std::optional<double> query_state(std::string_view /*query*/) const override { return std::nullopt; }
    [[nodiscard]] std::string_view get_type_name() const override { return "Combine"; }

    [[nodiscard]] bool publishes_output() const override { return true; }
    [[nodiscard]] size_t get_output_count() const override { return m_outputs.size(); }
    [[nodiscard]] std::span<const double> get_output_block(size_t index) const override;

    /**
     * @brief Slot-to-output weights, slot_count rows by output_count columns
     */
    [[nodiscard]] Kinesis::Stochastic::Weights& weights() noexcept { return m_weights; }
    [[nodiscard]] const Kinesis::Stochastic::Weights& weights() const noexcept { return m_weights; }

    [[nodiscard]] Output& output(size_t index) { return m_outputs[index]; }
    [[nodiscard]] const Output& output(size_t index) const { return m_outputs[index]; }

private:
    Kinesis::Stochastic::Weights m_weights;
    std::vector<Output> m_outputs;
    std::vector<double> m_values;
};

} // namespace MayaFlux::Nodes::Network
