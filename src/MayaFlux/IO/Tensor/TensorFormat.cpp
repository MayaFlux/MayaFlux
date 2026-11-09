#include "TensorFormat.hpp"

#include "Detail/TensorCodec.hpp"

namespace MayaFlux::IO {

TensorFormatRegistry& TensorFormatRegistry::instance()
{
    static TensorFormatRegistry registry;
    return registry;
}

void TensorFormatRegistry::register_format(TensorFormat format)
{
    for (auto& existing : m_formats) {
        if (existing.name == format.name) {
            existing = std::move(format);
            return;
        }
    }
    m_formats.push_back(std::move(format));
}

const TensorFormat* TensorFormatRegistry::find(std::string_view name) const
{
    for (const auto& format : m_formats) {
        if (format.name == name)
            return &format;
    }
    return nullptr;
}

const TensorFormat* TensorFormatRegistry::by_extension(const std::string& filepath, bool need_writer) const
{
    const auto ext = Detail::lower_extension(filepath);
    if (ext.empty())
        return nullptr;

    for (const auto& format : m_formats) {
        if (need_writer && !format.make_writer)
            continue;
        if (std::ranges::find(format.extensions, ext) != format.extensions.end())
            return &format;
    }
    return nullptr;
}

const TensorFormat* TensorFormatRegistry::detect(const std::string& filepath) const
{
    const auto path = FileReader::resolve_path(filepath);

    if (const auto head = Detail::read_file_head(path, 4096)) {
        for (const auto& format : m_formats) {
            if (format.probe && format.make_reader && format.probe(head->bytes, head->file_size))
                return &format;
        }
    }
    return by_extension(path, false);
}

std::unique_ptr<TensorReader> TensorFormatRegistry::create_reader(
    const std::string& filepath, std::string_view format) const
{
    const TensorFormat* chosen = format.empty() ? detect(filepath) : find(format);
    return chosen && chosen->make_reader ? chosen->make_reader() : nullptr;
}

std::unique_ptr<TensorWriter> TensorFormatRegistry::create_writer(
    const std::string& filepath, std::string_view format) const
{
    const TensorFormat* chosen = format.empty() ? by_extension(filepath, true) : find(format);
    return chosen && chosen->make_writer ? chosen->make_writer() : nullptr;
}

std::vector<std::string> TensorFormatRegistry::format_names() const
{
    std::vector<std::string> names;
    names.reserve(m_formats.size());
    for (const auto& format : m_formats)
        names.push_back(format.name);
    return names;
}

}
