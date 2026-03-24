#pragma once

#include "MayaFlux/Core/GlobalInputConfig.hpp"
#include "MayaFlux/Core/GlobalNetworkConfig.hpp"

namespace MayaFlux::Portal::Network {

// ─────────────────────────────────────────────────────────────────────────────
// OSC
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Parse a NetworkMessage payload as an OSC message.
 *
 * Intended for coroutine consumers that receive a Core::NetworkMessage from
 * co_await source->next_message() and need a typed value without manual
 * parsing or visitor boilerplate.
 *
 * @code
 * auto msg = co_await source->next_message();
 * if (auto osc = Portal::Network::as_osc(msg)) {
 *     float val = osc->get_float(0).value_or(0.0f);
 * }
 * @endcode
 *
 * @param msg NetworkMessage received from a NetworkSource.
 * @return Parsed OSCMessage, or std::nullopt if the payload is not valid OSC.
 */
[[nodiscard]] MAYAFLUX_API std::optional<Core::InputValue::OSCMessage>
as_osc(const Core::NetworkMessage& msg);

/**
 * @brief Serialize an OSC message to wire bytes for sending via NetworkService.
 *
 * @code
 * auto bytes = Portal::Network::serialize_osc("/synth/freq", {{ 440.0f }});
 * svc->send(endpoint_id, bytes.data(), bytes.size());
 * @endcode
 *
 * @param address OSC address string (must begin with '/').
 * @param args    Typed argument list.
 * @return Serialized bytes, or empty vector on invalid address.
 */
[[nodiscard]] MAYAFLUX_API std::vector<uint8_t>
serialize_osc(const std::string& address,
    const std::vector<Core::InputValue::OSCArg>& args);

} // namespace MayaFlux::Portal::Network
