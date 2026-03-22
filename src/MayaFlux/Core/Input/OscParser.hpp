#pragma once

#include "InputBinding.hpp"

namespace MayaFlux::Core {

/**
 * @class OscParser
 * @brief Stateless OSC message parser: raw UDP bytes -> InputValue
 *
 * Parses OSC 1.0 messages from raw datagram bytes. Does not handle
 * bundles (yet). Does not own a socket. Does not know about
 * NetworkService. Pure function: bytes in, optional InputValue out.
 *
 * OSC wire format:
 *   [address string, null-padded to 4-byte boundary]
 *   [type tag string starting with ',', null-padded to 4-byte boundary]
 *   [arguments, each padded to 4-byte boundary]
 *
 * Supported type tags:
 *   'i' -> int32_t (big-endian)
 *   'f' -> float (big-endian IEEE 754)
 *   's' -> null-terminated string (padded to 4 bytes)
 *   'b' -> blob: [int32 size][data, padded to 4 bytes]
 *
 * @code
 * auto result = OscParser::parse(data, size);
 * if (result) {
 *     input_manager->enqueue(*result);
 * }
 * @endcode
 */
class OscParser {
public:
    /**
     * @brief Parse a single OSC message from raw bytes
     * @param data Pointer to datagram payload.
     * @param size Payload size in bytes.
     * @param device_id Device id to stamp on the resulting InputValue.
     * @return Parsed InputValue with Type::OSC, or nullopt on parse failure.
     */
    static std::optional<InputValue> parse(const uint8_t* data, size_t size,
        uint32_t device_id = 0);

    /**
     * @brief Serialize an OSC message to wire format
     * @param address OSC address string (must start with '/').
     * @param args Typed arguments.
     * @return Serialized bytes, or empty on error.
     */
    static std::vector<uint8_t> serialize(const std::string& address,
        const std::vector<InputValue::OSCArg>& args);

private:
    static size_t padded_size(size_t len);
    static std::string read_string(const uint8_t* data, size_t max_len, size_t& offset);
    static int32_t read_int32(const uint8_t* data, size_t& offset);
    static float read_float(const uint8_t* data, size_t& offset);
    static std::vector<uint8_t> read_blob(const uint8_t* data, size_t max_len, size_t& offset);

    static void write_string(std::vector<uint8_t>& out, const std::string& str);
    static void write_int32(std::vector<uint8_t>& out, int32_t val);
    static void write_float(std::vector<uint8_t>& out, float val);
    static void write_blob(std::vector<uint8_t>& out, const std::vector<uint8_t>& blob);
};

} // namespace MayaFlux::Core
