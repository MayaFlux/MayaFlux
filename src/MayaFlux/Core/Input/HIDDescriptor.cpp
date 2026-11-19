#include "HIDDescriptor.hpp"

#include "MayaFlux/Transitive/Protocol/BitField.hpp"

namespace MayaFlux::Core {

namespace {

    uint32_t item_unsigned(const uint8_t* data, size_t bytes) noexcept
    {
        return static_cast<uint32_t>(
            Transitive::Protocol::load_bits_le(std::span(data, bytes), 0, bytes * 8U));
    }

    int32_t item_signed(const uint8_t* data, size_t bytes) noexcept
    {
        return static_cast<int32_t>(
            Transitive::Protocol::sign_extend(item_unsigned(data, bytes), bytes * 8U));
    }

    /**
     * @brief Global item state, saved and restored by Push and Pop.
     */
    struct GlobalState {
        uint16_t usage_page {};
        int32_t logical_min {};
        int32_t logical_max {};
        uint32_t report_size {};
        uint32_t report_count {};
        uint8_t report_id {};
    };

    /**
     * @brief A local usage carrying its own page.
     *
     * Extended 32-bit usages embed a page that applies only to that usage,
     * so the page cannot be folded into the global state.
     */
    struct LocalUsage {
        uint16_t page {};
        uint16_t usage {};
    };

} // namespace

HIDReportLayout parse_hid_descriptor(std::span<const uint8_t> descriptor)
{
    HIDReportLayout layout;

    GlobalState global;
    std::vector<GlobalState> global_stack;

    std::vector<LocalUsage> local_usages;
    uint32_t usage_min = 0;
    uint32_t usage_max = 0;
    bool has_usage_range = false;

    std::unordered_map<uint8_t, uint32_t> bit_cursor;
    size_t depth = 0;
    HIDCollection top_level;

    const auto clear_locals = [&]() {
        local_usages.clear();
        usage_min = 0;
        usage_max = 0;
        has_usage_range = false;
    };

    size_t i = 0;
    while (i < descriptor.size()) {
        const uint8_t prefix = descriptor[i];

        if (prefix == 0xFE) {
            if (i + 1 >= descriptor.size())
                break;
            i += 2U + descriptor[i + 1];
            continue;
        }

        size_t size = prefix & 0x03U;
        if (size == 3)
            size = 4;
        const uint8_t type = (prefix >> 2U) & 0x03U;
        const uint8_t tag = (prefix >> 4U) & 0x0FU;

        ++i;
        if (i + size > descriptor.size())
            break;

        const uint8_t* payload = descriptor.data() + i;
        i += size;

        if (type == 1) {
            switch (tag) {
            case 0x0:
                global.usage_page = static_cast<uint16_t>(item_unsigned(payload, size));
                break;

            case 0x1:
                global.logical_min = item_signed(payload, size);
                break;

            case 0x2: {
                const int32_t as_signed = item_signed(payload, size);
                global.logical_max = (global.logical_min >= 0 && as_signed < 0)
                    ? static_cast<int32_t>(item_unsigned(payload, size))
                    : as_signed;
                break;
            }

            case 0x7:
                global.report_size = item_unsigned(payload, size);
                break;

            case 0x8:
                global.report_id = static_cast<uint8_t>(item_unsigned(payload, size));
                layout.uses_report_ids = true;
                break;

            case 0x9:
                global.report_count = item_unsigned(payload, size);
                break;

            case 0xA:
                global_stack.push_back(global);
                break;

            case 0xB:
                if (!global_stack.empty()) {
                    global = global_stack.back();
                    global_stack.pop_back();
                }
                break;

            default:
                break;
            }
            continue;
        }

        if (type == 2) {
            switch (tag) {
            case 0x0:
                if (size == 4) {
                    const uint32_t full = item_unsigned(payload, size);
                    const auto page = static_cast<uint16_t>(full >> 16U);
                    local_usages.push_back({ .page = page, .usage = static_cast<uint16_t>(full & 0xFFFFU) });
                } else {
                    local_usages.push_back({ .page = global.usage_page,
                        .usage = static_cast<uint16_t>(item_unsigned(payload, size)) });
                }
                break;

            case 0x1:
                usage_min = item_unsigned(payload, size);
                has_usage_range = true;
                break;

            case 0x2:
                usage_max = item_unsigned(payload, size);
                has_usage_range = true;
                break;

            default:
                break;
            }
            continue;
        }

        if (type != 0) {
            continue;
        }

        if (tag == 0xA) {
            if (depth == 0) {
                top_level = HIDCollection { .type = static_cast<uint8_t>(item_unsigned(payload, size)) };
                if (!local_usages.empty()) {
                    top_level.usage_page = local_usages.back().page;
                    top_level.usage = local_usages.back().usage;
                }
            }
            ++depth;
            clear_locals();
            continue;
        }

        if (tag == 0xC) {
            if (depth > 0 && --depth == 0)
                top_level = HIDCollection {};
            clear_locals();
            continue;
        }

        if (tag != 0x8) {
            clear_locals();
            continue;
        }

        const uint32_t flags = item_unsigned(payload, size);
        const bool is_constant = (flags & 0x01U) != 0U;

        if (global.report_size == 0 || global.report_count == 0) {
            clear_locals();
            continue;
        }

        uint32_t& cursor = bit_cursor[global.report_id];

        for (uint32_t index = 0; index < global.report_count; ++index) {
            LocalUsage current { .page = global.usage_page, .usage = 0 };

            if (!is_constant) {
                if (index < local_usages.size()) {
                    current = local_usages[index];
                } else if (!local_usages.empty()) {
                    current = local_usages.back();
                } else if (has_usage_range) {
                    current.usage = static_cast<uint16_t>(
                        std::min(usage_min + index, usage_max));
                }
            }

            if (!is_constant && current.usage != 0) {
                layout.fields.push_back(HIDInputField {
                    .report_id = global.report_id,
                    .usage_page = current.page,
                    .usage = current.usage,
                    .bit_offset = cursor,
                    .bit_size = global.report_size,
                    .logical_min = global.logical_min,
                    .logical_max = global.logical_max,
                    .collection = top_level,
                    .is_array = (flags & 0x02U) == 0U,
                    .is_relative = (flags & 0x04U) != 0U,
                });
            }

            cursor += global.report_size;
        }

        clear_locals();
    }

    return layout;
}

size_t hid_input_report_size(const HIDReportLayout& layout) noexcept
{
    uint32_t bits = 0;
    for (const auto& field : layout.fields) {
        bits = std::max(bits, field.bit_offset + field.bit_size);
    }
    return (static_cast<size_t>(bits) + 7U) / 8U + (layout.uses_report_ids ? 1U : 0U);
}

int32_t read_hid_field(std::span<const uint8_t> report, const HIDInputField& field) noexcept
{
    using namespace Transitive::Protocol;

    return static_cast<int32_t>(field.logical_min < 0
            ? load_signed_bits_le(report, field.bit_offset, field.bit_size)
            : static_cast<int64_t>(load_bits_le(report, field.bit_offset, field.bit_size)));
}

} // namespace MayaFlux::Core
