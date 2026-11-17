#pragma once

#include "Address.hpp"
#include "Result.hpp"

namespace Lila {

/**
 * @class Listener
 * @brief Accepts clients that send code to a Lila.
 *
 * The address chooses the transport, and the protocol is plain text on both, so
 * anything that can write to a socket can use it: ncat, socat, a script, an editor.
 *
 * A message ends where a read ends in a newline, with trailing CR and LF removed,
 * so a block of several lines sent in one write is one message. Send each block in
 * a single write: a block longer than a read can be split mid-block, and a block
 * over 1 MiB is dropped.
 *
 * A message starting with '@' is a control message: "@session <id>" names the
 * connection and "@ping" is answered with pong. Every other message is evaluated.
 * Each message is answered with one line, in order: the result text, empty when
 * there is none, or {"status":"error","message":"..."} on failure. Control
 * messages are answered with {"status":"success",...} or an error.
 *
 * One io thread runs the message handler, so a long eval holds up every client.
 * Set the handler before start().
 */
class LILA_API Listener {
public:
    /// @brief Handles the body of an Eval request. The value is the Ok text, the error is the Error text.
    using MessageHandler = std::function<Result<std::string>(std::string_view)>;

    virtual ~Listener() = default;

    Listener(const Listener&) = delete;
    Listener& operator=(const Listener&) = delete;
    Listener(Listener&&) = delete;
    Listener& operator=(Listener&&) = delete;

    /**
     * @brief A listener for the given address. It is not listening until start().
     */
    [[nodiscard]] static std::unique_ptr<Listener> create(const Address& where);

    void set_message_handler(MessageHandler handler) { m_message = std::move(handler); }

    /**
     * @brief Begin listening.
     * @return Success, or why it could not.
     */
    [[nodiscard]] virtual Result<void> start() = 0;

    /**
     * @brief Stop listening and disconnect every client. A socket file is removed.
     */
    virtual void stop() noexcept = 0;

    [[nodiscard]] virtual bool is_running() const = 0;

    /**
     * @brief Where this listens, with anything resolved at start filled in.
     */
    [[nodiscard]] virtual Address address() const = 0;

protected:
    Listener() = default;

    MessageHandler m_message;
};

} // namespace Lila
