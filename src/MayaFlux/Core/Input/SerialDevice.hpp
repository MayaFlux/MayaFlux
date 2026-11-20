#pragma once

#include "SerialLink.hpp"
#include "SerialSpec.hpp"

#include "MayaFlux/Transitive/Memory/SnapshotCell.hpp"

namespace MayaFlux::Core {

/**
 * @enum SerialReply
 * @brief How a request ended.
 */
enum class SerialReply : uint8_t {
    REPLIED, ///< A message the matcher accepted arrived
    TIMED_OUT, ///< Nothing matched within the timeout
    LINK_LOST ///< The port left OPEN, or the device was closed, before a reply
};

/**
 * @struct SerialDeviceStats
 * @brief What a device has seen since it was made.
 *
 * dropped counts messages the framer refused for size or encoding, and unparsed counts
 * messages the parser did not turn into a value.
 */
struct SerialDeviceStats {
    uint64_t frames {};
    uint64_t dropped {};
    uint64_t unparsed {};
};

/**
 * @class SerialDevice
 * @brief A serial port with a protocol on it: framing, parsing, commands and replies.
 *
 * The device knows only an ISerialLink and a SerialSpec. Chunks from the link are cut
 * into messages by the spec's framing and each message becomes an InputValue through
 * its parser, delivered to the value callback with the device id and InputType::SERIAL.
 * With a chunk callback set, chunks go there as they arrive and the framing and parser
 * are skipped, for continuous streams. A message over the spec's limit is dropped and
 * counted, never queued.
 *
 * A request sends a command and waits for a message its matcher accepts. One request
 * is outstanding at a time, a matched message goes to the request and not to the value
 * callback, and every other message flows on as usual. When the port leaves OPEN the
 * partial message and the running MIDI status are dropped and a pending request ends
 * with LINK_LOST, so the stream resynchronises when the port returns. Destroying the
 * device drops a pending request without calling it.
 *
 * The three callbacks run on the link's thread and must be set before open(). send()
 * and request() may be called from any thread.
 *
 * @code
 * SerialDevice device(1, SerialSpec::lines().baud(115200).settle_ms(1500),
 *     std::make_unique<NetworkSerialLink>(service));
 * device.set_value_callback([](const InputValue& value) { });
 * device.open("/dev/ttyACM0");
 *
 * device.request(bytes_of("?\n"),
 *     [](Transitive::Protocol::Message m) { return Transitive::Protocol::as_text(m).starts_with("ok"); },
 *     std::chrono::milliseconds(200),
 *     [](SerialReply how, Transitive::Protocol::Message reply) { });
 * @endcode
 */
class MAYAFLUX_API SerialDevice {
public:
    using ValueCallback = std::function<void(const InputValue&)>;
    using ChunkCallback = ISerialLink::ChunkCallback;
    using StateCallback = ISerialLink::StateCallback;
    using ReplyMatcher = std::function<bool(Transitive::Protocol::Message message)>;
    using ReplyCallback = std::function<void(SerialReply how, Transitive::Protocol::Message reply)>;

    /**
     * @param device_id Id stamped on every value.
     * @param spec The line setup, framing and parsing.
     * @param link The transport. Owned.
     */
    SerialDevice(uint32_t device_id, SerialSpec spec, std::unique_ptr<ISerialLink> link);
    ~SerialDevice();

    SerialDevice(const SerialDevice&) = delete;
    SerialDevice& operator=(const SerialDevice&) = delete;
    SerialDevice(SerialDevice&&) = delete;
    SerialDevice& operator=(SerialDevice&&) = delete;

    void set_value_callback(ValueCallback callback);
    void set_chunk_callback(ChunkCallback callback);
    void set_state_callback(StateCallback callback);

    /** @brief Open the port at @p path with the spec's line setup. */
    bool open(const std::string& path);

    /** @brief Close the port. A pending request ends with LINK_LOST. */
    void close();

    [[nodiscard]] EndpointState state() const;

    /** @brief Send bytes as they are. */
    bool send(std::span<const uint8_t> bytes);

    /**
     * @brief Send one message framed the way the spec reads them.
     * @return false when the port is not open or the message cannot be framed, such as an
     *         LF inside a line.
     */
    bool send_framed(std::span<const uint8_t> message);

    /**
     * @brief Send a command and wait for its reply.
     * @param command Sent as it is.
     * @param matcher True for the message that answers the command.
     * @param timeout How long to wait.
     * @param done Called once, on the link's thread, with the reply when there is one.
     * @return false when a request is already pending, the port is not open or the send failed.
     */
    bool request(std::span<const uint8_t> command, ReplyMatcher matcher,
        std::chrono::milliseconds timeout, ReplyCallback done);

    bool set_control_lines(std::optional<bool> dtr, std::optional<bool> rts);
    bool send_break();

    /** @brief Bytes accepted for sending and not yet sent. */
    [[nodiscard]] size_t write_backlog() const;

    [[nodiscard]] SerialDeviceStats stats() const;
    [[nodiscard]] uint32_t device_id() const { return m_device_id; }
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

    struct Pending {
        ReplyMatcher matcher;
        ReplyCallback done;
        std::atomic<ISerialLink::TimerToken> timer { 0 };
    };

    static Framer make_framer(const SerialSpec& spec);

    void handle_chunk(std::span<const uint8_t> chunk);
    void handle_frame(Transitive::Protocol::Message frame);
    void handle_state(EndpointState previous, EndpointState current);
    void handle_gap();
    void parse_midi(std::span<const uint8_t> chunk);
    void deliver(std::optional<InputValue> value);
    void arm_gap();
    void cancel_gap();
    void reset_stream();
    bool complete_request(Transitive::Protocol::Message frame);
    std::shared_ptr<Pending> take_pending(const std::shared_ptr<Pending>& only = {});
    void end_request(const std::shared_ptr<Pending>& request, SerialReply how, Transitive::Protocol::Message reply);

    uint32_t m_device_id;
    SerialSpec m_spec;
    std::unique_ptr<ISerialLink> m_link;

    ValueCallback m_on_value;
    ChunkCallback m_on_chunk;
    StateCallback m_on_state;

    Framer m_framer;
    MidiStream m_midi;
    std::atomic<ISerialLink::TimerToken> m_gap_timer { 0 };

    Memory::SnapshotCell<std::shared_ptr<Pending>> m_pending;

    std::atomic<uint64_t> m_frames { 0 };
    std::atomic<uint64_t> m_dropped { 0 };
    std::atomic<uint64_t> m_unparsed { 0 };
};

} // namespace MayaFlux::Core
