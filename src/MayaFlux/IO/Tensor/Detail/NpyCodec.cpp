#include "NpyCodec.hpp"

#include "TensorCodec.hpp"

#include <charconv>

namespace MayaFlux::IO::Detail {

namespace {

    constexpr std::array<uint8_t, 6> npy_magic { 0x93, 'N', 'U', 'M', 'P', 'Y' };

    std::string_view value_after_key(std::string_view text, std::string_view key)
    {
        for (const char quote : { '\'', '"' }) {
            std::string token;
            token += quote;
            token += key;
            token += quote;
            auto pos = text.find(token);
            if (pos == std::string_view::npos) {
                continue;
            }
            pos += token.size();
            while (pos < text.size() && (text[pos] == ' ' || text[pos] == ':')) {
                ++pos;
            }
            return text.substr(pos);
        }
        return {};
    }

    std::optional<std::vector<uint64_t>> parse_shape(std::string_view value)
    {
        if (value.empty() || value[0] != '(') {
            return std::nullopt;
        }
        const auto close = value.find(')');
        if (close == std::string_view::npos) {
            return std::nullopt;
        }

        std::vector<uint64_t> shape;
        std::string_view body = value.substr(1, close - 1);
        size_t pos = 0;
        while (pos < body.size()) {
            if (body[pos] == ' ' || body[pos] == ',') {
                ++pos;
                continue;
            }
            uint64_t extent = 0;
            const auto [end, error] = std::from_chars(body.data() + pos, body.data() + body.size(), extent);
            if (error != std::errc {}) {
                return std::nullopt;
            }
            shape.push_back(extent);
            pos = static_cast<size_t>(end - body.data());
            if (pos < body.size() && body[pos] == 'L') {
                ++pos;
            }
        }
        return shape;
    }

    std::string shape_text(const std::vector<uint64_t>& shape)
    {
        std::string text = "(";
        for (size_t i = 0; i < shape.size(); ++i) {
            if (i > 0) {
                text += ", ";
            }
            text += std::to_string(shape[i]);
        }
        if (shape.size() == 1) {
            text += ",";
        }
        return text + ")";
    }

    std::vector<std::byte> build_header(std::string_view body, uint8_t major)
    {
        const size_t prefix = major == 1 ? 10 : 12;
        std::string header(body);
        const size_t padding = (64 - (prefix + header.size() + 1) % 64) % 64;
        header.append(padding, ' ');
        header.push_back('\n');

        std::vector<std::byte> bytes;
        bytes.reserve(prefix + header.size());
        for (const auto byte : npy_magic) {
            bytes.push_back(static_cast<std::byte>(byte));
        }
        bytes.push_back(static_cast<std::byte>(major));
        bytes.push_back(std::byte { 0 });
        const size_t length_bytes = major == 1 ? 2 : 4;
        for (size_t i = 0; i < length_bytes; ++i) {
            bytes.push_back(static_cast<std::byte>((header.size() >> (8 * i)) & 0xFF));
        }
        for (const char c : header) {
            bytes.push_back(static_cast<std::byte>(c));
        }
        return bytes;
    }

}

std::optional<size_t> npy_prefix_size(std::span<const std::byte> leading, std::string& why)
{
    if (leading.size() < 10 || !std::equal(npy_magic.begin(), npy_magic.end(), leading.begin(),
            [](uint8_t expected, std::byte actual) { return expected == std::to_integer<uint8_t>(actual); })) {
        why = "missing numpy magic string";
        return std::nullopt;
    }

    const auto major = std::to_integer<uint8_t>(leading[6]);
    size_t length = 0;
    size_t prefix = 0;
    if (major == 1) {
        length = std::to_integer<size_t>(leading[8]) | (std::to_integer<size_t>(leading[9]) << 8);
        prefix = 10;
    } else if (major == 2 || major == 3) {
        if (leading.size() < 12) {
            why = "numpy header is truncated";
            return std::nullopt;
        }
        for (size_t i = 0; i < 4; ++i) {
            length |= std::to_integer<size_t>(leading[8 + i]) << (8 * i);
        }
        prefix = 12;
    } else {
        why = "numpy format version " + std::to_string(major) + " is not supported";
        return std::nullopt;
    }
    return prefix + length;
}

std::optional<NpyHeader> parse_npy_header(
    std::span<const std::byte> prefix, std::string_view name, std::string& why)
{
    const auto total = npy_prefix_size(prefix, why);
    if (!total || *total != prefix.size()) {
        if (total) {
            why = "numpy header is truncated";
        }
        return std::nullopt;
    }

    const size_t header_start = std::to_integer<uint8_t>(prefix[6]) == 1 ? 10 : 12;
    const std::string_view text(reinterpret_cast<const char*>(prefix.data()) + header_start,
        prefix.size() - header_start);

    const auto descr = value_after_key(text, "descr");
    if (descr.empty() || (descr[0] != '\'' && descr[0] != '"')) {
        why = descr.empty() ? "numpy header has no descr" : "structured numpy dtypes are not supported";
        return std::nullopt;
    }
    const auto descr_end = descr.find(descr[0], 1);
    if (descr_end == std::string_view::npos) {
        why = "numpy descr is malformed";
        return std::nullopt;
    }
    const auto typestr = npy_dtype(descr.substr(1, descr_end - 1), why);
    if (!typestr) {
        return std::nullopt;
    }

    const auto order = value_after_key(text, "fortran_order");
    bool fortran = false;
    if (order.starts_with("True")) {
        fortran = true;
    } else if (!order.starts_with("False")) {
        why = "numpy header has no fortran_order";
        return std::nullopt;
    }

    const auto shape = parse_shape(value_after_key(text, "shape"));
    if (!shape) {
        why = "numpy header has no valid shape";
        return std::nullopt;
    }

    NpyHeader header;
    header.big_endian = typestr->big_endian;
    header.data_offset = *total;
    header.spec = Kakshya::TensorSpec::contiguous(typestr->dtype, *shape, std::string(name));

    if (fortran && shape->size() > 1) {
        header.spec.strides.assign(shape->size(), 1);
        for (size_t i = 1; i < shape->size(); ++i) {
            header.spec.strides[i] = header.spec.strides[i - 1] * static_cast<int64_t>(std::max<uint64_t>((*shape)[i - 1], 1));
        }
    }

    if (const auto reason = header.spec.diagnose(); !reason.empty()) {
        why = reason;
        return std::nullopt;
    }
    return header;
}

std::optional<std::vector<std::byte>> encode_npy(const Kakshya::TensorData& tensor, std::string& why)
{
    const auto& spec = tensor.spec();
    const auto typestr = npy_typestr(spec.dtype);
    if (!typestr) {
        why = "numpy has no type for element type " + spec.dtype.name();
        return std::nullopt;
    }

    std::optional<Kakshya::TensorData> gathered;
    const auto payload = dense_bytes(tensor, gathered);
    if (!payload) {
        why = "tensor '" + spec.name + "' could not be read";
        return std::nullopt;
    }

    const std::string body = "{'descr': '" + *typestr + "', 'fortran_order': False, 'shape': "
        + shape_text(spec.shape) + ", }";
    auto bytes = build_header(body, 1);
    if (bytes.size() - 10 > 0xFFFF) {
        bytes = build_header(body, 2);
    }

    const size_t data_start = bytes.size();
    bytes.insert(bytes.end(), payload->begin(), payload->end());
    if (!host_is_little_endian()) {
        swap_components(std::span<std::byte>(bytes).subspan(data_start), component_bytes(spec.dtype));
    }
    return bytes;
}

}
