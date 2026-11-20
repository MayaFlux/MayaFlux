#pragma once

namespace MayaFlux::Transitive::Protocol {

/**
 * @brief One complete message. Valid only while the sink runs.
 */
using Message = std::span<const uint8_t>;

/**
 * @brief A callable that receives each complete message.
 *
 * It must not push into the framer that is calling it. If it throws, the
 * exception propagates, the rest of that push is discarded and the framer
 * stays valid.
 */
template <typename Sink>
concept MessageSink = std::invocable<Sink&, Message>;

inline constexpr size_t default_max_line = size_t { 1 } << 20;
inline constexpr size_t default_max_frame = size_t { 64 } << 20;

/**
 * @enum FrameFault
 * @brief Why input was refused.
 *
 * Oversized: a line, block or declared length exceeded its limit.
 * Invalid: a zero length where empty messages are not allowed, or a malformed encoding.
 */
enum class FrameFault : uint8_t {
    None,
    Oversized,
    Invalid
};

/**
 * @struct FrameReport
 * @brief What one push did.
 *
 * messages counts messages delivered to the sink. discarded counts lines or
 * blocks dropped for size, which a framer recovers from. fault is the last
 * fault seen. A terminal fault (LengthPrefixFramer only) also reports on every
 * later push until reset().
 */
struct [[nodiscard]] FrameReport {
    size_t messages {};
    size_t discarded {};
    FrameFault fault { FrameFault::None };

    [[nodiscard]] bool ok() const noexcept { return fault == FrameFault::None; }
};

/**
 * @brief View a message as text without copying.
 */
[[nodiscard]] inline std::string_view as_text(Message message) noexcept
{
    return { reinterpret_cast<const char*>(message.data()), message.size() };
}

namespace detail {

    class ClearOnExit {
    public:
        explicit ClearOnExit(std::vector<uint8_t>& buffer) noexcept
            : m_buffer(buffer)
        {
        }

        ClearOnExit(const ClearOnExit&) = delete;
        ClearOnExit& operator=(const ClearOnExit&) = delete;
        ClearOnExit(ClearOnExit&&) = delete;
        ClearOnExit& operator=(ClearOnExit&&) = delete;

        ~ClearOnExit() { m_buffer.clear(); }

    private:
        std::vector<uint8_t>& m_buffer;
    };

    [[nodiscard]] inline uint32_t load_be_u32(const uint8_t* src) noexcept
    {
        return (uint32_t { src[0] } << 24) | (uint32_t { src[1] } << 16)
            | (uint32_t { src[2] } << 8) | uint32_t { src[3] };
    }

    inline void append_be_u32(std::vector<uint8_t>& out, uint32_t value)
    {
        out.push_back(static_cast<uint8_t>(value >> 24));
        out.push_back(static_cast<uint8_t>(value >> 16));
        out.push_back(static_cast<uint8_t>(value >> 8));
        out.push_back(static_cast<uint8_t>(value));
    }

}

/**
 * @class LineFramer
 * @brief Splits a byte stream into lines, whatever the chunking.
 *
 * A message is the bytes before each LF with one trailing CR removed. Empty
 * lines are delivered as empty messages. A line longer than max_line, counted
 * raw before the LF and including any CR, is dropped and the framer resumes
 * after the next LF. A line that sits whole inside one push is delivered as a
 * view into the input with no copy.
 *
 * A payload that contains LF cannot be carried, so multi-line text needs
 * LengthPrefixFramer.
 *
 * Not thread safe: one thread at a time per framer.
 */
class LineFramer {
public:
    explicit LineFramer(size_t max_line = default_max_line) noexcept
        : m_max_line(max_line)
    {
    }

