#include "EXRWriter.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

#include <tinyexr.h>

#include <fstream>

namespace MayaFlux::IO {

namespace {

    std::string extension_of(const std::string& filepath)
    {
        auto ext = std::filesystem::path(filepath).extension().string();
        if (!ext.empty() && ext[0] == '.') {
            ext = ext.substr(1);
        }
        std::ranges::transform(ext, ext.begin(),
            [](unsigned char c) { return std::tolower(c); });
        return ext;
    }

    bool flush_to_disk(const std::string& filepath, const unsigned char* bytes, size_t size)
    {
        std::ofstream out(filepath, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) {
            return false;
        }
        out.write(reinterpret_cast<const char*>(bytes),
            static_cast<std::streamsize>(size));
        return out.good();
    }

    int resolve_compression(int requested)
    {
        if (requested < 0) {
            return TINYEXR_COMPRESSIONTYPE_ZIP;
        }
        switch (requested) {
        case TINYEXR_COMPRESSIONTYPE_NONE:
        case TINYEXR_COMPRESSIONTYPE_RLE:
        case TINYEXR_COMPRESSIONTYPE_ZIPS:
        case TINYEXR_COMPRESSIONTYPE_ZIP:
        case TINYEXR_COMPRESSIONTYPE_PIZ:
            return requested;
        default:
            return TINYEXR_COMPRESSIONTYPE_ZIP;
        }
    }

} // namespace

// ============================================================================
// Registry hook
// ============================================================================

void EXRWriter::register_with_registry()
{
    auto& reg = ImageWriterRegistry::instance();
    reg.register_writer(
        { "exr" },
        []() -> std::unique_ptr<ImageWriter> {
            return std::make_unique<EXRWriter>();
        });

    MF_INFO(Journal::Component::IO, Journal::Context::Init,
        "EXRWriter registered for: exr");
}

// ============================================================================
// Public interface
// ============================================================================

bool EXRWriter::can_write(const std::string& filepath) const
{
    return extension_of(filepath) == "exr";
}

std::vector<std::string> EXRWriter::get_supported_extensions() const
{
    return { "exr" };
}

// ============================================================================
// Channel name resolution
// ============================================================================

std::vector<std::string> EXRWriter::resolve_channel_names(
    const ImageData& data, const ImageWriteOptions& options) const
{
    if (!options.channel_names.empty()) {
        if (options.channel_names.size() != data.channels) {
            MF_WARN(Journal::Component::IO, Journal::Context::FileIO,
                "EXRWriter: channel_names size ({}) does not match channels ({}); "
                "falling back to defaults",
                options.channel_names.size(), data.channels);
        } else {
            return options.channel_names;
        }
    }

    switch (data.channels) {
    case 1:
        return { "Y" };
    case 2:
        return { "Y", "A" };
    case 3:
        return { "B", "G", "R" };
    case 4:
        return { "A", "B", "G", "R" };
    default: {
        std::vector<std::string> names;
        names.reserve(data.channels);
        for (uint32_t i = 0; i < data.channels; ++i) {
            names.emplace_back("C" + std::to_string(i));
        }
        return names;
    }
    }
}

// ============================================================================
// Deinterleave
// ============================================================================

std::vector<std::vector<float>> EXRWriter::deinterleave(
    const std::vector<float>& src,
    uint32_t width, uint32_t height, uint32_t channels,
    bool flip_vertically)
{
    const size_t pixel_count = static_cast<size_t>(width) * height;
    std::vector<std::vector<float>> planes(channels, std::vector<float>(pixel_count));

    for (uint32_t y = 0; y < height; ++y) {
        const uint32_t src_row = flip_vertically ? (height - 1 - y) : y;
        const size_t src_row_offset = static_cast<size_t>(src_row) * width * channels;
        const size_t dst_row_offset = static_cast<size_t>(y) * width;

        for (uint32_t x = 0; x < width; ++x) {
            const size_t src_pixel = src_row_offset + static_cast<size_t>(x) * channels;
            const size_t dst_pixel = dst_row_offset + x;

            for (uint32_t c = 0; c < channels; ++c) {
                planes[c][dst_pixel] = src[src_pixel + c];
            }
        }
    }

    return planes;
}

// ============================================================================
// Write
// ============================================================================

bool EXRWriter::write(
    const std::string& filepath,
    const ImageData& data,
    const ImageWriteOptions& options)
{
    m_last_error.clear();

    if (!data.is_consistent()) {
        m_last_error = "ImageData variant does not match declared ImageFormat";
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO, m_last_error);
        return false;
    }

