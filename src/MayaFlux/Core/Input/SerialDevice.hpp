#pragma once

#include "SerialLink.hpp"
#include "SerialReader.hpp"
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

using SerialDeviceStats = SerialReaderStats;

/**
 * @class SerialDevice
 * @brief A serial port with a protocol on it: framing, parsing, commands and replies.
 *
 * The device knows only an ISerialLink and a SerialSpec. Chunks from the link are cut
 * into messages by the spec's framing and each message becomes an InputValue through
 * its parser, delivered to the value callback with the device id and InputType::SERIAL.
 * Any number of further readers can be added, each with a spec of its own, and every
 * reader sees the same bytes and reads them independently. A message over a spec's limit
 * is dropped and counted, never queued.
 *
 * A request sends a command and waits for a message its matcher accepts. One request
 * is outstanding at a time, a matched message goes to the request and not to the value
 * callback, and every other message flows on as usual. When the port leaves OPEN the
 * partial message and the running MIDI status are dropped and a pending request ends
 * with LINK_LOST, so the stream resynchronises when the port returns. Destroying the
 * device drops a pending request without calling it.
 *
 * The device's own callbacks run on the link's thread and must be set before open(). A
 * reader's callback runs there too. send(), request(), add_reader() and remove_reader()
 * may be called from any thread.
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
    using ReaderToken = uint64_t;
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

    /**
     * @brief Read the same bytes a second way.
     * @param spec Framing and parsing of this reader. Its line setup is not used.
     * @param id Id stamped on the values it produces.
     * @param callback Receives each value, on the link's thread.
     * @return A token for remove_reader(), never 0.
     *
     * The reader does not take part in requests, and it starts from a clean stream.
     */
    ReaderToken add_reader(SerialSpec spec, uint32_t id, ValueCallback callback);

    /** @brief Stop a reader. No-op for an unknown token. */
    void remove_reader(ReaderToken token);

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
     * @param done Called exactly once for a request that returns true, on the link's thread,
     *             with the reply when there is one. Never called for a request that returns false.
     * @return false when a request is already pending, the port is not open or the send
     *         failed. A request whose reply or timeout beat a failed send is already complete,
     *         its callback has run, and this returns true.
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
    struct Pending {
        ReplyMatcher matcher;
        ReplyCallback done;
        std::atomic<ISerialLink::TimerToken> timer { 0 };
    };

    using ReaderList = std::vector<std::pair<ReaderToken, std::shared_ptr<SerialReader>>>;

    void handle_chunk(std::span<const uint8_t> chunk);
    void handle_state(EndpointState previous, EndpointState current);
    bool complete_request(Transitive::Protocol::Message frame);
    std::shared_ptr<Pending> take_pending(const std::shared_ptr<Pending>& only = {});
    [[nodiscard]] bool is_pending(const std::shared_ptr<Pending>& request) const;
    void end_request(const std::shared_ptr<Pending>& request, SerialReply how, Transitive::Protocol::Message reply);

    uint32_t m_device_id;
    SerialSpec m_spec;
    std::unique_ptr<ISerialLink> m_link;

    std::unique_ptr<SerialReader> m_reader;
    Memory::SnapshotCell<ReaderList> m_readers;
    std::atomic<ReaderToken> m_next_token { 1 };
    StateCallback m_on_state;

    Memory::SnapshotCell<std::shared_ptr<Pending>> m_pending;
};

} // namespace MayaFlux::Core
