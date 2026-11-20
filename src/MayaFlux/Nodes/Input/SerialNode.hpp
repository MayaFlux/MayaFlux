#pragma once

#include "InputNode.hpp"

namespace MayaFlux::Nodes::Input {

/**
 * @struct SerialConfig
 * @brief Which value of each serial message a SerialNode follows
 *
 * A serial message that is a line of numbers arrives as a vector, and the node's scalar
 * output is the value at channel. A message with fewer values, or any other kind of
 * message, leaves the output where it was.
 */
struct SerialConfig : InputConfig {
    size_t channel { 0 }; ///< Which number of the message to follow (0-based)

    double range_min { 0.0 }; ///< Expected input minimum (for normalization)
    double range_max { 1.0 }; ///< Expected input maximum (for normalization)
    bool normalize { false }; ///< Map [range_min, range_max] to [0.0, 1.0]

    /**
     * @brief Follow the first number
     */
    static SerialConfig value() { return {}; }

    /**
     * @brief Follow the number at @p index
     */
    static SerialConfig at(size_t index)
    {
        SerialConfig config;
        config.channel = index;
        return config;
    }

    /**
     * @brief Follow the number at @p index, mapped from [min_val, max_val] to [0, 1]
     */
    static SerialConfig normalized(double min_val, double max_val, size_t index = 0)
    {
        SerialConfig config;
        config.channel = index;
        config.range_min = min_val;
        config.range_max = max_val;
        config.normalize = true;
        return config;
    }
};

/**
 * @class SerialNode
 * @brief InputNode for a serial device that sends numbers, such as an Arduino sensor
 *
 * Follows one number of each message through the node's smoothing. Everything else a
 * serial device sends is available from the last input and the input history, and a MIDI
 * stream from a serial port is read by a MIDINode bound to the same device.
 *
 * @code
 * auto fader = std::make_shared<SerialNode>(SerialConfig::normalized(0.0, 1023.0, 1));
 * register_input_node(fader, bind_serial(device.id));
 *
 * auto node = vega.read_serial(SerialConfig::at(0));
 * @endcode
 */
class MAYAFLUX_API SerialNode : public InputNode {
public:
    explicit SerialNode(SerialConfig config = {});

    void save_state() override { }
    void restore_state() override { }

protected:
    double extract_value(const Core::InputValue& value) override;

private:
    SerialConfig m_serial;
};

} // namespace MayaFlux::Nodes::Input
