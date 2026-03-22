#include "OscParser.hpp"

#include "bit"

namespace MayaFlux::Core {

size_t OscParser::padded_size(size_t len)
{
    return (len + 3) & ~size_t(3);
}

std::string OscParser::read_string(const uint8_t* data, size_t max_len, size_t& offset)
{
    const auto start = reinterpret_cast<const char*>(data + offset);
    size_t len = strnlen(start, max_len - offset);
    std::string result(start, len);
    offset += padded_size(len + 1);
    return result;
}

int32_t OscParser::read_int32(const uint8_t* data, size_t& offset)
{
    int32_t val {};
    std::memcpy(&val, data + offset, 4);
    offset += 4;
    if constexpr (std::endian::native == std::endian::little) {
        val = std::byteswap(static_cast<uint32_t>(val));
    }
    return val;
}

float OscParser::read_float(const uint8_t* data, size_t& offset)
{
    uint32_t raw {};
    std::memcpy(&raw, data + offset, 4);
    offset += 4;
    if constexpr (std::endian::native == std::endian::little) {
        raw = std::byteswap(raw);
    }
    float val {};
    std::memcpy(&val, &raw, 4);
    return val;
}

std::vector<uint8_t> OscParser::read_blob(const uint8_t* data, size_t max_len, size_t& offset)
{
    int32_t blob_size = read_int32(data, offset);
    if (blob_size < 0 || offset + static_cast<size_t>(blob_size) > max_len) {
        return {};
    }
    std::vector<uint8_t> result(data + offset, data + offset + blob_size);
    offset += padded_size(static_cast<size_t>(blob_size));
    return result;
}

std::optional<InputValue> OscParser::parse(const uint8_t* data, size_t size,
    uint32_t device_id)
{
    if (size < 4 || data[0] != '/') {
        return std::nullopt;
    }

    size_t offset = 0;

    std::string address = read_string(data, size, offset);
    if (address.empty() || offset >= size) {
        return std::nullopt;
    }

    std::string type_tags;
    if (offset < size && data[offset] == ',') {
        type_tags = read_string(data, size, offset);
        if (!type_tags.empty() && type_tags[0] == ',') {
            type_tags = type_tags.substr(1);
        }
    }

    std::vector<InputValue::OSCArg> arguments;
    arguments.reserve(type_tags.size());

    for (char tag : type_tags) {
        if (offset > size) {
            break;
        }

        switch (tag) {
        case 'i':
            if (offset + 4 <= size) {
                arguments.emplace_back(read_int32(data, offset));
            }
            break;

        case 'f':
            if (offset + 4 <= size) {
                arguments.emplace_back(read_float(data, offset));
            }
            break;

        case 's':
            arguments.emplace_back(read_string(data, size, offset));
            break;

        case 'b':
            arguments.emplace_back(read_blob(data, size, offset));
            break;

        default:
            break;
        }
    }

    return InputValue::make_osc(std::move(address), std::move(arguments), device_id);
}

// ─────────────────────────────────────────────────────────────────────────────
// Serialization
// ─────────────────────────────────────────────────────────────────────────────

void OscParser::write_string(std::vector<uint8_t>& out, const std::string& str)
{
    out.insert(out.end(), str.begin(), str.end());
    size_t padded = padded_size(str.size() + 1);
    out.resize(out.size() + (padded - str.size()), 0);
}

void OscParser::write_int32(std::vector<uint8_t>& out, int32_t val)
{
    auto raw = static_cast<uint32_t>(val);
    if constexpr (std::endian::native == std::endian::little) {
        raw = std::byteswap(raw);
    }
    const auto* bytes = reinterpret_cast<const uint8_t*>(&raw);
    out.insert(out.end(), bytes, bytes + 4);
}

void OscParser::write_float(std::vector<uint8_t>& out, float val)
{
    uint32_t raw {};
    std::memcpy(&raw, &val, 4);
    if constexpr (std::endian::native == std::endian::little) {
        raw = std::byteswap(raw);
    }
    const auto* bytes = reinterpret_cast<const uint8_t*>(&raw);
    out.insert(out.end(), bytes, bytes + 4);
}

void OscParser::write_blob(std::vector<uint8_t>& out, const std::vector<uint8_t>& blob)
{
    write_int32(out, static_cast<int32_t>(blob.size()));
    out.insert(out.end(), blob.begin(), blob.end());
    size_t padded = padded_size(blob.size());
    out.resize(out.size() + (padded - blob.size()), 0);
}

std::vector<uint8_t> OscParser::serialize(const std::string& address,
    const std::vector<InputValue::OSCArg>& args)
{
    if (address.empty() || address[0] != '/') {
        return {};
    }

    std::vector<uint8_t> out;
    out.reserve(256);

    write_string(out, address);

    std::string type_tags = ",";
    for (const auto& arg : args) {
        std::visit([&type_tags](const auto& val) {
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::is_same_v<T, int32_t>) {
                type_tags += 'i';
            } else if constexpr (std::is_same_v<T, float>) {
                type_tags += 'f';
            } else if constexpr (std::is_same_v<T, std::string>) {
                type_tags += 's';
            } else if constexpr (std::is_same_v<T, std::vector<uint8_t>>) {
                type_tags += 'b';
            }
        },
            arg);
    }
    write_string(out, type_tags);

    for (const auto& arg : args) {
        std::visit([&out](const auto& val) {
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::is_same_v<T, int32_t>) {
                write_int32(out, val);
            } else if constexpr (std::is_same_v<T, float>) {
                write_float(out, val);
            } else if constexpr (std::is_same_v<T, std::string>) {
                write_string(out, val);
            } else if constexpr (std::is_same_v<T, std::vector<uint8_t>>) {
                write_blob(out, val);
            }
        },
            arg);
    }

    return out;
}

} // namespace MayaFlux::Core