    if (data.width == 0 || data.height == 0 || data.channels == 0) {
        m_last_error = "ImageData has zero dimensions";
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO, m_last_error);
        return false;
    }

    const auto* src = data.as_float();
    if (!src) {
        m_last_error = "EXR requires float ImageData (uint8 and uint16 not supported)";
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO, m_last_error);
        return false;
    }

    const size_t expected_elements = static_cast<size_t>(data.width) * data.height * data.channels;
    if (src->size() < expected_elements) {
        m_last_error = "EXR source buffer too small: expected "
            + std::to_string(expected_elements)
            + " got " + std::to_string(src->size());
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO, m_last_error);
        return false;
    }

    const auto names = resolve_channel_names(data, options);
    auto planes = deinterleave(*src, data.width, data.height, data.channels,
        options.flip_vertically);

    // ------------------------------------------------------------------------
    // Populate EXRImage
    // ------------------------------------------------------------------------
    EXRImage image;
    InitEXRImage(&image);

    image.num_channels = static_cast<int>(data.channels);
    image.width = static_cast<int>(data.width);
    image.height = static_cast<int>(data.height);

    std::vector<float*> plane_ptrs(data.channels);
    for (uint32_t c = 0; c < data.channels; ++c) {
        plane_ptrs[c] = planes[c].data();
    }
    image.images = reinterpret_cast<unsigned char**>(plane_ptrs.data());

    // ------------------------------------------------------------------------
    // Populate EXRHeader
    // ------------------------------------------------------------------------
    EXRHeader header;
    InitEXRHeader(&header);

    header.num_channels = static_cast<int>(data.channels);
    header.compression_type = resolve_compression(options.compression);

    std::vector<EXRChannelInfo> channel_infos(data.channels);
    std::vector<int> pixel_types(data.channels, TINYEXR_PIXELTYPE_FLOAT);
    std::vector<int> requested_pixel_types(data.channels, TINYEXR_PIXELTYPE_FLOAT);

    for (uint32_t c = 0; c < data.channels; ++c) {
        std::memset(&channel_infos[c], 0, sizeof(EXRChannelInfo));
        const std::string& n = names[c];
        const size_t copy_len = std::min<size_t>(n.size(), 255);
        std::memcpy(channel_infos[c].name, n.data(), copy_len);
        channel_infos[c].name[copy_len] = '\0';
    }

    header.channels = channel_infos.data();
    header.pixel_types = pixel_types.data();
    header.requested_pixel_types = requested_pixel_types.data();

    // ------------------------------------------------------------------------
    // Encode to memory, then flush
    // ------------------------------------------------------------------------
    unsigned char* encoded = nullptr;
    const char* err = nullptr;

    const size_t encoded_size = SaveEXRImageToMemory(
        &image, &header, &encoded, &err);

    if (encoded_size == 0 || encoded == nullptr) {
        m_last_error = err ? std::string("tinyexr: ") + err
                           : "SaveEXRImageToMemory returned 0 bytes";
        if (err) {
            FreeEXRErrorMessage(err);
        }
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO, m_last_error);
        return false;
    }

    const bool ok = flush_to_disk(filepath, encoded, encoded_size);

    std::free(encoded);

    if (!ok) {
        m_last_error = "Failed to write EXR file: " + filepath;
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO, m_last_error);
        return false;
    }

    MF_INFO(Journal::Component::IO, Journal::Context::FileIO,
        "Wrote EXR: {} ({}x{}, {} channels, compression {}, {} bytes)",
        filepath, data.width, data.height, data.channels,
        header.compression_type, encoded_size);

    return true;
}

} // namespace MayaFlux::IO
