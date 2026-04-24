#include "STBImageWriter.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

#define STB_IMAGE_WRITE_IMPLEMENTATION
// #define STBI_WRITE_NO_STDIO

#if __has_include("stb/stb_image_write.h")
#include "stb/stb_image_write.h"
#elif __has_include("stb_image_write.h")
#include "stb_image_write.h"
#else
#error "stb_image_write.h not found"
#endif

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

    /**
     * @brief stbi_write callback appending encoded bytes to a std::vector.
     */
    void stbi_memory_writer(void* context, void* data, int size)
    {
        auto* buf = static_cast<std::vector<uint8_t>*>(context);
        const auto* bytes = static_cast<const uint8_t*>(data);
        buf->insert(buf->end(), bytes, bytes + size);
    }

    /**
     * @brief Flush an encoded byte buffer to disk atomically.
     */
    bool flush_to_disk(const std::string& filepath, const std::vector<uint8_t>& buffer)
    {
        std::ofstream out(filepath, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) {
            return false;
        }
        out.write(reinterpret_cast<const char*>(buffer.data()),
            static_cast<std::streamsize>(buffer.size()));
        return out.good();
    }

} // namespace

// ============================================================================
// Registry hook
// ============================================================================

void STBImageWriter::register_with_registry()
{
    auto& reg = ImageWriterRegistry::instance();
    reg.register_writer(
        { "png", "jpg", "jpeg", "bmp", "tga", "hdr" },
        []() -> std::unique_ptr<ImageWriter> {
            return std::make_unique<STBImageWriter>();
        });

    MF_INFO(Journal::Component::IO, Journal::Context::Init,
        "STBImageWriter registered for: png, jpg, jpeg, bmp, tga, hdr");
}

// ============================================================================
// Public interface
// ============================================================================

bool STBImageWriter::can_write(const std::string& filepath) const
{
    const auto ext = extension_of(filepath);
    static const std::vector<std::string> supported = {
        "png", "jpg", "jpeg", "bmp", "tga", "hdr"
    };
    return std::ranges::find(supported, ext) != supported.end();
}

std::vector<std::string> STBImageWriter::get_supported_extensions() const
{
    return { "png", "jpg", "jpeg", "bmp", "tga", "hdr" };
}

bool STBImageWriter::write(
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

    const auto ext = extension_of(filepath);

    if (ext == "png")
        return write_png(filepath, data, options);
    if (ext == "jpg" || ext == "jpeg")
        return write_jpg(filepath, data, options);
    if (ext == "bmp")
        return write_bmp(filepath, data);
    if (ext == "tga")
        return write_tga(filepath, data);
    if (ext == "hdr")
        return write_hdr(filepath, data);

    m_last_error = "Unsupported extension: " + ext;
    MF_ERROR(Journal::Component::IO, Journal::Context::FileIO, m_last_error);
    return false;
}

// ============================================================================
// Format dispatch
// ============================================================================

bool STBImageWriter::write_png(
    const std::string& filepath,
    const ImageData& data,
    const ImageWriteOptions& options)
{
    const auto* src = data.as_uint8();
    if (!src) {
        m_last_error = "PNG requires uint8 ImageData";
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO, m_last_error);
        return false;
    }

    const int compression = (options.compression >= 0 && options.compression <= 9)
        ? options.compression
        : 6;
    stbi_write_png_compression_level = compression;
    stbi_flip_vertically_on_write(options.flip_vertically ? 1 : 0);

    std::vector<uint8_t> encoded;
    encoded.reserve(static_cast<size_t>(data.width) * data.height * data.channels);

    const int stride = static_cast<int>(data.width * data.channels);
    const int ok = stbi_write_png_to_func(
        &stbi_memory_writer,
        &encoded,
        static_cast<int>(data.width),
        static_cast<int>(data.height),
        static_cast<int>(data.channels),
        src->data(),
        stride);

    if (!ok || encoded.empty()) {
        m_last_error = "stbi_write_png_to_func failed";
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO, m_last_error);
        return false;
    }

    if (!flush_to_disk(filepath, encoded)) {
        m_last_error = "Failed to write PNG file: " + filepath;
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO, m_last_error);
        return false;
    }

    MF_INFO(Journal::Component::IO, Journal::Context::FileIO,
        "Wrote PNG: {} ({}x{}, {} channels, {} bytes)",
        filepath, data.width, data.height, data.channels, encoded.size());
    return true;
}

