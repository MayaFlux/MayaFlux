#include "RawTensorReader.hpp"

#include "Detail/TensorCodec.hpp"

namespace MayaFlux::IO {

bool RawTensorReader::set_layout(std::vector<RawTensorEntry> entries)
{
    for (size_t i = 0; i < entries.size(); ++i) {
        const auto& spec = entries[i].spec;
        if (spec.name.empty()) {
            return fail("every raw tensor needs a name");
        }
        if (const auto why = spec.diagnose(); !why.empty()) {
            return fail(why);
        }
        if (!spec.device.is_host_accessible()) {
            return fail("tensor '" + spec.name + "' is not host memory");
        }
        for (size_t j = i + 1; j < entries.size(); ++j) {
            if (entries[j].spec.name == spec.name) {
                return fail("duplicate tensor name '" + spec.name + "'");
            }
        }
    }
    m_layout = std::move(entries);
    return true;
}

bool RawTensorReader::can_read(const std::string& filepath) const
{
    std::error_code error;
    return std::filesystem::is_regular_file(resolve_path(filepath), error);
}

void RawTensorReader::close()
{
    if (m_file.is_open()) {
        m_file.close();
    }
    m_offsets.clear();
    m_open = false;
}

bool RawTensorReader::open(const std::string& filepath, FileReadOptions)
{
    close();
    if (m_layout.empty()) {
        return fail("set_layout() must describe the tensors before open()");
    }

    m_path = resolve_path(filepath);
    m_file.open(m_path, std::ios::binary);
    if (!m_file) {
        return fail("cannot open '" + m_path + "'");
    }
    m_file.seekg(0, std::ios::end);
    const auto file_size = static_cast<uint64_t>(m_file.tellg());

    uint64_t cursor = 0;
    for (const auto& entry : m_layout) {
        const uint64_t begin = entry.file_offset.value_or(cursor);
        const auto range = entry.spec.byte_range();
        if (!range || range->second > file_size || begin > file_size - range->second) {
            close();
            return fail("tensor '" + entry.spec.name + "' extends past the end of the file");
        }
        m_offsets.push_back(begin);
        cursor = begin + range->second;
    }

    m_open = true;
    m_last_error.clear();
    return true;
}

std::vector<Kakshya::TensorSpec> RawTensorReader::describe() const
{
    std::vector<Kakshya::TensorSpec> specs;
    specs.reserve(m_layout.size());
    for (const auto& entry : m_layout) {
        specs.push_back(entry.spec);
    }
    return specs;
}

std::optional<Kakshya::TensorData> RawTensorReader::read(std::string_view name)
{
    if (!m_open) {
        fail("no file is open");
        return std::nullopt;
    }

    const auto entry = std::ranges::find_if(m_layout,
        [name](const RawTensorEntry& candidate) { return candidate.spec.name == name; });
    if (entry == m_layout.end()) {
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
        m_file.seekg(static_cast<std::streamoff>(m_offsets[static_cast<size_t>(entry - m_layout.begin())]));
        if (!m_file.read(reinterpret_cast<char*>(storage.data()), static_cast<std::streamsize>(storage.size()))) {
            fail("tensor '" + std::string(name) + "' is truncated");
            return std::nullopt;
        }
        if (entry->big_endian == Detail::host_is_little_endian()) {
            Detail::swap_components(storage, Detail::component_bytes(entry->spec.dtype));
        }
    }
    return tensor;
}

}
