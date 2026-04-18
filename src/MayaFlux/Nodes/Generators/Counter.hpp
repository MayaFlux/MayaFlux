#pragma once

#include "Generator.hpp"

namespace MayaFlux::Nodes::Generator {

/**
 * @class Counter
 * @brief Integer step accumulator with modulo wrap and optional trigger reset
 *
 * Advances an internal integer counter by a fixed step on every process_sample()
 * call, wrapping at a configurable modulo boundary. Output is the counter value
 * normalized to [0, 1] by default, making it directly composable with any
 * downstream node expecting a unit-range signal.
 *
 * When modulo is zero the counter accumulates without bound and the raw integer
 * value is emitted as a double, suitable for use as a direct index.
 *
 * An optional reset trigger node resets the counter to zero on a rising edge
 * (transition from zero to nonzero). The trigger is edge-sensitive: a sustained
 * nonzero value does not repeatedly reset.
 *
 * Tick rate is determined entirely by the processing token the node is routed
 * into (AUDIO_RATE or VISUAL_RATE), consistent with all other generators.
 */
class MAYAFLUX_API Counter : public Generator, public std::enable_shared_from_this<Counter> {
public:
    /**
     * @brief Construct with modulo and step
     * @param modulo Wrap boundary (0 = unbounded)
     * @param step Increment per tick (negative for countdown)
     */
    Counter(uint32_t modulo = 16, int32_t step = 1);

    /**
     * @brief Construct with reset trigger node
     * @param reset_trigger Node whose rising edge resets the counter
     * @param modulo Wrap boundary (0 = unbounded)
     * @param step Increment per tick (negative for countdown)
     */
    Counter(const std::shared_ptr<Node>& reset_trigger, uint32_t modulo = 16, int32_t step = 1);

    ~Counter() override = default;

    double process_sample(double input = 0.0) override;

    std::vector<double> process_batch(unsigned int num_samples) override;

    inline void printGraph() override { }
    inline void printCurrent() override { }

    /**
     * @brief Sets the wrap boundary
     * @param modulo New modulo value (0 = unbounded)
     */
    void set_modulo(uint32_t modulo);

    /**
     * @brief Sets the step increment
     * @param step Signed step value applied each tick
     */
    void set_step(int32_t step);

    /**
     * @brief Connects a reset trigger node
     * @param trigger Node whose rising edge resets the counter to zero
     */
    void set_reset_trigger(const std::shared_ptr<Node>& trigger);

    /**
     * @brief Disconnects the reset trigger node
     */
    void clear_reset_trigger();

    /**
     * @brief Resets counter to zero immediately
     */
    void reset();

    /** @brief Returns current raw counter value */
    [[nodiscard]] uint32_t get_count() const { return m_count; }

    /** @brief Returns current modulo */
    [[nodiscard]] uint32_t get_modulo() const { return m_modulo; }

    /** @brief Returns current step */
    [[nodiscard]] int32_t get_step() const { return m_step; }

    /**
     * @brief Registers a callback fired on every increment
     * @param callback Receives GeneratorContext directly; phase field carries the raw count
     */
    void on_increment(const TypedHook<GeneratorContext>& callback);

    /**
     * @brief Registers a callback fired when the counter wraps to zero
     * @param callback Receives GeneratorContext at the wrap boundary
     */
    void on_wrap(const TypedHook<GeneratorContext>& callback);

    /**
     * @brief Registers a callback fired when the counter reaches a specific raw value
     * @param target Raw counter value to match
     * @param callback Receives GeneratorContext when count == target
     */
    void on_count(uint32_t target, const TypedHook<GeneratorContext>& callback);

    bool remove_hook(const NodeHook& callback) override;
    void remove_all_hooks() override;

    void set_frequency(float) override { }
    [[nodiscard]] float get_frequency() const { return 0.F; }

    void save_state() override;
    void restore_state() override;

protected:
    void notify_tick(double value) override;
    void update_context(double value) override;
    NodeContext& get_last_context() override;

private:
    uint32_t m_count { 0 };
    uint32_t m_modulo { 16 };
    int32_t m_step { 1 };

    std::shared_ptr<Node> m_reset_trigger;
    double m_last_trigger_value { 0.0 };

    uint32_t m_saved_count { 0 };
    double m_saved_last_trigger_value { 0.0 };
    double m_saved_last_output { 0.0 };

    bool m_wrapped { false };

    std::vector<TypedHook<GeneratorContext>> m_increment_callbacks;
    std::vector<TypedHook<GeneratorContext>> m_wrap_callbacks;
    std::vector<std::pair<uint32_t, TypedHook<GeneratorContext>>> m_count_callbacks;

    GeneratorContext m_context { 0.0, 0.F, 1.0, 0.0 };
};

} // namespace MayaFlux::Nodes::Generator