    template <MessageSink Sink>
    FrameReport push(Message bytes, Sink&& sink)
    {
        FrameReport report;

        while (!bytes.empty()) {
            const auto end = std::ranges::find(bytes, static_cast<uint8_t>('\n'));
            const bool complete = end != bytes.end();
            const auto length = static_cast<size_t>(end - bytes.begin());
            const Message segment = bytes.first(length);
            bytes = complete ? bytes.subspan(length + 1) : bytes.subspan(length);

            if (m_discarding) {
                m_discarding = !complete;
                continue;
            }

            if (m_pending.size() + segment.size() > m_max_line) {
                m_pending.clear();
                ++report.discarded;
                report.fault = FrameFault::Oversized;
                m_discarding = !complete;
                continue;
            }

            if (!complete) {
                m_pending.insert(m_pending.end(), segment.begin(), segment.end());
                continue;
            }

            if (m_pending.empty()) {
                deliver(segment, sink, report);
            } else {
                m_pending.insert(m_pending.end(), segment.begin(), segment.end());
                const detail::ClearOnExit guard { m_pending };
                deliver(Message { m_pending }, sink, report);
            }
        }

        return report;
    }

    /** @brief Drop any partial line and any discard in progress. */
    void reset() noexcept
    {
        m_pending.clear();
        m_discarding = false;
    }

    /** @brief Bytes held for a line that has not ended. */
    [[nodiscard]] size_t buffered() const noexcept { return m_pending.size(); }

private:
    template <typename Sink>
    static void deliver(Message line, Sink& sink, FrameReport& report)
    {
        if (!line.empty() && line.back() == '\r') {
            line = line.first(line.size() - 1);
        }
        sink(line);
        ++report.messages;
    }

    std::vector<uint8_t> m_pending;
    size_t m_max_line;
    bool m_discarding { false };
};

/**
 * @class LengthPrefixFramer
 * @brief Messages with a four byte big-endian length before each payload.
 *
 * A zero length is an empty message, unless reject_empty is set and it becomes
 * a fault. A length above max_payload is a fault. Faults are terminal: the
 * stream cannot be resynchronised, so the framer delivers nothing more and
 * reports the fault on every push until reset(). Messages completed earlier in
 * the same push are still delivered. Nothing is reserved from the declared
 * length, so a hostile length costs memory only as bytes actually arrive.
 *
 * A frame that sits whole inside one push is delivered as a view into the input
 * with no copy.
 *
 * Not thread safe: one thread at a time per framer.
 */
class LengthPrefixFramer {
public:
    explicit LengthPrefixFramer(size_t max_payload = default_max_frame, bool reject_empty = false) noexcept
        : m_max_payload(std::min<size_t>(max_payload, std::numeric_limits<uint32_t>::max()))
        , m_reject_empty(reject_empty)
    {
    }

    template <MessageSink Sink>
    FrameReport push(Message bytes, Sink&& sink)
    {
        FrameReport report;
        if (m_fault != FrameFault::None) {
            report.fault = m_fault;
            return report;
        }

        while (!bytes.empty()) {
            if (!m_in_payload) {
                const size_t take = std::min(header_size - m_header_size, bytes.size());
                std::copy_n(bytes.begin(), take, m_header.begin() + static_cast<std::ptrdiff_t>(m_header_size));
                m_header_size += take;
                bytes = bytes.subspan(take);
                if (m_header_size < header_size) {
                    break;
                }
                m_header_size = 0;

                const uint32_t length = detail::load_be_u32(m_header.data());
                if (length == 0 && m_reject_empty) {
                    return fail(report, FrameFault::Invalid);
                }
                if (length > m_max_payload) {
                    return fail(report, FrameFault::Oversized);
                }
                if (length == 0) {
                    sink(Message {});
                    ++report.messages;
                    continue;
                }
                m_expected = length;
                m_in_payload = true;
            }

            if (m_payload.empty() && bytes.size() >= m_expected) {
                const Message frame = bytes.first(m_expected);
                bytes = bytes.subspan(m_expected);
                m_in_payload = false;
                sink(frame);
                ++report.messages;
                continue;
            }

            const size_t take = std::min(m_expected - m_payload.size(), bytes.size());
            m_payload.insert(m_payload.end(), bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(take));
            bytes = bytes.subspan(take);
            if (m_payload.size() < m_expected) {
                break;
            }

            m_in_payload = false;
            const detail::ClearOnExit guard { m_payload };
            sink(Message { m_payload });
            ++report.messages;
        }

        return report;
    }

