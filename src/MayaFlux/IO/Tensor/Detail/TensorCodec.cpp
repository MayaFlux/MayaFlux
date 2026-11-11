#include "TensorCodec.hpp"

#include <bit>
#include <charconv>
#include <fstream>

namespace MayaFlux::IO::Detail {

namespace {

    using Kind = Kakshya::TensorDTypeKind;

    struct SafetensorsEntry {
        std::string_view name;
        Kind kind;
        uint8_t bits;
    };

    constexpr std::array<SafetensorsEntry, 22> k_safetensors { {
        { "BOOL", Kind::BOOL, 8 },
        { "F4", Kind::FLOAT4_E2M1FN, 4 },
        { "F6_E2M3", Kind::FLOAT6_E2M3FN, 6 },
        { "F6_E3M2", Kind::FLOAT6_E3M2FN, 6 },
        { "U8", Kind::UINT, 8 },
        { "I8", Kind::INT, 8 },
        { "F8_E5M2", Kind::FLOAT8_E5M2, 8 },
        { "F8_E4M3", Kind::FLOAT8_E4M3FN, 8 },
        { "F8_E8M0", Kind::FLOAT8_E8M0FNU, 8 },
        { "F8_E4M3FNUZ", Kind::FLOAT8_E4M3FNUZ, 8 },
        { "F8_E5M2FNUZ", Kind::FLOAT8_E5M2FNUZ, 8 },
        { "I16", Kind::INT, 16 },
        { "U16", Kind::UINT, 16 },
        { "F16", Kind::FLOAT, 16 },
        { "BF16", Kind::BFLOAT, 16 },
        { "I32", Kind::INT, 32 },
        { "U32", Kind::UINT, 32 },
        { "F32", Kind::FLOAT, 32 },
        { "C64", Kind::COMPLEX, 64 },
        { "F64", Kind::FLOAT, 64 },
        { "I64", Kind::INT, 64 },
        { "U64", Kind::UINT, 64 },
    } };

}

bool host_is_little_endian() noexcept
{
    return std::endian::native == std::endian::little;
}

size_t component_bytes(Kakshya::TensorDType dtype) noexcept
{
    const size_t bits = (dtype.kind == Kind::COMPLEX || dtype.kind == Kind::BCOMPLEX)
        ? dtype.bits / 2U
        : dtype.bits;
    return bits % 8 == 0 ? bits / 8 : 0;
}

void swap_components(std::span<std::byte> data, size_t component_bytes) noexcept
{
    if (component_bytes <= 1)
        return;
    for (size_t offset = 0; offset + component_bytes <= data.size(); offset += component_bytes)
        std::reverse(data.begin() + static_cast<std::ptrdiff_t>(offset),
            data.begin() + static_cast<std::ptrdiff_t>(offset + component_bytes));
}

std::optional<Kakshya::TensorDType> safetensors_dtype(std::string_view name) noexcept
{
    for (const auto& entry : k_safetensors) {
        if (entry.name == name)
            return Kakshya::TensorDType { .kind = entry.kind, .bits = entry.bits, .lanes = 1 };
    }
    return std::nullopt;
}

std::optional<std::string_view> safetensors_name(Kakshya::TensorDType dtype) noexcept
{
    if (dtype.lanes != 1)
        return std::nullopt;
    for (const auto& entry : k_safetensors) {
        if (entry.kind == dtype.kind && entry.bits == dtype.bits)
            return entry.name;
    }
    return std::nullopt;
}

std::optional<NpyTypestr> npy_dtype(std::string_view typestr, std::string& why)
{
    size_t pos = 0;
    bool big_endian = false;
    if (!typestr.empty()) {
        switch (typestr[0]) {
        case '>':
            big_endian = true;
            pos = 1;
            break;
        case '=':
            big_endian = !host_is_little_endian();
            pos = 1;
            break;
        case '<':
        case '|':
            pos = 1;
            break;
        default:
            break;
        }
    }
    if (pos >= typestr.size()) {
        why = "malformed numpy type string '" + std::string(typestr) + "'";
        return std::nullopt;
    }

    const char code = typestr[pos++];
    unsigned size = 0;
    const char* end = typestr.data() + typestr.size();
    const auto [parsed, error] = std::from_chars(typestr.data() + pos, end, size);
    if (error != std::errc {} || parsed != end || size == 0 || size > 16) {
        why = "malformed numpy type string '" + std::string(typestr) + "'";
        return std::nullopt;
    }

    const auto bits = static_cast<uint8_t>(size * 8);
    const auto make = [&](Kind kind) -> std::optional<NpyTypestr> {
        return NpyTypestr { .dtype = { .kind = kind, .bits = bits, .lanes = 1 }, .big_endian = big_endian && size > 1 };
    };

    switch (code) {
    case 'f':
        if (size == 2 || size == 4 || size == 8)
            return make(Kind::FLOAT);
        break;
    case 'i':
        if (size == 1 || size == 2 || size == 4 || size == 8)
            return make(Kind::INT);
        break;
    case 'u':
        if (size == 1 || size == 2 || size == 4 || size == 8)
            return make(Kind::UINT);
        break;
    case 'b':
        if (size == 1)
            return make(Kind::BOOL);
        break;
    case 'c':
        if (size == 8 || size == 16)
            return make(Kind::COMPLEX);
        break;
    default:
        break;
    }

    why = "numpy type '" + std::string(typestr) + "' is not supported";
    return std::nullopt;
}

std::optional<std::string> npy_typestr(Kakshya::TensorDType dtype)
{
    if (dtype.lanes != 1)
        return std::nullopt;

    const auto sized = [&](char code) -> std::optional<std::string> {
        const unsigned size = dtype.bits / 8U;
        return std::string(size == 1 ? "|" : "<") + code + std::to_string(size);
    };

    switch (dtype.kind) {
    case Kind::FLOAT:
        return (dtype.bits == 16 || dtype.bits == 32 || dtype.bits == 64) ? sized('f') : std::nullopt;
    case Kind::INT:
        return (dtype.bits == 8 || dtype.bits == 16 || dtype.bits == 32 || dtype.bits == 64) ? sized('i') : std::nullopt;
    case Kind::UINT:
        return (dtype.bits == 8 || dtype.bits == 16 || dtype.bits == 32 || dtype.bits == 64) ? sized('u') : std::nullopt;
    case Kind::BOOL:
        return dtype.bits == 8 ? std::optional<std::string> { "|b1" } : std::nullopt;
    case Kind::COMPLEX:
        return (dtype.bits == 64 || dtype.bits == 128) ? sized('c') : std::nullopt;
    default:
        return std::nullopt;
    }
}

std::optional<FileHead> read_file_head(const std::string& path, size_t limit)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
        return std::nullopt;

    file.seekg(0, std::ios::end);
    FileHead head;
    head.file_size = static_cast<uint64_t>(file.tellg());
    file.seekg(0);

    head.bytes.resize(static_cast<size_t>(std::min<uint64_t>(limit, head.file_size)));
    file.read(reinterpret_cast<char*>(head.bytes.data()), static_cast<std::streamsize>(head.bytes.size()));
    head.bytes.resize(static_cast<size_t>(file.gcount()));
    return head;
}

std::string lower_extension(const std::string& path)
{
    auto ext = std::filesystem::path(path).extension().string();
    if (!ext.empty() && ext[0] == '.')
        ext.erase(0, 1);
    std::ranges::transform(ext, ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext;
}

std::optional<std::span<const std::byte>> dense_bytes(
    const Kakshya::TensorData& tensor, std::optional<Kakshya::TensorData>& dense)
{
    const auto view = tensor.access();
    if (!view)
        return std::nullopt;

    const auto raw = view->bytes();
    if (!raw.empty() || view->element_count() == 0)
        return raw;

    dense = Kakshya::TensorData::copy_of(*view);
    if (!dense)
        return std::nullopt;
    const auto gathered = dense->access();
    if (!gathered)
        return std::nullopt;
    return gathered->bytes();
}

}
