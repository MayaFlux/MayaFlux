#pragma once

namespace MayaFlux::Core {

/**
 * @brief The top-level collection a field belongs to.
 *
 * Usage is zero when the collection declared none. Type is the HID
 * collection type, where 0x01 is Application.
 */
struct HIDCollection {
    uint16_t usage_page {};
    uint16_t usage {};
    uint8_t type {};
};

/**
 * @brief One input field located in a report by descriptor parsing.
 *
 * Bit offsets are measured from the first byte after the report ID when the
 * device uses numbered reports, and from byte zero when it does not.
 */
struct HIDInputField {
    uint8_t report_id {};
    uint16_t usage_page {};
    uint16_t usage {};
    uint32_t bit_offset {};
    uint32_t bit_size {};
    int32_t logical_min {};
    int32_t logical_max {};
    HIDCollection collection;
};

/**
 * @brief Every input field of one device, in descriptor order.
 */
struct HIDReportLayout {
    std::vector<HIDInputField> fields;
    bool uses_report_ids { false };
};

/**
 * @brief Parse a HID report descriptor into its input fields.
 *
 * Free of device state and of HIDAPI, so captured descriptor bytes can be
 * parsed with no hardware present. Constant padding and fields without a
 * usage are skipped, though both still advance the bit position of the
 * fields that follow.
 *
 * @param descriptor Raw report descriptor bytes.
 * @return Parsed layout. No fields means the descriptor declared no usable input.
 */
[[nodiscard]] MAYAFLUX_API HIDReportLayout parse_hid_descriptor(std::span<const uint8_t> descriptor);

/**
 * @brief Read one field from a report as a signed integer.
 *
 * The value is sign extended when the field's logical minimum is negative.
 *
 * @param report Report bytes with the report ID already removed.
 * @param field  Field to read.
 */
[[nodiscard]] MAYAFLUX_API int32_t read_hid_field(
    std::span<const uint8_t> report, const HIDInputField& field) noexcept;

} // namespace MayaFlux::Core
