#pragma once

#include "MayaFlux/Core/GlobalInputConfig.hpp"
#include "MayaFlux/Transitive/Protocol/Framing.hpp"

namespace MayaFlux::Core {

/**
 * @brief A user parser: one message to a value, or nothing to drop it.
 *
 * The device id and source type of the result are set by the device. It runs on the
 * link's thread, so it should be quick.
 */
using SerialParser = std::function<std::optional<InputValue>(Transitive::Protocol::Message message)>;

/**
 * @struct SerialSpec
 * @brief What a serial device is: its line setup, framing and parsing, as a plain value.
 *
 * Start from a framing factory and refine with the chained setters, each of which
 * returns a changed copy.
 *
 * @code
 * const auto board = SerialSpec::lines().baud(115200).dtr_on_open(false).settle_ms(1500);
 * const auto din = SerialSpec::midi();
 * const auto frames = SerialSpec::cobs().baud(921600).parse_with(
 *     [](Transitive::Protocol::Message m) -> std::optional<InputValue> { return {}; });
 * @endcode
 */
struct MAYAFLUX_API SerialSpec {
    SerialLineSettings line;
    SerialFraming framing { SerialFraming::LINES };
    std::vector<uint8_t> terminator;
    std::chrono::milliseconds gap { 2 };
    size_t message_limit { 4096 };
    SerialParse parse { SerialParse::NUMBERS };
    SerialParser parser;

    [[nodiscard]] static SerialSpec lines()
    {
        return {};
    }

    /** @brief Messages ended by @p terminator, of one or more bytes. Empty means LF. */
    [[nodiscard]] static SerialSpec delimited(std::string_view terminator)
    {
        SerialSpec spec;
        spec.framing = SerialFraming::DELIMITED;
        spec.terminator.assign(terminator.begin(), terminator.end());
        return spec;
    }

    [[nodiscard]] static SerialSpec cobs()
    {
        SerialSpec spec;
        spec.framing = SerialFraming::COBS;
        spec.parse = SerialParse::BYTES;
        return spec;
    }

    [[nodiscard]] static SerialSpec slip()
    {
        SerialSpec spec;
        spec.framing = SerialFraming::SLIP;
        spec.parse = SerialParse::BYTES;
        return spec;
    }

    /** @brief Messages ended by @p gap of silence. Rounded up to whole milliseconds. */
    [[nodiscard]] static SerialSpec idle_gap(std::chrono::milliseconds gap)
    {
        SerialSpec spec;
        spec.framing = SerialFraming::IDLE_GAP;
        spec.parse = SerialParse::BYTES;
        spec.gap = gap;
        return spec;
    }

    [[nodiscard]] static SerialSpec raw()
    {
        SerialSpec spec;
        spec.framing = SerialFraming::RAW;
        spec.parse = SerialParse::BYTES;
        return spec;
    }

    /** @brief A MIDI byte stream at 31250 baud. */
    [[nodiscard]] static SerialSpec midi()
    {
        SerialSpec spec;
        spec.framing = SerialFraming::RAW;
        spec.parse = SerialParse::MIDI;
        spec.line.baud_rate = 31250;
        return spec;
    }

    [[nodiscard]] SerialSpec baud(uint32_t rate) const
    {
        SerialSpec copy = *this;
        copy.line.baud_rate = rate;
        return copy;
    }

    [[nodiscard]] SerialSpec data_bits(uint8_t bits) const
    {
        SerialSpec copy = *this;
        copy.line.data_bits = bits;
        return copy;
    }

    [[nodiscard]] SerialSpec parity(SerialParity value) const
    {
        SerialSpec copy = *this;
        copy.line.parity = value;
        return copy;
    }

    [[nodiscard]] SerialSpec stop_bits(SerialStopBits value) const
    {
        SerialSpec copy = *this;
        copy.line.stop_bits = value;
        return copy;
    }

    [[nodiscard]] SerialSpec flow_control(SerialFlowControl value) const
    {
        SerialSpec copy = *this;
        copy.line.flow_control = value;
        return copy;
    }

    [[nodiscard]] SerialSpec dtr_on_open(bool level) const
    {
        SerialSpec copy = *this;
        copy.line.dtr_on_open = level;
        return copy;
    }

    [[nodiscard]] SerialSpec rts_on_open(bool level) const
    {
        SerialSpec copy = *this;
        copy.line.rts_on_open = level;
        return copy;
    }

    /** @brief How long the port stays unread after it opens, for boards that restart on open. */
    [[nodiscard]] SerialSpec settle_ms(uint32_t milliseconds) const
    {
        SerialSpec copy = *this;
        copy.line.settle_ms = milliseconds;
        return copy;
    }

    /** @brief The longest message in bytes. A longer one is dropped and counted. */
    [[nodiscard]] SerialSpec max_message(size_t bytes) const
    {
        SerialSpec copy = *this;
        copy.message_limit = bytes;
        return copy;
    }

    [[nodiscard]] SerialSpec numbers() const
    {
        SerialSpec copy = *this;
        copy.parse = SerialParse::NUMBERS;
        return copy;
    }

    [[nodiscard]] SerialSpec bytes() const
    {
        SerialSpec copy = *this;
        copy.parse = SerialParse::BYTES;
        return copy;
    }

    [[nodiscard]] SerialSpec parse_with(SerialParser parser_function) const
    {
        SerialSpec copy = *this;
        copy.parse = SerialParse::CUSTOM;
        copy.parser = std::move(parser_function);
        return copy;
    }

    /** @brief The spec a configured port describes. */
    [[nodiscard]] static SerialSpec from(const SerialLineSettings& line, const SerialProtocolSettings& protocol)
    {
        SerialSpec spec;
        spec.line = line;
        spec.framing = protocol.framing;
        spec.terminator.assign(protocol.terminator.begin(), protocol.terminator.end());
        spec.gap = std::chrono::milliseconds(protocol.gap_ms);
        spec.message_limit = protocol.message_limit;
        spec.parse = protocol.parse;
        return spec;
    }

    /** @brief The terminator of DELIMITED framing, LF when none was given. */
    [[nodiscard]] std::vector<uint8_t> delimiter() const
    {
        return terminator.empty() ? std::vector<uint8_t> { static_cast<uint8_t>('\n') } : terminator;
    }
};

} // namespace MayaFlux::Core