    /** @brief Drop partial input and clear a terminal fault. */
    void reset() noexcept
    {
        m_header_size = 0;
        m_in_payload = false;
        m_expected = 0;
        m_payload.clear();
        m_fault = FrameFault::None;
    }

    /** @brief True after a terminal fault, until reset(). */
    [[nodiscard]] bool broken() const noexcept { return m_fault != FrameFault::None; }

    /** @brief Partial header bytes plus payload bytes held for a frame that has not completed. */
    [[nodiscard]] size_t buffered() const noexcept { return m_header_size + m_payload.size(); }

private:
    static constexpr size_t header_size = 4;

    FrameReport fail(FrameReport& report, FrameFault fault) noexcept
    {
        m_fault = fault;
        m_header_size = 0;
        m_in_payload = false;
        m_payload.clear();
        report.fault = fault;
        return report;
    }

    std::array<uint8_t, header_size> m_header {};
    std::vector<uint8_t> m_payload;
    size_t m_header_size { 0 };
    size_t m_expected { 0 };
    size_t m_max_payload;
    FrameFault m_fault { FrameFault::None };
    bool m_in_payload { false };
    bool m_reject_empty;
};

/**
 * @brief Append a payload and its LF to out, in the form LineFramer reads back.
 *
 * Refused, with out unchanged, when the payload contains an LF or ends in CR,
 * because LineFramer could not return it exactly. An empty payload is a blank
 * line.
 */
[[nodiscard]] inline bool encode_line(Message payload, std::vector<uint8_t>& out)
{
    if (std::ranges::find(payload, static_cast<uint8_t>('\n')) != payload.end()) {
        return false;
    }
    if (!payload.empty() && payload.back() == '\r') {
        return false;
    }
    out.insert(out.end(), payload.begin(), payload.end());
    out.push_back(static_cast<uint8_t>('\n'));
    return true;
}

/**
 * @brief Append a length prefix and a payload to out, in the form LengthPrefixFramer reads back.
 *
 * Refused, with out unchanged, when the payload is longer than max_payload or
 * 4294967295 bytes, or is empty while reject_empty is set.
 */
[[nodiscard]] inline bool encode_length_prefixed(Message payload, std::vector<uint8_t>& out,
    size_t max_payload = default_max_frame, bool reject_empty = false)
{
    const size_t limit = std::min<size_t>(max_payload, std::numeric_limits<uint32_t>::max());
    if (payload.size() > limit || (payload.empty() && reject_empty)) {
        return false;
    }
    detail::append_be_u32(out, static_cast<uint32_t>(payload.size()));
    out.insert(out.end(), payload.begin(), payload.end());
    return true;
}

/**
 * @struct RawChunkFramer
 * @brief The framer for raw streams: every push is one message, with nothing to reassemble.
 *
 * An empty push delivers nothing.
 */
struct RawChunkFramer {
    template <MessageSink Sink>
    FrameReport push(Message bytes, Sink&& sink)
    {
        FrameReport report;
        if (!bytes.empty()) {
            sink(bytes);
            report.messages = 1;
        }
        return report;
    }

    void reset() noexcept { }
};

/**
 * @class DelimiterFramer
 * @brief Splits a byte stream at a terminator of one or more bytes, whatever the chunking.
 *
 * A message is the bytes before each terminator, such as "\r" for a modem, ">" for an
 * ELM327 or "\r\n". Empty messages are delivered. A message longer than max_message is
 * dropped and the framer resumes after the next terminator. An empty terminator is
 * taken as LF.
 *
 * Not thread safe: one thread at a time per framer.
 */
class DelimiterFramer {
public:
    explicit DelimiterFramer(std::span<const uint8_t> terminator, size_t max_message = default_max_line)
        : m_terminator(terminator.begin(), terminator.end())
        , m_max_message(max_message)
    {
        if (m_terminator.empty()) {
            m_terminator.push_back(static_cast<uint8_t>('\n'));
        }
    }

    explicit DelimiterFramer(std::string_view terminator, size_t max_message = default_max_line)
        : DelimiterFramer(std::span<const uint8_t> { reinterpret_cast<const uint8_t*>(terminator.data()), terminator.size() }, max_message)
    {
    }

