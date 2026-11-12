#include "STBImageWriter.hpp"

#include "MayaFlux/IO/FileWriter.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#include <miniz.h>

namespace {

    thread_local int png_compression_level = 6;

    unsigned char* compress_png(unsigned char* data, int size, int* output_size, int)
    {
        mz_ulong compressed_size = mz_compressBound(static_cast<mz_ulong>(size));
        if (compressed_size > static_cast<mz_ulong>(std::numeric_limits<int>::max() - 57)) {
            return nullptr;
        }
        auto* compressed = static_cast<unsigned char*>(std::malloc(compressed_size));
        if (!compressed) {
            return nullptr;
        }
        if (mz_compress2(compressed, &compressed_size, data,
                static_cast<mz_ulong>(size), png_compression_level)
            != MZ_OK) {
            std::free(compressed);
            return nullptr;
        }
        *output_size = static_cast<int>(compressed_size);
        return compressed;
    }

}

#define STBIW_ZLIB_COMPRESS compress_png
#define STB_IMAGE_WRITE_STATIC
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

    template <typename T>
    std::vector<T> flipped_rows(const std::vector<T>& pixels, uint32_t width, uint32_t height, uint32_t channels)
    {
        const size_t row_size = static_cast<size_t>(width) * channels;
        std::vector<T> result(row_size * height);
        for (size_t y = 0; y < height; ++y) {
            std::copy_n(pixels.data() + (height - 1 - y) * row_size, row_size,
                result.data() + y * row_size);
        }
        return result;
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
        const auto resolved = resolve_write_path(filepath);
        std::ofstream out(resolved, std::ios::binary | std::ios::trunc);
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

    const auto max_int = static_cast<uint32_t>(std::numeric_limits<int>::max());
    if (data.channels > 4 || data.width > max_int / data.channels || data.height > max_int) {
        m_last_error = "ImageData dimensions or channels exceed STB limits";
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO, m_last_error);
        return false;
    }

    const size_t row_elements = static_cast<size_t>(data.width) * data.channels;
    if (data.height > std::numeric_limits<size_t>::max() / row_elements) {
        m_last_error = "ImageData dimensions exceed addressable storage";
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO, m_last_error);
        return false;
    }
    const size_t expected_elements = row_elements * data.height;
    if (expected_elements > max_int) {
        m_last_error = "ImageData dimensions exceed STB limits";
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO, m_last_error);
        return false;
    }
    if (data.element_count() < expected_elements) {
        m_last_error = "ImageData source buffer too small: expected "
            + std::to_string(expected_elements) + " got " + std::to_string(data.element_count());
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO, m_last_error);
        return false;
    }

    const auto ext = extension_of(filepath);

    if (ext == "png" && data.height > max_int / (row_elements + 1)) {
        m_last_error = "PNG dimensions exceed STB limits";
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO, m_last_error);
        return false;
    }

    if (ext == "png")
        return write_png(filepath, data, options);
    if (ext == "jpg" || ext == "jpeg")
        return write_jpg(filepath, data, options);
    if (ext == "bmp")
        return write_bmp(filepath, data, options);
    if (ext == "tga")
        return write_tga(filepath, data, options);
    if (ext == "hdr")
        return write_hdr(filepath, data, options);

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
    std::vector<uint8_t> encoded;
    encoded.reserve(static_cast<size_t>(data.width) * data.height * data.channels);
    const auto flipped = options.flip_vertically
        ? flipped_rows(*src, data.width, data.height, data.channels)
        : std::vector<uint8_t> {};
    const auto* pixels = options.flip_vertically ? flipped.data() : src->data();
    const int stride = static_cast<int>(data.width * data.channels);
    png_compression_level = compression;
    const int ok = stbi_write_png_to_func(
        &stbi_memory_writer,
        &encoded,
        static_cast<int>(data.width),
        static_cast<int>(data.height),
        static_cast<int>(data.channels),
        pixels,
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
    std::vector<uint8_t> encoded;
    const auto flipped = options.flip_vertically
        ? flipped_rows(*src, data.width, data.height, data.channels)
        : std::vector<uint8_t> {};
    const auto* pixels = options.flip_vertically ? flipped.data() : src->data();
    const int ok = stbi_write_jpg_to_func(
        &stbi_memory_writer,
        &encoded,
        static_cast<int>(data.width),
        static_cast<int>(data.height),
        static_cast<int>(data.channels),
        pixels,
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

bool STBImageWriter::write_bmp(
    const std::string& filepath, const ImageData& data, const ImageWriteOptions& options)
{
    const auto* src = data.as_uint8();
    if (!src) {
        m_last_error = "BMP requires uint8 ImageData";
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO, m_last_error);
        return false;
    }

    std::vector<uint8_t> encoded;
    const auto flipped = options.flip_vertically
        ? flipped_rows(*src, data.width, data.height, data.channels)
        : std::vector<uint8_t> {};
    const auto* pixels = options.flip_vertically ? flipped.data() : src->data();
    const int ok = stbi_write_bmp_to_func(
        &stbi_memory_writer,
        &encoded,
        static_cast<int>(data.width),
        static_cast<int>(data.height),
        static_cast<int>(data.channels),
        pixels);

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

bool STBImageWriter::write_tga(
    const std::string& filepath, const ImageData& data, const ImageWriteOptions& options)
{
    const auto* src = data.as_uint8();
    if (!src) {
        m_last_error = "TGA requires uint8 ImageData";
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO, m_last_error);
        return false;
    }

    std::vector<uint8_t> encoded;
    const auto flipped = options.flip_vertically
        ? flipped_rows(*src, data.width, data.height, data.channels)
        : std::vector<uint8_t> {};
    const auto* pixels = options.flip_vertically ? flipped.data() : src->data();
    const int ok = stbi_write_tga_to_func(
        &stbi_memory_writer,
        &encoded,
        static_cast<int>(data.width),
        static_cast<int>(data.height),
        static_cast<int>(data.channels),
        pixels);

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

bool STBImageWriter::write_hdr(
    const std::string& filepath, const ImageData& data, const ImageWriteOptions& options)
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
    const auto flipped = options.flip_vertically
        ? flipped_rows(*src, data.width, data.height, data.channels)
        : std::vector<float> {};
    const auto* pixels = options.flip_vertically ? flipped.data() : src->data();
    const int ok = stbi_write_hdr_to_func(
        &stbi_memory_writer,
        &encoded,
        static_cast<int>(data.width),
        static_cast<int>(data.height),
        static_cast<int>(data.channels),
        pixels);

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
