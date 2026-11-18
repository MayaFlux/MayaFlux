#pragma once

namespace Lila {

/**
 * @brief Where an evaluated block came from.
 *
 * Local: a call on the Lila object itself, which includes everything the host
 * evaluates and every block staged before the session started.
 * Client: a message a connected client sent.
 */
enum class EvalOrigin : uint8_t {
    Local,
    Client
};

/**
 * @brief What one evaluation did.
 *
 * The views are valid only for the duration of the observer call. Copy them to
 * keep them. When ok is true, reason is empty.
 */
struct EvalReport {
    EvalOrigin origin;
    bool ok;
    std::string_view code;
    std::string_view reason;
};

/**
 * @brief Receives each evaluation's report.
 *
 * A plain function and a pointer it is handed back, so nothing owned by one
 * side of the library boundary is released by the other. It runs on the thread
 * that evaluated, which for a client is the listener's, and must not block. An
 * exception that escapes it is logged and dropped.
 */
using EvalObserver = void (*)(void* user, const EvalReport& report);

} // namespace Lila
