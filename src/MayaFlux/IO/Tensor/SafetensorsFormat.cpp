#include "SafetensorsFormat.hpp"

#include "Detail/TensorCodec.hpp"
#include "TensorFormat.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

#include <nlohmann/json.hpp>

namespace MayaFlux::IO {

namespace {

    constexpr uint64_t max_header_bytes = 100'000'000;

    uint64_t read_le64(const std::byte* bytes)
    {
        uint64_t value = 0;
        for (size_t i = 0; i < 8; ++i)
            value |= static_cast<uint64_t>(std::to_integer<uint8_t>(bytes[i])) << (8 * i);
        return value;
    }

    void write_le64(std::string& out, uint64_t value)
    {
        for (size_t i = 0; i < 8; ++i)
            out.push_back(static_cast<char>((value >> (8 * i)) & 0xFF));
    }

}

void SafetensorsReader::register_with_registry()
{
    TensorFormatRegistry::instance().register_format(TensorFormat {
        .name = "safetensors",
        .extensions = { "safetensors" },
        .probe = &SafetensorsReader::probe,
        .make_reader = []() -> std::unique_ptr<TensorReader> { return std::make_unique<SafetensorsReader>(); },
        .make_writer = []() -> std::unique_ptr<TensorWriter> { return std::make_unique<SafetensorsWriter>(); } });

    MF_INFO(Journal::Component::IO, Journal::Context::Init,
        "SafetensorsReader registered for: safetensors");
}

bool SafetensorsReader::probe(std::span<const std::byte> head, uint64_t file_size)
{
    if (head.size() < 9 || file_size < 9)
        return false;
    const uint64_t header_size = read_le64(head.data());
    return header_size > 0 && header_size <= max_header_bytes && header_size <= file_size - 8
        && head[8] == std::byte { '{' };
}

bool SafetensorsReader::can_read(const std::string& filepath) const
{
    const auto path = resolve_path(filepath);
    if (Detail::lower_extension(path) == "safetensors")
        return true;
    const auto head = Detail::read_file_head(path, 16);
    return head && probe(head->bytes, head->file_size);
}

bool SafetensorsReader::reject(std::string reason)
{
    close();
    return fail(std::move(reason));
}

void SafetensorsReader::close()
{
    if (m_file.is_open())
        m_file.close();
    m_entries.clear();
    m_metadata.clear();
    m_data_start = 0;
    m_open = false;
}

bool SafetensorsReader::open(const std::string& filepath, FileReadOptions)
{
    close();
    m_path = resolve_path(filepath);
    m_file.open(m_path, std::ios::binary);
    if (!m_file)
        return reject("cannot open '" + m_path + "'");

    m_file.seekg(0, std::ios::end);
    const auto file_size = static_cast<uint64_t>(m_file.tellg());
    m_file.seekg(0);

    std::array<std::byte, 8> prefix {};
    if (!m_file.read(reinterpret_cast<char*>(prefix.data()), 8))
        return reject("file is shorter than a safetensors prefix");

    const uint64_t header_size = read_le64(prefix.data());
    if (header_size == 0 || header_size > max_header_bytes || header_size > file_size - 8)
        return reject("safetensors header size " + std::to_string(header_size) + " is invalid");

    std::string text(static_cast<size_t>(header_size), '\0');
    if (!m_file.read(text.data(), static_cast<std::streamsize>(header_size)))
        return reject("safetensors header is truncated");

    const auto index = nlohmann::ordered_json::parse(text, nullptr, false);
    if (index.is_discarded() || !index.is_object())
        return reject("safetensors header is not a JSON object");

    const uint64_t data_size = file_size - 8 - header_size;

    for (const auto& [key, value] : index.items()) {
        if (key == "__metadata__") {
            if (!value.is_object())
                return reject("__metadata__ must be an object");
            for (const auto& [meta_key, meta_value] : value.items()) {
                if (!meta_value.is_string())
                    return reject("__metadata__ value for '" + meta_key + "' is not a string");
                m_metadata[meta_key] = meta_value.get<std::string>();
            }
            continue;
        }

        if (!value.is_object())
            return reject("tensor '" + key + "' is not an object");

        const auto dtype_it = value.find("dtype");
        const auto shape_it = value.find("shape");
        const auto offsets_it = value.find("data_offsets");
        if (dtype_it == value.end() || !dtype_it->is_string() || shape_it == value.end() || !shape_it->is_array()
            || offsets_it == value.end() || !offsets_it->is_array() || offsets_it->size() != 2)
            return reject("tensor '" + key + "' lacks dtype, shape or data_offsets");

        const auto dtype = Detail::safetensors_dtype(dtype_it->get<std::string>());
        if (!dtype) {
            MF_WARN(Journal::Component::IO, Journal::Context::FileIO,
                "SafetensorsReader: skipping '{}' with unsupported dtype '{}'",
                key, dtype_it->get<std::string>());
            continue;
        }

        std::vector<uint64_t> shape;
        for (const auto& extent : *shape_it) {
            if (!extent.is_number_unsigned())
                return reject("tensor '" + key + "' has a non-integer extent");
            shape.push_back(extent.get<uint64_t>());
        }
        if (!(*offsets_it)[0].is_number_unsigned() || !(*offsets_it)[1].is_number_unsigned())
            return reject("tensor '" + key + "' has non-integer data_offsets");

        Entry entry;
        entry.begin = (*offsets_it)[0].get<uint64_t>();
        entry.end = (*offsets_it)[1].get<uint64_t>();
        entry.spec = Kakshya::TensorSpec::contiguous(*dtype, std::move(shape), key);

        if (const auto why = entry.spec.diagnose(); !why.empty())
            return reject(why);

        const auto bytes = dtype->storage_bytes(entry.spec.element_count().value_or(0));
        if (!bytes || entry.end < entry.begin || entry.end > data_size || entry.end - entry.begin != *bytes)
            return reject("tensor '" + key + "' has inconsistent data_offsets");

        m_entries.push_back(std::move(entry));
    }

    m_data_start = 8 + header_size;
    m_open = true;
    m_last_error.clear();
    return true;
}

std::vector<Kakshya::TensorSpec> SafetensorsReader::describe() const
{
    std::vector<Kakshya::TensorSpec> specs;
    specs.reserve(m_entries.size());
    for (const auto& entry : m_entries)
        specs.push_back(entry.spec);
    return specs;
}

std::optional<Kakshya::TensorData> SafetensorsReader::read(std::string_view name)
{
    if (!m_open) {
        fail("no file is open");
        return std::nullopt;
    }

    const auto entry = std::ranges::find_if(m_entries,
        [name](const Entry& candidate) { return candidate.spec.name == name; });
    if (entry == m_entries.end()) {
        fail("no tensor named '" + std::string(name) + "'");
        return std::nullopt;
    }

    auto tensor = Kakshya::TensorData::allocate(entry->spec);
    if (!tensor) {
        fail("cannot allocate tensor '" + std::string(name) + "'");
        return std::nullopt;
    }

    const auto storage = tensor->storage();
    if (!storage.empty()) {
        m_file.clear();
        m_file.seekg(static_cast<std::streamoff>(m_data_start + entry->begin));
        if (!m_file.read(reinterpret_cast<char*>(storage.data()), static_cast<std::streamsize>(storage.size()))) {
            fail("tensor '" + std::string(name) + "' is truncated");
            return std::nullopt;
        }
        if (!Detail::host_is_little_endian())
            Detail::swap_components(storage, Detail::component_bytes(entry->spec.dtype));
    }
    return tensor;
}

bool SafetensorsWriter::write(const std::string& filepath, const Kakshya::TensorSet& tensors)
{
    m_last_error.clear();
    if (!tensors.is_consistent()) {
        m_last_error = "tensors must be valid with unique, non-empty names";
        return false;
    }

    nlohmann::ordered_json index = nlohmann::ordered_json::object();
    if (!tensors.metadata.empty()) {
        auto meta = nlohmann::ordered_json::object();
        for (const auto& [key, value] : tensors.metadata)
            meta[key] = value;
        index["__metadata__"] = std::move(meta);
    }

    std::vector<size_t> order(tensors.tensors.size());
    std::iota(order.begin(), order.end(), size_t { 0 });
    std::ranges::sort(order, [&tensors](size_t a, size_t b) {
        const auto& left = tensors.tensors[a].spec();
        const auto& right = tensors.tensors[b].spec();
        const auto left_width = std::max<size_t>(1, left.dtype.element_bytes());
        const auto right_width = std::max<size_t>(1, right.dtype.element_bytes());
        return left_width != right_width ? left_width > right_width : left.name < right.name;
    });

    std::vector<std::optional<Kakshya::TensorData>> gathered(tensors.tensors.size());
    std::vector<std::span<const std::byte>> payloads;
    payloads.reserve(tensors.tensors.size());

    uint64_t offset = 0;
    for (size_t i = 0; i < order.size(); ++i) {
        const auto& tensor = tensors.tensors[order[i]];
        const auto& spec = tensor.spec();

        const auto name = Detail::safetensors_name(spec.dtype);
        if (!name) {
            m_last_error = "tensor '" + spec.name + "' has element type " + spec.dtype.name()
                + " which safetensors cannot store";
            return false;
        }
        const auto bytes = Detail::dense_bytes(tensor, gathered[i]);
        if (!bytes) {
            m_last_error = "tensor '" + spec.name + "' could not be read";
            return false;
        }

        index[spec.name] = {
            { "dtype", std::string(*name) },
            { "shape", spec.shape },
            { "data_offsets", std::vector<uint64_t> { offset, offset + bytes->size() } }
        };
        offset += bytes->size();
        payloads.push_back(*bytes);
    }

    std::string header = index.dump();
    header.append((8 - header.size() % 8) % 8, ' ');

    std::string prefix;
    write_le64(prefix, header.size());

    std::ofstream file(filepath, std::ios::binary | std::ios::trunc);
    if (!file) {
        m_last_error = "cannot create '" + filepath + "'";
        return false;
    }
    file.write(prefix.data(), static_cast<std::streamsize>(prefix.size()));
    file.write(header.data(), static_cast<std::streamsize>(header.size()));

    std::vector<std::byte> swapped;
    for (size_t i = 0; i < payloads.size(); ++i) {
        std::span<const std::byte> payload = payloads[i];
        if (!Detail::host_is_little_endian()) {
            swapped.assign(payload.begin(), payload.end());
            Detail::swap_components(swapped, Detail::component_bytes(tensors.tensors[order[i]].spec().dtype));
            payload = swapped;
        }
        file.write(reinterpret_cast<const char*>(payload.data()), static_cast<std::streamsize>(payload.size()));
    }

    file.close();
    if (!file) {
        m_last_error = "failed while writing '" + filepath + "'";
        return false;
    }
    return true;
}

}