    template <MessageSink Sink>
    FrameReport push(Message bytes, Sink&& sink)
    {
        FrameReport report;
        const size_t width = m_terminator.size();

        for (const uint8_t byte : bytes) {
            m_pending.push_back(byte);
            if (m_discarding && m_pending.size() > width) {
                m_pending.erase(m_pending.begin());
            }

            if (!ends_with_terminator()) {
                if (!m_discarding && m_pending.size() >= width && m_pending.size() - width >= m_max_message) {
                    m_discarding = true;
                    ++report.discarded;
                    report.fault = FrameFault::Oversized;
                    if (m_pending.size() > width) {
                        m_pending.erase(m_pending.begin(), m_pending.end() - static_cast<std::ptrdiff_t>(width));
                    }
                }
                continue;
            }

            if (m_discarding) {
                m_pending.clear();
                m_discarding = false;
                continue;
            }

            const Message message = Message { m_pending }.first(m_pending.size() - width);
            const detail::ClearOnExit guard { m_pending };
            sink(message);
            ++report.messages;
        }

        return report;
    }

    /** @brief Drop any partial message and any discard in progress. */
    void reset() noexcept
    {
        m_pending.clear();
        m_discarding = false;
    }

    /** @brief Bytes held for a message that has not ended. */
    [[nodiscard]] size_t buffered() const noexcept { return m_pending.size(); }

private:
    [[nodiscard]] bool ends_with_terminator() const
    {
        return m_pending.size() >= m_terminator.size()
            && std::equal(m_terminator.begin(), m_terminator.end(),
                m_pending.end() - static_cast<std::ptrdiff_t>(m_terminator.size()));
    }

    std::vector<uint8_t> m_terminator;
    std::vector<uint8_t> m_pending;
    size_t m_max_message;
    bool m_discarding { false };
};

/**
 * @brief Append a payload and a terminator to out, in the form DelimiterFramer reads back.
 *
 * Refused, with out unchanged, when the terminator is empty or when the terminator
 * would end the message before the payload does.
 */
[[nodiscard]] inline bool encode_delimited(Message payload, Message terminator, std::vector<uint8_t>& out)
{
    if (terminator.empty()) {
        return false;
    }

    std::vector<uint8_t> joined(payload.begin(), payload.end());
    joined.insert(joined.end(), terminator.begin(), terminator.end());
    const auto hit = std::ranges::search(joined, terminator);
    if (static_cast<size_t>(hit.begin() - joined.begin()) != payload.size()) {
        return false;
    }

    out.insert(out.end(), joined.begin(), joined.end());
    return true;
}

namespace detail {

    /**
     * @brief Decode one COBS frame, without its zero delimiter.
     * @return false when the frame is malformed. out may then hold a partial result.
     */
    [[nodiscard]] inline bool cobs_decode(Message frame, std::vector<uint8_t>& out)
    {
        size_t index = 0;
        while (index < frame.size()) {
            const size_t code = frame[index++];
            if (code == 0) {
                return false;
            }
            const size_t run = code - 1;
            if (index + run > frame.size()) {
                return false;
            }
            for (size_t i = 0; i < run; ++i) {
                if (frame[index + i] == 0) {
                    return false;
                }
            }
            out.insert(out.end(), frame.begin() + static_cast<std::ptrdiff_t>(index),
                frame.begin() + static_cast<std::ptrdiff_t>(index + run));
            index += run;
            if (code != 0xFF && index < frame.size()) {
                out.push_back(0);
            }
        }
        return true;
    }

}

/**
 * @class CobsFramer
 * @brief Messages in Consistent Overhead Byte Stuffing, each ended by a zero byte.
 *
 * The payload may hold any bytes, since COBS removes every zero from the encoded
 * frame. Frames that are only a delimiter are skipped. A frame that does not decode
 * is dropped and counted as discarded with FrameFault::Invalid, and one whose
 * encoded length exceeds max_frame is dropped with FrameFault::Oversized. Either way
 * the framer resumes after the next zero.
 *
 * Not thread safe: one thread at a time per framer.
 */
class CobsFramer {
public:
    explicit CobsFramer(size_t max_frame = default_max_line) noexcept
        : m_max_frame(max_frame)
    {
    }