bool STBImageWriter::write_jpg(
    const std::string& filepath,
    const ImageData& data,
    const ImageWriteOptions& options)
{
    const auto* src = data.as_uint8();
    if (!src) {
        m_last_error = "JPG requires uint8 ImageData";
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO, m_last_error);
        return false;
    }

    if (data.channels != 1 && data.channels != 3) {
        m_last_error = "JPG supports only 1 or 3 channels (got "
            + std::to_string(data.channels) + ")";
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO, m_last_error);
        return false;
    }

    const int quality = std::clamp(options.quality, 1, 100);
    stbi_flip_vertically_on_write(options.flip_vertically ? 1 : 0);

    std::vector<uint8_t> encoded;
    const int ok = stbi_write_jpg_to_func(
        &stbi_memory_writer,
        &encoded,
        static_cast<int>(data.width),
        static_cast<int>(data.height),
        static_cast<int>(data.channels),
        src->data(),
        quality);

    if (!ok || encoded.empty()) {
        m_last_error = "stbi_write_jpg_to_func failed";
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO, m_last_error);
        return false;
    }

    if (!flush_to_disk(filepath, encoded)) {
        m_last_error = "Failed to write JPG file: " + filepath;
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO, m_last_error);
        return false;
    }

    MF_INFO(Journal::Component::IO, Journal::Context::FileIO,
        "Wrote JPG: {} ({}x{}, quality {}, {} bytes)",
        filepath, data.width, data.height, quality, encoded.size());
    return true;
}

bool STBImageWriter::write_bmp(const std::string& filepath, const ImageData& data)
{
    const auto* src = data.as_uint8();
    if (!src) {
        m_last_error = "BMP requires uint8 ImageData";
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO, m_last_error);
        return false;
    }

    std::vector<uint8_t> encoded;
    const int ok = stbi_write_bmp_to_func(
        &stbi_memory_writer,
        &encoded,
        static_cast<int>(data.width),
        static_cast<int>(data.height),
        static_cast<int>(data.channels),
        src->data());

    if (!ok || encoded.empty()) {
        m_last_error = "stbi_write_bmp_to_func failed";
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO, m_last_error);
        return false;
    }

    if (!flush_to_disk(filepath, encoded)) {
        m_last_error = "Failed to write BMP file: " + filepath;
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO, m_last_error);
        return false;
    }

    MF_INFO(Journal::Component::IO, Journal::Context::FileIO,
        "Wrote BMP: {} ({}x{}, {} channels)", filepath, data.width, data.height, data.channels);
    return true;
}

bool STBImageWriter::write_tga(const std::string& filepath, const ImageData& data)
{
    const auto* src = data.as_uint8();
    if (!src) {
        m_last_error = "TGA requires uint8 ImageData";
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO, m_last_error);
        return false;
    }

    std::vector<uint8_t> encoded;
    const int ok = stbi_write_tga_to_func(
        &stbi_memory_writer,
        &encoded,
        static_cast<int>(data.width),
        static_cast<int>(data.height),
        static_cast<int>(data.channels),
        src->data());

    if (!ok || encoded.empty()) {
        m_last_error = "stbi_write_tga_to_func failed";
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO, m_last_error);
        return false;
    }

    if (!flush_to_disk(filepath, encoded)) {
        m_last_error = "Failed to write TGA file: " + filepath;
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO, m_last_error);
        return false;
    }

    MF_INFO(Journal::Component::IO, Journal::Context::FileIO,
        "Wrote TGA: {} ({}x{}, {} channels)", filepath, data.width, data.height, data.channels);
    return true;
}

bool STBImageWriter::write_hdr(const std::string& filepath, const ImageData& data)
{
    const auto* src = data.as_float();
    if (!src) {
        m_last_error = "HDR requires float ImageData";
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO, m_last_error);
        return false;
    }

    if (data.channels != 3 && data.channels != 4) {
        m_last_error = "HDR supports only 3 or 4 channels (got "
            + std::to_string(data.channels) + ")";
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO, m_last_error);
        return false;
    }

    std::vector<uint8_t> encoded;
    const int ok = stbi_write_hdr_to_func(
        &stbi_memory_writer,
        &encoded,
        static_cast<int>(data.width),
        static_cast<int>(data.height),
        static_cast<int>(data.channels),
        src->data());

    if (!ok || encoded.empty()) {
        m_last_error = "stbi_write_hdr_to_func failed";
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO, m_last_error);
        return false;
    }

    if (!flush_to_disk(filepath, encoded)) {
        m_last_error = "Failed to write HDR file: " + filepath;
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO, m_last_error);
        return false;
    }

    MF_INFO(Journal::Component::IO, Journal::Context::FileIO,
        "Wrote HDR: {} ({}x{}, {} channels)", filepath, data.width, data.height, data.channels);
    return true;
}

} // namespace MayaFlux::IO
