#pragma once

#include "InputNode.hpp"

namespace MayaFlux::Nodes::Input {

/**
 * @struct OSCConfig
 * @brief Configuration for OSC input node argument extraction
 *
 * Determines which argument from an OSC message to extract as the
 * node's scalar output. Address filtering is handled by InputBinding,
 * not by the node. The node only extracts and interprets arguments
 * from messages that already passed binding resolution.
 */
struct OSCConfig : InputConfig {
    size_t argument_index { 0 }; ///< Which argument to extract (0-based)

    double range_min { 0.0 }; ///< Expected input minimum (for normalization)
    double range_max { 1.0 }; ///< Expected input maximum (for normalization)
    bool normalize { false }; ///< Map [range_min, range_max] to [0.0, 1.0]

    std::function<double(const Core::InputValue::OSCArg&)> custom_extractor;

    /**
     * @brief Extract first float argument (most common case)
     */
    static OSCConfig value() { return {}; }

    /**
     * @brief Extract a specific argument by index
     */
    static OSCConfig arg(size_t index)
    {
        OSCConfig cfg;
        cfg.argument_index = index;
        return cfg;
    }

    /**
     * @brief Extract and normalize to [0, 1]
     */
    static OSCConfig normalized(double min_val, double max_val, size_t index = 0)
    {
        OSCConfig cfg;
        cfg.argument_index = index;
        cfg.range_min = min_val;
        cfg.range_max = max_val;
        cfg.normalize = true;
        return cfg;
    }

    /**
     * @brief Use a custom extraction function
     */
    template <typename F>
    static OSCConfig custom(F&& extractor)
    {
        OSCConfig cfg;
        cfg.custom_extractor = std::forward<F>(extractor);
        return cfg;
    }

    OSCConfig& with_range(double min_val, double max_val)
    {
        range_min = min_val;
        range_max = max_val;
        normalize = true;
        return *this;
    }

    OSCConfig& at_index(size_t index)
    {
        argument_index = index;
        return *this;
    }
};

/**
 * @class OSCNode
 * @brief Specialized InputNode for OSC messages
 *
 * Extracts scalar values from OSC message arguments. Address pattern
 * filtering is handled by InputBinding::osc_address_pattern at the
 * InputManager routing level. The node receives only messages that
 * already matched its binding.
 *
 * Supports typed argument callbacks for structured access to the
 * full OSC message beyond the scalar output.
 *
 * Example usage:
 * @code
 * // Simple: first float argument from /sensor/pressure
 * auto pressure = std::make_shared<OSCNode>(OSCConfig::value());
 * register_input_node(pressure, InputBinding::osc("/sensor/pressure"));
 *
 * // Second argument, normalized from [0, 1023] to [0, 1]
 * auto adc = std::make_shared<OSCNode>(
 *     OSCConfig::normalized(0.0, 1023.0, 1));
 * register_input_node(adc, InputBinding::osc("/adc"));
 *
 * // With message callback for multi-argument access
 * auto multi = std::make_shared<OSCNode>();
 * multi->on_message([](const std::string& addr, const auto& args) {
 *     // access all arguments
 * });
 * register_input_node(multi, InputBinding::osc("/multi"));
 *
 * // Via Creator API
 * auto node = vega.read_osc(OSCConfig::value(), InputBinding::osc("/fader/1"));
 * @endcode
 */
class MAYAFLUX_API OSCNode : public InputNode {
public:
    using MessageCallback = std::function<void(
        const std::string& address,
        const std::vector<Core::InputValue::OSCArg>& arguments)>;

    explicit OSCNode(OSCConfig config = {});

    void save_state() override { }
    void restore_state() override { }

    /**
     * @brief Register callback for full OSC message access
     * @param callback Receives address and all typed arguments
     *
     * Fires on every matching message, independent of scalar extraction.
     * Use for multi-argument messages or when you need the address.
     */
    void on_message(MessageCallback callback)
    {
        m_message_callbacks.push_back(std::move(callback));
    }

protected:
    double extract_value(const Core::InputValue& value) override;

    void notify_tick(double value) override;

private:
    OSCConfig m_config;
    std::optional<Core::InputValue::OSCMessage> m_last_osc_message;

    std::vector<MessageCallback> m_message_callbacks;

    void fire_osc_callbacks(const Core::InputValue::OSCMessage& osc);
};

} // namespace MayaFlux::Nodes::Input