    template <MessageSink Sink>
    FrameReport push(Message bytes, Sink&& sink)
    {
        FrameReport report;

        for (const uint8_t byte : bytes) {
            if (byte != 0) {
                if (m_discarding) {
                    continue;
                }
                if (m_pending.size() >= m_max_frame) {
                    m_pending.clear();
                    m_discarding = true;
                    ++report.discarded;
                    report.fault = FrameFault::Oversized;
                    continue;
                }
                m_pending.push_back(byte);
                continue;
            }

            if (m_discarding) {
                m_discarding = false;
                continue;
            }
            if (m_pending.empty()) {
                continue;
            }

            m_decoded.clear();
            const bool decoded = detail::cobs_decode(Message { m_pending }, m_decoded);
            m_pending.clear();
            if (!decoded) {
                ++report.discarded;
                report.fault = FrameFault::Invalid;
                continue;
            }
            sink(Message { m_decoded });
            ++report.messages;
        }

        return report;
    }

    /** @brief Drop any partial frame and any discard in progress. */
    void reset() noexcept
    {
        m_pending.clear();
        m_decoded.clear();
        m_discarding = false;
    }

    /** @brief Encoded bytes held for a frame that has not ended. */
    [[nodiscard]] size_t buffered() const noexcept { return m_pending.size(); }

private:
    std::vector<uint8_t> m_pending;
    std::vector<uint8_t> m_decoded;
    size_t m_max_frame;
    bool m_discarding { false };
};

/**
 * @brief Append a payload in COBS, followed by its zero delimiter, to out.
 *
 * Any payload is accepted, an empty one included.
 */
inline void encode_cobs(Message payload, std::vector<uint8_t>& out)
{
    size_t block = 0;
    uint8_t code = 0;
    bool open = false;

    const auto begin = [&] {
        block = out.size();
        out.push_back(0);
        code = 1;
        open = true;
    };
    const auto close = [&] {
        out[block] = code;
        open = false;
    };

    begin();
    for (const uint8_t byte : payload) {
        if (!open) {
            begin();
        }
        if (byte == 0) {
            close();
            begin();
            continue;
        }
        out.push_back(byte);
        if (++code == 0xFF) {
            close();
        }
    }
    if (open) {
        close();
    }
    out.push_back(0);
}

/**
 * @class SlipFramer
 * @brief Messages in SLIP (RFC 1055), each ended by 0xC0.
 *
 * The payload may hold any bytes, with 0xC0 and 0xDB escaped. Empty frames are
 * skipped, so a leading 0xC0 is harmless. An escape followed by anything but the two
 * defined codes, or by 0xC0, drops the frame and counts it as discarded with
 * FrameFault::Invalid. A decoded message longer than max_message is dropped with
 * FrameFault::Oversized. Either way the framer resumes after the next 0xC0.
 *
 * Not thread safe: one thread at a time per framer.
 */
class SlipFramer {
public:
    explicit SlipFramer(size_t max_message = default_max_line) noexcept
        : m_max_message(max_message)
    {
    }

    template <MessageSink Sink>
    FrameReport push(Message bytes, Sink&& sink)
    {
        FrameReport report;

        const auto discard = [&](FrameFault fault) {
            m_pending.clear();
            m_escaped = false;
            m_discarding = true;
            ++report.discarded;
            report.fault = fault;
        };

        for (const uint8_t byte : bytes) {
            if (byte == end) {
                const bool broken = m_escaped && !m_discarding;
                const bool was_discarding = m_discarding;
                const bool has_message = !m_pending.empty();
                m_escaped = false;
                m_discarding = false;
                if (broken) {
                    m_pending.clear();
                    ++report.discarded;
                    report.fault = FrameFault::Invalid;
                } else if (!was_discarding && has_message) {
                    const detail::ClearOnExit guard { m_pending };
                    sink(Message { m_pending });
                    ++report.messages;
                } else {
                    m_pending.clear();
                }
                continue;
            }

            if (m_discarding) {
                continue;
            }

            uint8_t value = byte;
            if (m_escaped) {
                m_escaped = false;
                if (byte == escaped_end) {
                    value = end;
                } else if (byte == escaped_escape) {
                    value = escape;
                } else {
                    discard(FrameFault::Invalid);
                    continue;
                }
            } else if (byte == escape) {
                m_escaped = true;
                continue;
            }

            if (m_pending.size() >= m_max_message) {
                discard(FrameFault::Oversized);
                continue;
            }
            m_pending.push_back(value);
        }

        return report;
    }

