#pragma once

#include "SerialLink.hpp"
#include "SerialSpec.hpp"

namespace MayaFlux::Core {

/**
 * @struct SerialReaderStats
 * @brief What a reader has seen since it was made.
 *
 * dropped counts messages the framer refused for size or encoding, and unparsed counts
 * messages the parser did not turn into a value.
 */
struct SerialReaderStats {
    uint64_t frames {};
    uint64_t dropped {};
    uint64_t unparsed {};
};

/**
 * @class SerialReader
 * @brief One reading of a byte stream: the spec's framing and parsing, to values.
 *
 * Bytes pushed in are cut into messages by the spec's framing and each message becomes an
 * InputValue through its parser, stamped with the id and InputType::SERIAL and handed to the
 * value callback. A frame filter sees each message first and a message it accepts is not
 * parsed. Several readers over the same bytes read them independently.
 *
 * The link is used only for the timer of an idle gap framing and must outlive the reader.
 * push() and reset() belong to one thread, the link's.
 */
class MAYAFLUX_API SerialReader {
public:
    using ValueCallback = std::function<void(const InputValue&)>;
    using FrameFilter = std::function<bool(Transitive::Protocol::Message message)>;

    /**
     * @param id Id stamped on every value.
     * @param spec The framing and parsing. Its line setup is not used.
     * @param link Supplies the timer of an idle gap framing.
     */
    SerialReader(uint32_t id, SerialSpec spec, ISerialLink& link);
    ~SerialReader();

    SerialReader(const SerialReader&) = delete;
    SerialReader& operator=(const SerialReader&) = delete;
    SerialReader(SerialReader&&) = delete;
    SerialReader& operator=(SerialReader&&) = delete;

    void set_value_callback(ValueCallback callback);
    void set_frame_filter(FrameFilter filter);

    void push(std::span<const uint8_t> chunk);

    /** @brief Drop the partial message, the running MIDI status and a pending gap. */
    void reset();

    [[nodiscard]] SerialReaderStats stats() const;
    [[nodiscard]] uint32_t id() const { return m_id; }
    [[nodiscard]] const SerialSpec& spec() const { return m_spec; }

private:
    using Framer = std::variant<
        Transitive::Protocol::RawChunkFramer,
        Transitive::Protocol::LineFramer,
        Transitive::Protocol::DelimiterFramer,
        Transitive::Protocol::CobsFramer,
        Transitive::Protocol::SlipFramer,
        Transitive::Protocol::IdleGapFramer>;

    struct MidiStream {
        uint8_t status {};
        uint8_t data[2] {};
        uint8_t need {};
        uint8_t have {};
        bool in_sysex {};
    };

    static Framer make_framer(const SerialSpec& spec);

    void handle_frame(Transitive::Protocol::Message frame);
    void handle_gap();
    void parse_midi(std::span<const uint8_t> chunk);
    void deliver(std::optional<InputValue> value);
    void arm_gap();
    void cancel_gap();

    uint32_t m_id;
    SerialSpec m_spec;
    ISerialLink& m_link;
    ValueCallback m_on_value;
    FrameFilter m_filter;
    Framer m_framer;
    MidiStream m_midi;
    std::atomic<ISerialLink::TimerToken> m_gap_timer { 0 };

    std::atomic<uint64_t> m_frames { 0 };
    std::atomic<uint64_t> m_dropped { 0 };
    std::atomic<uint64_t> m_unparsed { 0 };
};

} // namespace MayaFlux::Core
