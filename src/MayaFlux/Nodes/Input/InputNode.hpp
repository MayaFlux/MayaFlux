#pragma once

#include "MayaFlux/Core/GlobalInputConfig.hpp"
#include "MayaFlux/Memory.hpp"
#include "MayaFlux/Nodes/Node.hpp"

namespace MayaFlux::Nodes::Input {

/**
 * @enum InputEventType
 * @brief Types of input events that can trigger callbacks
 */
enum class InputEventType : uint8_t {
    TICK, ///< Every input received
    VALUE_CHANGE, ///< Value changed from previous
    THRESHOLD_RISING, ///< Value crossed threshold upward
    THRESHOLD_FALLING, ///< Value crossed threshold downward
    RANGE_ENTER, ///< Value entered specified range
    RANGE_EXIT, ///< Value exited specified range
    BUTTON_PRESS, ///< Button went from 0.0 to 1.0
    BUTTON_RELEASE, ///< Button went from 1.0 to 0.0
    CONDITIONAL ///< User-provided condition
};

/**
 * @struct InputCallback
 * @brief Callback registration with event type and optional parameters
 */
struct InputCallback {
    NodeHook callback;
    InputEventType event_type;
    std::optional<NodeCondition> condition; // For CONDITIONAL
    std::optional<double> threshold; // For THRESHOLD_*
    std::optional<std::pair<double, double>> range; // For RANGE_*
};

// ─────────────────────────────────────────────────────────────────────────────
// InputContext - Specialized context for input node callbacks
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @class InputContext
 * @brief Context for InputNode callbacks - provides input event access
 *
 * Similar to GeneratorContext or TextureContext, provides node-specific
 * information to callbacks. Contains both the smoothed output value and
 * access to the raw input data.
 */
class MAYAFLUX_API InputContext : public NodeContext {
public:
    InputContext(double value, double raw_value,
        Core::InputType source, uint32_t device_id)
        : NodeContext(value, typeid(InputContext).name())
        , raw_value(raw_value)
        , source_type(source)
        , device_id(device_id)
    {
    }

    double raw_value; ///< Unsmoothed input value
    Core::InputType source_type; ///< Backend that produced this input
    uint32_t device_id; ///< Source device ID
};

/**
 * @brief Smoothing mode for input values
 */
enum class SmoothingMode : uint8_t {
    NONE, ///< No smoothing - immediate value changes (buttons)
    LINEAR, ///< Linear interpolation between values
    EXPONENTIAL, ///< Exponential smoothing / 1-pole lowpass (default)
    SLEW ///< Slew rate limiting
};

/**
 * @brief Configuration for InputNode behavior
 */
struct InputConfig {
    SmoothingMode smoothing { SmoothingMode::EXPONENTIAL };
    double smoothing_factor { 0.1 }; ///< 0-1, higher = faster response
    double slew_rate { 1.0 }; ///< Max change per sample (SLEW mode)
    double default_value { 0.0 }; ///< Initial output value
    size_t history_size { 8 }; ///< Input history buffer size
};

// ─────────────────────────────────────────────────────────────────────────────
// InputNode - Base class for input-driven nodes
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @class InputNode
 * @brief Abstract base class for nodes that receive external input
 *
 * InputNode follows a similar pattern to GpuSync: it bridges async external
 * data (input events) with the synchronous node processing system.
 *
 * Key characteristics:
 * - `on_input()` receives async data from InputSubsystem (like compute_frame produces data)
 * - `process_sample()` returns smoothed values for downstream consumption
 * - `needs_processing()` indicates if new input has arrived (like needs_gpu_update)
 * - Minimal callback overhead (input IS the event)
 *
 * Unlike audio Generators that produce continuous streams, InputNodes:
 * - Are event-driven (data arrives sporadically)
 * - Apply smoothing to convert discrete events to continuous signals
 * - Don't drive timing (they respond to external timing)
 *
 * Derived classes implement:
 * - `extract_value()`: Convert InputValue to scalar
 * - Optionally override `on_input()` for specialized handling
 */
class MAYAFLUX_API InputNode : public Node {
public:
    // explicit InputNode(Config config);

    explicit InputNode(InputConfig config = {});
    ~InputNode() override = default;

    // ─────────────────────────────────────────────────────────────────────
    // Node Interface
    // ─────────────────────────────────────────────────────────────────────

    /**
     * @brief Process a single sample
     * @param input Unused (InputNodes generate from external input)
     * @return Current smoothed input value
     */
    double process_sample(double input = 0.0) override;

    /**
     * @brief Process a batch of samples
     * @param num_samples Number of samples to generate
     * @return Vector of smoothed values
     */
    std::vector<double> process_batch(unsigned int num_samples) override;

    // ─────────────────────────────────────────────────────────────────────
    // Input Reception (called by InputManager)
    // ─────────────────────────────────────────────────────────────────────

    /**
     * @brief Process an input value from a backend
     * @param value The input value
     *
     * Called by InputManager when input arrives. This is the main entry point.
     * - Extracts scalar value via extract_value()
     * - Applies smoothing
     * - Stores result
     * - Fires notify_tick() which triggers user callbacks
     *
     * Thread-safe. Called from InputManager's processing thread.
     */
    virtual void process_input(const Core::InputValue& value);