    /** @brief Drop any partial frame and any discard in progress. */
    void reset() noexcept
    {
        m_pending.clear();
        m_escaped = false;
        m_discarding = false;
    }

    /** @brief Decoded bytes held for a frame that has not ended. */
    [[nodiscard]] size_t buffered() const noexcept { return m_pending.size(); }

    static constexpr uint8_t end = 0xC0;
    static constexpr uint8_t escape = 0xDB;
    static constexpr uint8_t escaped_end = 0xDC;
    static constexpr uint8_t escaped_escape = 0xDD;

private:
    std::vector<uint8_t> m_pending;
    size_t m_max_message;
    bool m_escaped { false };
    bool m_discarding { false };
};

/**
 * @brief Append a payload in SLIP, followed by 0xC0, to out.
 *
 * Any payload is accepted, an empty one included.
 */
inline void encode_slip(Message payload, std::vector<uint8_t>& out)
{
    for (const uint8_t byte : payload) {
        if (byte == SlipFramer::end) {
            out.push_back(SlipFramer::escape);
            out.push_back(SlipFramer::escaped_end);
        } else if (byte == SlipFramer::escape) {
            out.push_back(SlipFramer::escape);
            out.push_back(SlipFramer::escaped_escape);
        } else {
            out.push_back(byte);
        }
    }
    out.push_back(SlipFramer::end);
}

/**
 * @class IdleGapFramer
 * @brief Messages ended by a quiet period on the line, as in Modbus RTU.
 *
 * The framer reads no clock. push() only gathers bytes, and the caller ends the
 * message by calling flush() once gap() has passed with no further push(), for
 * example from a timer re-armed on every chunk. A message longer than max_message is
 * dropped with FrameFault::Oversized, and the rest of that burst is ignored until
 * flush().
 *
 * Not thread safe: one thread at a time per framer, and flush() must not run
 * concurrently with push().
 */
class IdleGapFramer {
public:
    explicit IdleGapFramer(std::chrono::microseconds gap, size_t max_message = default_max_line) noexcept
        : m_gap(gap)
        , m_max_message(max_message)
    {
    }

    /** @brief How long the line must stay quiet before the caller calls flush(). */
    [[nodiscard]] std::chrono::microseconds gap() const noexcept { return m_gap; }

    template <MessageSink Sink>
    FrameReport push(Message bytes, Sink&&)
    {
        FrameReport report;
        if (m_discarding) {
            return report;
        }

        if (m_pending.size() + bytes.size() > m_max_message) {
            m_pending.clear();
            m_discarding = true;
            ++report.discarded;
            report.fault = FrameFault::Oversized;
            return report;
        }

        m_pending.insert(m_pending.end(), bytes.begin(), bytes.end());
        return report;
    }

    /** @brief End the message in progress, delivering it, or ending a discard. */
    template <MessageSink Sink>
    FrameReport flush(Sink&& sink)
    {
        FrameReport report;
        const bool was_discarding = m_discarding;
        m_discarding = false;
        if (was_discarding || m_pending.empty()) {
            return report;
        }

        const detail::ClearOnExit guard { m_pending };
        sink(Message { m_pending });
        ++report.messages;
        return report;
    }

    /** @brief Drop any partial message and any discard in progress. */
    void reset() noexcept
    {
        m_pending.clear();
        m_discarding = false;
    }

    /** @brief Bytes held for a message that has not ended. */
    [[nodiscard]] size_t buffered() const noexcept { return m_pending.size(); }

private:
    std::vector<uint8_t> m_pending;
    std::chrono::microseconds m_gap;
    size_t m_max_message;
    bool m_discarding { false };
};

} // namespace MayaFlux::Transitive::Protocol
