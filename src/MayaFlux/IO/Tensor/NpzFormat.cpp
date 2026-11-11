#include "NpzFormat.hpp"

#include "Detail/NpyCodec.hpp"
#include "Detail/TensorCodec.hpp"
#include "TensorFormat.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

#include "miniz.h"

namespace MayaFlux::IO {

namespace Detail {

    struct NpzArchive {
        mz_zip_archive zip {};
        bool active {};

        ~NpzArchive()
        {
            if (active) {
                mz_zip_reader_end(&zip);
            }
        }
    };

}

namespace {

    constexpr std::string_view npy_suffix = ".npy";

    std::vector<std::byte> read_entry_prefix(mz_zip_archive& zip, uint32_t index, std::string& why)
    {
        auto* iterator = mz_zip_reader_extract_iter_new(&zip, index, 0);
        if (!iterator) {
            why = "cannot open archive entry";
            return {};
        }

        std::array<std::byte, 12> leading {};
        const size_t got = mz_zip_reader_extract_iter_read(iterator, leading.data(), leading.size());

        std::vector<std::byte> prefix;
        if (const auto total = Detail::npy_prefix_size(std::span<const std::byte>(leading).first(got), why)) {
            prefix.resize(*total);
            const size_t have = std::min(got, *total);
            std::memcpy(prefix.data(), leading.data(), have);
            const size_t more = *total - have;
            if (more > 0 && mz_zip_reader_extract_iter_read(iterator, prefix.data() + have, more) != more) {
                why = "numpy header is truncated";
                prefix.clear();
            }
        }
        mz_zip_reader_extract_iter_free(iterator);
        return prefix;
    }

}

NpzReader::NpzReader() = default;

NpzReader::~NpzReader()
{
    close();
}

void NpzReader::register_with_registry()
{
    TensorFormatRegistry::instance().register_format(TensorFormat {
        .name = "npz",
        .extensions = { "npz" },
        .probe = {},
        .make_reader = []() -> std::unique_ptr<TensorReader> { return std::make_unique<NpzReader>(); },
        .make_writer = []() -> std::unique_ptr<TensorWriter> { return std::make_unique<NpzWriter>(); } });

    MF_INFO(Journal::Component::IO, Journal::Context::Init, "NpzReader registered for: npz");
}

bool NpzReader::can_read(const std::string& filepath) const
{
    return Detail::lower_extension(resolve_path(filepath)) == "npz";
}

void NpzReader::close()
{
    m_archive.reset();
    m_entries.clear();
    m_open = false;
}

bool NpzReader::open(const std::string& filepath, FileReadOptions)
{
    close();
    m_path = resolve_path(filepath);

    m_archive = std::make_unique<Detail::NpzArchive>();
    if (!mz_zip_reader_init_file(&m_archive->zip, m_path.c_str(), 0)) {
        const std::string reason = mz_zip_get_error_string(mz_zip_get_last_error(&m_archive->zip));
        close();
        return fail("cannot open archive '" + m_path + "': " + reason);
    }
    m_archive->active = true;

    const mz_uint count = mz_zip_reader_get_num_files(&m_archive->zip);
    for (mz_uint i = 0; i < count; ++i) {
        mz_zip_archive_file_stat stat {};
        if (!mz_zip_reader_file_stat(&m_archive->zip, i, &stat) || stat.m_is_directory) {
            continue;
        }

        const std::string entry_name = stat.m_filename;
        if (!entry_name.ends_with(npy_suffix)) {
            continue;
        }
        const std::string name = entry_name.substr(0, entry_name.size() - npy_suffix.size());

        std::string why;
        const auto prefix = read_entry_prefix(m_archive->zip, i, why);
        const auto header = prefix.empty() ? std::nullopt : Detail::parse_npy_header(prefix, name, why);
        if (!header) {
            MF_WARN(Journal::Component::IO, Journal::Context::FileIO,
                "NpzReader: skipping '{}': {}", entry_name, why);
            continue;
        }

        const auto bytes = header->spec.dtype.storage_bytes(header->spec.element_count().value_or(0));
        if (!bytes || header->data_offset + *bytes > stat.m_uncomp_size) {
            MF_WARN(Journal::Component::IO, Journal::Context::FileIO,
                "NpzReader: skipping '{}': data is shorter than its header declares", entry_name);
            continue;
        }

        m_entries.push_back({ .index = i, .spec = header->spec, .data_offset = header->data_offset, .big_endian = header->big_endian });
    }

    m_open = true;
    m_last_error.clear();
    return true;
}

std::vector<Kakshya::TensorSpec> NpzReader::describe() const
{
    std::vector<Kakshya::TensorSpec> specs;
    specs.reserve(m_entries.size());
    for (const auto& entry : m_entries) {
        specs.push_back(entry.spec);
    }
    return specs;
}

std::optional<Kakshya::TensorData> NpzReader::read(std::string_view name)
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

    size_t size = 0;
    void* heap = mz_zip_reader_extract_to_heap(&m_archive->zip, entry->index, &size, 0);
    if (!heap) {
        fail("cannot extract '" + std::string(name) + "'");
        return std::nullopt;
    }

    auto tensor = Kakshya::TensorData::allocate(entry->spec);
    const auto storage = tensor ? tensor->storage() : std::span<std::byte> {};
    if (!tensor || entry->data_offset + storage.size() > size) {
        mz_free(heap);
        fail("tensor '" + std::string(name) + "' is truncated");
        return std::nullopt;
    }

    if (!storage.empty()) {
        std::memcpy(storage.data(), static_cast<const std::byte*>(heap) + entry->data_offset, storage.size());
        if (entry->big_endian == Detail::host_is_little_endian()) {
            Detail::swap_components(storage, Detail::component_bytes(entry->spec.dtype));
        }
    }
    mz_free(heap);
    return tensor;
}

bool NpzWriter::write(const std::string& filepath, const Kakshya::TensorSet& tensors)
{
    m_last_error.clear();
    if (!tensors.is_consistent()) {
        m_last_error = "tensors must be valid with unique, non-empty names";
        return false;
    }

    std::vector<std::vector<std::byte>> encoded;
    encoded.reserve(tensors.tensors.size());
    for (const auto& tensor : tensors.tensors) {
        auto bytes = Detail::encode_npy(tensor, m_last_error);
        if (!bytes) {
            return false;
        }
        encoded.push_back(std::move(*bytes));
    }

    mz_zip_archive zip {};
    if (!mz_zip_writer_init_file(&zip, filepath.c_str(), 0)) {
        m_last_error = "cannot create '" + filepath + "'";
        return false;
    }

    for (size_t i = 0; i < encoded.size(); ++i) {
        const std::string entry = tensors.tensors[i].spec().name + std::string(npy_suffix);
        if (!mz_zip_writer_add_mem(&zip, entry.c_str(), encoded[i].data(), encoded[i].size(), MZ_NO_COMPRESSION)) {
            m_last_error = std::string("cannot add '") + entry + "': " + mz_zip_get_error_string(mz_zip_get_last_error(&zip));
            mz_zip_writer_end(&zip);
            return false;
        }
    }

    const bool finalized = mz_zip_writer_finalize_archive(&zip) != 0;
    mz_zip_writer_end(&zip);
    if (!finalized) {
        m_last_error = "cannot finalize '" + filepath + "'";
        return false;
    }
    return true;
}

}
