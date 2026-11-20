#pragma once

#include "MayaFlux/Core/GlobalNetworkConfig.hpp"

namespace MayaFlux::Core {

/**
 * @class ISerialLink
 * @brief What a serial device needs from the transport under it.
 *
 * One link is one port. Both callbacks run on the link's own thread and never
 * concurrently with each other or with a timer, and both must be set before open().
 * write() returns whether the bytes were accepted for sending, not delivered.
 */
class MAYAFLUX_API ISerialLink {
public:
    using ChunkCallback = std::function<void(std::span<const uint8_t> bytes)>;
    using StateCallback = std::function<void(EndpointState previous, EndpointState current)>;
    using TimerToken = uint64_t;

    virtual ~ISerialLink() = default;

    /**
     * @brief Open the port at @p path with @p line.
     * @return false when it cannot be opened. A link that reconnects returns true for a port
     *         that is not there yet and reports the wait as RECONNECTING.
     */
    virtual bool open(const std::string& path, const SerialLineSettings& line) = 0;

    /** @brief Close the port. No-op when it is not open. */
    virtual void close() = 0;

    [[nodiscard]] virtual EndpointState state() const = 0;

    virtual bool write(std::span<const uint8_t> bytes) = 0;

    /** @brief Bytes accepted by write() and not yet sent. */
    [[nodiscard]] virtual size_t backlog() const = 0;

    /** @brief Set DTR and RTS, each optional so one can change alone. */
    virtual bool set_control_lines(std::optional<bool> dtr, std::optional<bool> rts) = 0;

    virtual bool send_break() = 0;

    /** @brief Run @p callback once after @p delay on the link's thread. */
    virtual TimerToken post_after(std::chrono::milliseconds delay, std::function<void()> callback) = 0;

    /** @brief Cancel a timer. No-op when it already ran. */
    virtual void cancel_post(TimerToken token) = 0;

    virtual void set_chunk_callback(ChunkCallback callback) = 0;
    virtual void set_state_callback(StateCallback callback) = 0;
};

} // namespace MayaFlux::Core
