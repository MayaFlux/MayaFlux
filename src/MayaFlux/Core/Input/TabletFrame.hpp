#pragma once

namespace MayaFlux::Core {

/**
 * @brief Physical tool type.
 *
 * HID report descriptors distinguish pen from eraser and nothing further.
 * Vendor-specific tools such as brush or airbrush are not representable
 * without a device database and are reported as PEN.
 */
enum class TabletToolType : uint8_t {
    PEN,
    ERASER,
    UNKNOWN
};

/**
 * @brief Axes a tool actually reports.
 *
 * Slots whose bit is clear are always zero and must not be interpreted.
 * Position and button slots are always present and have no bit.
 */
enum class TabletAxes : uint16_t {
    NONE = 0,
    PRESSURE = 1U << 0U,
    DISTANCE = 1U << 1U,
    TILT = 1U << 2U,
    ROTATION = 1U << 3U,
    SLIDER = 1U << 4U,
    WHEEL = 1U << 5U
};

[[nodiscard]] constexpr TabletAxes operator|(TabletAxes a, TabletAxes b) noexcept
{
    return static_cast<TabletAxes>(
        static_cast<uint16_t>(a) | static_cast<uint16_t>(b));
}

constexpr TabletAxes& operator|=(TabletAxes& a, TabletAxes b) noexcept
{
    a = a | b;
    return a;
}

[[nodiscard]] constexpr bool has_axis(TabletAxes set, TabletAxes axis) noexcept
{
    return (static_cast<uint16_t>(set) & static_cast<uint16_t>(axis)) != 0U;
}

/**
 * @brief State bits packed into the STATE slot.
 */
enum class TabletState : uint32_t {
    NONE = 0,
    IN_PROXIMITY = 1U << 0U,
    IN_CONTACT = 1U << 1U
};

/**
 * @brief Slot indices into the double vector carried by a tablet InputValue.
 *
 * A tablet source emits one InputValue of Type::VECTOR per hardware sample,
 * sized SLOT_COUNT. Every slot is always present. Absent axes read zero and
 * the tool's TabletAxes mask says which those are.
 *
 * Position, pressure and distance are normalised 0..1. Tilt is -90..90
 * degrees and rotation -180..180 degrees. Slider is -1..1. Buttons and
 * state are integral values stored as doubles and recovered by casting.
 *
 * Slot layout is a public contract. Appending slots is permitted, reordering
 * or reinterpreting existing slots is not.
 */
struct MAYAFLUX_API TabletFrame {
    enum Slot : size_t {
        X = 0, ///< 0.0 to 1.0 across the active area
        Y = 1, ///< 0.0 to 1.0 across the active area
        PRESSURE = 2, ///< 0.0 to 1.0
        DISTANCE = 3, ///< 0.0 to 1.0, hover height
        TILT_X = 4, ///< -90.0 to 90.0 degrees
        TILT_Y = 5, ///< -90.0 to 90.0 degrees
        ROTATION = 6, ///< -180.0 to 180.0 degrees
        SLIDER = 7, ///< -1.0 to 1.0, barrel pressure or finger wheel
        WHEEL = 8, ///< Degrees of wheel rotation this sample
        WHEEL_CLICKS = 9, ///< Integral detents this sample
        BUTTONS = 10, ///< Bit 0 barrel, bit 1 secondary barrel, bit 2 eraser
        STATE = 11 ///< TabletState bitmask
    };

    static constexpr size_t SLOT_COUNT = 12;

    /** @brief Decode the state slot of a frame. */
    [[nodiscard]] static TabletState state_of(std::span<const double> frame) noexcept
    {
        if (frame.size() <= STATE)
            return TabletState::NONE;
        return static_cast<TabletState>(static_cast<uint32_t>(frame[STATE]));
    }

    /** @brief Test a state bit on a frame. */
    [[nodiscard]] static bool in_state(std::span<const double> frame,
        TabletState bit) noexcept
    {
        return (static_cast<uint32_t>(state_of(frame))
                   & static_cast<uint32_t>(bit))
            != 0U;
    }
};

} // namespace MayaFlux::Core
