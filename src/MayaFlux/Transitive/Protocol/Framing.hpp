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
 * Invalid: a zero length where empty messages are not allowed.
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

} // namespace MayaFlux::Transitive::Protocol
