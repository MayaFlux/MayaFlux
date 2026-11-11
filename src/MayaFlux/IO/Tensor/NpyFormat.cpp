#include "NpyFormat.hpp"

#include "Detail/NpyCodec.hpp"
#include "Detail/TensorCodec.hpp"
#include "TensorFormat.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::IO {

void NpyReader::register_with_registry()
{
    TensorFormatRegistry::instance().register_format(TensorFormat {
        .name = "npy",
        .extensions = { "npy" },
        .probe = &NpyReader::probe,
        .make_reader = []() -> std::unique_ptr<TensorReader> { return std::make_unique<NpyReader>(); },
        .make_writer = []() -> std::unique_ptr<TensorWriter> { return std::make_unique<NpyWriter>(); } });

    MF_INFO(Journal::Component::IO, Journal::Context::Init, "NpyReader registered for: npy");
}

bool NpyReader::probe(std::span<const std::byte> head, uint64_t)
{
    std::string why;
    return head.size() >= 10 && Detail::npy_prefix_size(head.first(std::min<size_t>(head.size(), 12)), why).has_value();
}

bool NpyReader::can_read(const std::string& filepath) const
{
    const auto path = resolve_path(filepath);
    if (Detail::lower_extension(path) == "npy") {
        return true;
    }
    const auto head = Detail::read_file_head(path, 12);
    return head && probe(head->bytes, head->file_size);
}

void NpyReader::close()
{
    if (m_file.is_open()) {
        m_file.close();
    }
    m_spec = {};
    m_data_offset = 0;
    m_open = false;
}

bool NpyReader::open(const std::string& filepath, FileReadOptions)
{
    close();
    m_path = resolve_path(filepath);
    m_file.open(m_path, std::ios::binary);
    if (!m_file) {
        return fail("cannot open '" + m_path + "'");
    }

    m_file.seekg(0, std::ios::end);
    const auto file_size = static_cast<uint64_t>(m_file.tellg());
    m_file.seekg(0);

    std::array<std::byte, 12> leading {};
    m_file.read(reinterpret_cast<char*>(leading.data()), static_cast<std::streamsize>(leading.size()));
    const auto got = static_cast<size_t>(m_file.gcount());
    m_file.clear();

    std::string why;
    const auto prefix_size = Detail::npy_prefix_size(std::span<const std::byte>(leading).first(got), why);
    if (!prefix_size || *prefix_size > file_size) {
        close();
        return fail(prefix_size ? "numpy header is truncated" : why);
    }

    std::vector<std::byte> prefix(*prefix_size);
    m_file.seekg(0);
    if (!m_file.read(reinterpret_cast<char*>(prefix.data()), static_cast<std::streamsize>(prefix.size()))) {
        close();
        return fail("numpy header is truncated");
    }

    const auto stem = std::filesystem::path(m_path).stem().string();
    const auto header = Detail::parse_npy_header(prefix, stem, why);
    if (!header) {
        close();
        return fail(why);
    }

    const auto bytes = header->spec.dtype.storage_bytes(header->spec.element_count().value_or(0));
    if (!bytes || header->data_offset + *bytes > file_size) {
        close();
        return fail("numpy data is shorter than its header declares");
    }

    m_spec = header->spec;
    m_data_offset = header->data_offset;
    m_big_endian = header->big_endian;
    m_open = true;
    m_last_error.clear();
    return true;
}

std::vector<Kakshya::TensorSpec> NpyReader::describe() const
{
    return m_open ? std::vector<Kakshya::TensorSpec> { m_spec } : std::vector<Kakshya::TensorSpec> {};
}

std::optional<Kakshya::TensorData> NpyReader::read(std::string_view name)
{
    if (!m_open) {
        fail("no file is open");
        return std::nullopt;
    }
    if (!name.empty() && name != m_spec.name) {
        fail("no tensor named '" + std::string(name) + "'");
        return std::nullopt;
    }

    auto tensor = Kakshya::TensorData::allocate(m_spec);
    if (!tensor) {
        fail("cannot allocate tensor '" + m_spec.name + "'");
        return std::nullopt;
    }

    const auto storage = tensor->storage();
    if (!storage.empty()) {
        m_file.clear();
        m_file.seekg(static_cast<std::streamoff>(m_data_offset));
        if (!m_file.read(reinterpret_cast<char*>(storage.data()), static_cast<std::streamsize>(storage.size()))) {
            fail("numpy data is truncated");
            return std::nullopt;
        }
        if (m_big_endian == Detail::host_is_little_endian()) {
            Detail::swap_components(storage, Detail::component_bytes(m_spec.dtype));
        }
    }
    return tensor;
}

bool NpyWriter::write(const std::string& filepath, const Kakshya::TensorSet& tensors)
{
    m_last_error.clear();
    if (tensors.tensors.size() != 1) {
        m_last_error = "a .npy file holds exactly one tensor, received " + std::to_string(tensors.tensors.size());
        return false;
    }

    const auto bytes = Detail::encode_npy(tensors.tensors.front(), m_last_error);
    if (!bytes) {
        return false;
    }

    std::ofstream file(filepath, std::ios::binary | std::ios::trunc);
    if (!file) {
        m_last_error = "cannot create '" + filepath + "'";
        return false;
    }
    file.write(reinterpret_cast<const char*>(bytes->data()), static_cast<std::streamsize>(bytes->size()));
    file.close();
    if (!file) {
        m_last_error = "failed while writing '" + filepath + "'";
        return false;
    }
    return true;
}

}
