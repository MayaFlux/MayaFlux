#include "OscParser.hpp"

#include "MayaFlux/Transitive/Protocol/BinaryBuffer.hpp"

namespace MayaFlux::Core {

std::optional<InputValue> OscParser::parse(const uint8_t* data, size_t size,
    uint32_t device_id)
{
    if (size < 4 || data[0] != '/') {
        return std::nullopt;
    }

    size_t offset = 0;

    std::string address = Transitive::Protocol::read_string(data, size, offset);
    if (address.empty() || offset >= size) {
        return std::nullopt;
    }

    std::string type_tags;
    if (offset < size && data[offset] == ',') {
        type_tags = Transitive::Protocol::read_string(data, size, offset);
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
                arguments.emplace_back(Transitive::Protocol::read_int32(data, offset));
            }
            break;

        case 'f':
            if (offset + 4 <= size) {
                arguments.emplace_back(Transitive::Protocol::read_float(data, offset));
            }
            break;

        case 's':
            arguments.emplace_back(Transitive::Protocol::read_string(data, size, offset));
            break;

        case 'b':
            arguments.emplace_back(Transitive::Protocol::read_blob(data, size, offset));
            break;

        default:
            break;
        }
    }

    return InputValue::make_osc(std::move(address), std::move(arguments), device_id);
}

std::vector<uint8_t> OscParser::serialize(const std::string& address,
    const std::vector<InputValue::OSCArg>& args)
{
    if (address.empty() || address[0] != '/') {
        return {};
    }

    std::vector<uint8_t> out;
    out.reserve(256);

    Transitive::Protocol::write_string(out, address);

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
    Transitive::Protocol::write_string(out, type_tags);

    for (const auto& arg : args) {
        std::visit([&out](const auto& val) {
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::is_same_v<T, int32_t>) {
                Transitive::Protocol::write_int32(out, val);
            } else if constexpr (std::is_same_v<T, float>) {
                Transitive::Protocol::write_float(out, val);
            } else if constexpr (std::is_same_v<T, std::string>) {
                Transitive::Protocol::write_string(out, val);
            } else if constexpr (std::is_same_v<T, std::vector<uint8_t>>) {
                Transitive::Protocol::write_blob(out, val);
            }
        },
            arg);
    }

    return out;
}

} // namespace MayaFlux::Core
