#pragma once

#include "InputNode.hpp"

namespace MayaFlux::Nodes::Input {

/**
 * @enum HIDParseMode
 * @brief How to interpret HID report bytes
 */
enum class HIDParseMode {
    AXIS, ///< Joystick/gamepad axis with normalization & deadzone
    BUTTON, ///< Digital button (bit mask)
    CUSTOM ///< User-provided parser function
};

/**
 * @struct HIDConfig
 * @brief Unified configuration for all HID input types
 */
struct HIDConfig : InputConfig {
    HIDParseMode mode { HIDParseMode::AXIS };

    // Byte parsing
    size_t byte_offset { 0 };
    size_t byte_size { 1 }; ///< 1 or 2 bytes (for AXIS)
    uint8_t bit_mask { 0xFF }; ///< Bit mask (for BUTTON)

    // Axis normalization
    bool is_signed { false };
    double min_raw { 0.0 };
    double max_raw { 255.0 };
    double deadzone { 0.05 };

    // Button inversion
    bool invert { false };

    // Custom parser
    std::function<double(std::span<const uint8_t>)> custom_parser;

    // Factory methods for clean construction
    static HIDConfig axis(size_t offset, size_t bytes = 1, bool signed_val = false)
    {
        return HIDConfig {
            .mode = HIDParseMode::AXIS,
            .byte_offset = offset,
            .byte_size = bytes,
            .is_signed = signed_val
        };
    }

    static HIDConfig button(size_t offset, uint8_t mask = 0xFF, bool invert_val = false)
    {
        return HIDConfig {
            .mode = HIDParseMode::BUTTON,
            .byte_offset = offset,
            .bit_mask = mask,
            .invert = invert_val
        };
    }

    HIDConfig with_deadzone(double dz) const
    {
        HIDConfig copy = *this;
        copy.deadzone = dz;
        return copy;
    }

    HIDConfig with_range(double min_val, double max_val) const
    {
        HIDConfig copy = *this;
        copy.min_raw = min_val;
        copy.max_raw = max_val;
        return copy;
    }

    template <typename F>
    static HIDConfig custom(F&& parser)
    {
        return HIDConfig {
            .mode = HIDParseMode::CUSTOM,
            .custom_parser = std::forward<F>(parser)
        };
    }
};
/**
 * @class HIDAxisNode
 * @brief InputNode for joystick/gamepad axes with deadzone
 *
 * ### Examples
 * @code
 * // Clean, expressive usage
 * auto stick_x = std::make_shared<HIDNode>(HIDConfig::axis(0, 2, true)
 *     .with_deadzone(0.1)
 *     .with_range(-32768, 32767));
 *
 * auto trigger = std::make_shared<HIDNode>(HIDConfig::axis(4, 1)
 *     .with_range(0, 255));
 *
 * auto button_a = std::make_shared<HIDNode>(HIDConfig::button(6, 0x01));
 *
 * auto custom = std::make_shared<HIDNode>(HIDConfig::custom(
 *     [](auto bytes) { return complex_parsing_logic(bytes); }
 * ));
 * @endcode
 */
class MAYAFLUX_API HIDNode : public InputNode {
public:
    explicit HIDNode(HIDConfig config = {});

    void save_state() override { }

    void restore_state() override { }

protected:
    double extract_value(const Core::InputValue& value) override;

private:
    HIDConfig m_config;

    /**
     * @brief Parse axis value from HID report bytes
     * @param bytes HID report bytes
     * @return Normalized axis value (0.0 to 1.0)
     */
    [[nodiscard]] double parse_axis(std::span<const uint8_t> bytes) const;

    /**
     * @brief Parse button value from HID report bytes
     * @param bytes HID report bytes
     * @return Button state (0.0 or 1.0)
     */
    [[nodiscard]] double parse_button(std::span<const uint8_t> bytes) const;

    [[nodiscard]] double apply_deadzone(double normalized) const;
};

} // namespace MayaFlux::Nodes::Input