    /**
     * @brief Check if new input has arrived since last check
     * @return true if process_input() was called
     *
     * Useful for polling-style checks. Calling this clears the flag.
     */
    bool has_new_input();

    /**
     * @brief Clear the new input flag without checking
     */
    void clear_input_flag() { m_has_new_input.store(false); }

    // ─────────────────────────────────────────────────────────────────────
    // State Access
    // ─────────────────────────────────────────────────────────────────────

    /** @brief Get the target value (before smoothing) */
    [[nodiscard]] double get_target_value() const { return m_target_value.load(); }

    /** @brief Get the current smoothed value */
    [[nodiscard]] double get_current_value() const { return m_current_value.load(); }

    /** @brief Get the most recent raw InputValue */
    [[nodiscard]] std::optional<Core::InputValue> get_last_input() const;

    /** @brief Get input history (thread-safe copy) */
    [[nodiscard]] std::vector<Core::InputValue> get_input_history() const;

    // ─────────────────────────────────────────────────────────────────────
    // Configuration
    // ─────────────────────────────────────────────────────────────────────

    void set_smoothing(SmoothingMode mode, double factor = 0.1)
    {
        m_config.smoothing = mode;
        m_config.smoothing_factor = factor;
    }
    void set_slew_rate(double rate) { m_config.slew_rate = rate; }
    [[nodiscard]] const InputConfig& get_config() const { return m_config; }

    // ─────────────────────────────────────────────────────────────────────
    // Context Access (for callbacks)
    // ─────────────────────────────────────────────────────────────────────

    NodeContext& get_last_context() override { return m_context; }

    /**
     * @brief Register callback for any input received
     * @param callback Function to call on every input
     *
     * Alias for on_tick() - fires on every process_input() call
     */
    void on_input(const NodeHook& callback)
    {
        on_tick(callback);
    }

    /**
     * @brief Register callback for value changes
     * @param callback Function to call when value differs from previous
     * @param epsilon Minimum change to trigger (default: 0.0001)
     */
    void on_value_change(const NodeHook& callback, double epsilon = 0.0001);

    /**
     * @brief Register callback for threshold crossing (rising edge)
     * @param threshold Value threshold
     * @param callback Function to call when value crosses threshold upward
     */
    void on_threshold_rising(double threshold, const NodeHook& callback);

    /**
     * @brief Register callback for threshold crossing (falling edge)
     * @param threshold Value threshold
     * @param callback Function to call when value crosses threshold downward
     */
    void on_threshold_falling(double threshold, const NodeHook& callback);

    /**
     * @brief Register callback for entering a value range
     * @param min Range minimum
     * @param max Range maximum
     * @param callback Function to call when value enters range
     */
    void on_range_enter(double min, double max, const NodeHook& callback);

    /**
     * @brief Register callback for exiting a value range
     * @param min Range minimum
     * @param max Range maximum
     * @param callback Function to call when value exits range
     */
    void on_range_exit(double min, double max, const NodeHook& callback);

    /**
     * @brief Register callback for button press (0.0 → 1.0 transition)
     * @param callback Function to call on button press
     *
     * Specialized for button inputs - fires once on press
     */
    void on_button_press(const NodeHook& callback);

    /**
     * @brief Register callback for button release (1.0 → 0.0 transition)
     * @param callback Function to call on button release
     *
     * Specialized for button inputs - fires once on release
     */
    void on_button_release(const NodeHook& callback);

    /**
     * @brief Register callback while value is in range
     * @param min Range minimum
     * @param max Range maximum
     * @param callback Function to call continuously while in range
     */
    void while_in_range(double min, double max, const NodeHook& callback);

protected:
    /**
     * @brief Extract a scalar value from an InputValue
     * @param value The input value to extract from
     * @return Scalar value for smoothing/output
     *
     * Override in derived classes for specific input types.
     * Default handles SCALAR and MIDI CC values.
     */
    virtual double extract_value(const Core::InputValue& value);

    /**
     * @brief Update context after processing
     */
    void update_context(double value) override;

    /**
     * @brief Notify callbacks (minimal for InputNode)
     *
     * InputNodes fire callbacks sparingly - the input event IS the notification.
     * Override if you need per-sample callbacks (rare for input).
     */
    void notify_tick(double value) override;

    InputConfig m_config;
    InputContext m_context;

private:
    std::atomic<double> m_target_value { 0.0 };
    std::atomic<double> m_current_value { 0.0 };
    std::atomic<bool> m_has_new_input { false };
    double m_previous_value {};
    bool m_was_in_range {};

    std::atomic<uint32_t> m_last_device_id { 0 };
    Core::InputType m_last_source_type { Core::InputType::HID };
    std::vector<InputCallback> m_input_callbacks;

    Memory::LockFreeRingBuffer<Core::InputValue, 64> m_input_history;

    [[nodiscard]] double apply_smoothing(double target, double current) const;

    void add_input_callback(
        const NodeHook& callback,
        InputEventType event_type,
        std::optional<double> threshold = {},
        std::optional<std::pair<double, double>> range = {},
        std::optional<NodeCondition> condition = {});
};

} // namespace MayaFlux::Nodes::Input
