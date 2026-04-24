#include "ImageReader.hpp"

#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKImage.hpp"

#include "MayaFlux/Buffers/Staging/StagingUtils.hpp"
#include "MayaFlux/Buffers/Textures/TextureBuffer.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

#ifdef MAYAFLUX_ARCH_X64
#define STBI_SSE2
#ifdef MAYAFLUX_COMPILER_MSVC
#include <immintrin.h>
#endif
#endif

#ifdef MAYAFLUX_ARCH_ARM64
#define STBI_NEON
#endif

#define STBI_NO_FAILURE_STRINGS
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO

#if __has_include("stb/stb_image.h")
#include "stb/stb_image.h"
#elif __has_include("stb_image.h")
#include "stb_image.h"
#else
#error "stb_image.h not found"
#endif

#include <tinyexr.h>

#include <cstddef>
#include <fstream>

namespace MayaFlux::IO {

namespace {

    std::string extension_of(const std::filesystem::path& path)
    {
        auto ext = path.extension().string();
        if (!ext.empty() && ext[0] == '.') {
            ext = ext.substr(1);
        }
        std::ranges::transform(ext, ext.begin(),
            [](unsigned char c) { return std::tolower(c); });
        return ext;
    }

    /**
     * @brief Decode an OpenEXR file from an in-memory byte buffer.
     *
     * Uses tinyexr's ParseEXRHeaderFromMemory + LoadEXRImageFromMemory pair to
     * access arbitrary channel counts and names. Reinterleaves tinyexr's planar
     * output into pixel-interleaved float layout to match ImageData's contract.
     *
     * Maps channel count to ImageFormat (R32F / RG32F / RGBA32F). 3-channel EXR
     * (BGR) is promoted to 4-channel (ABGR) with A=1.0 because we don't support
     * a 3-channel float ImageFormat.
     *
     * Half-precision pixel types are converted to float on the fly; EXR files
     * with mixed pixel types are not currently supported.
     */
    std::optional<ImageData> load_exr_from_memory(
        const unsigned char* bytes, size_t size)
    {
        using F = Portal::Graphics::ImageFormat;

        EXRVersion version;
        if (ParseEXRVersionFromMemory(&version, bytes, size) != TINYEXR_SUCCESS) {
            MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
                "EXR: failed to parse version");
            return std::nullopt;
        }
        if (version.multipart || version.non_image) {
            MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
                "EXR: multipart and deep images are not supported");
            return std::nullopt;
        }

        EXRHeader header;
        InitEXRHeader(&header);

        const char* err = nullptr;
        if (ParseEXRHeaderFromMemory(&header, &version, bytes, size, &err)
            != TINYEXR_SUCCESS) {
            MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
                "EXR: header parse failed: {}", err ? err : "unknown");
            if (err)
                FreeEXRErrorMessage(err);
            return std::nullopt;
        }

        for (int i = 0; i < header.num_channels; ++i) {
            if (header.pixel_types[i] == TINYEXR_PIXELTYPE_UINT) {
                MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
                    "EXR: uint pixel type channel '{}' not supported",
                    header.channels[i].name);
                FreeEXRHeader(&header);
                return std::nullopt;
            }
            header.requested_pixel_types[i] = TINYEXR_PIXELTYPE_FLOAT;
        }

        EXRImage image;
        InitEXRImage(&image);

        if (LoadEXRImageFromMemory(&image, &header, bytes, size, &err)
            != TINYEXR_SUCCESS) {
            MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
                "EXR: image load failed: {}", err ? err : "unknown");
            if (err)
                FreeEXRErrorMessage(err);
            FreeEXRHeader(&header);
            return std::nullopt;
        }

        const auto width = static_cast<uint32_t>(image.width);
        const auto height = static_cast<uint32_t>(image.height);
        const auto src_channels = static_cast<uint32_t>(image.num_channels);

        if (width == 0 || height == 0 || src_channels == 0) {
            MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
                "EXR: zero dimensions or channels");
            FreeEXRImage(&image);
            FreeEXRHeader(&header);
            return std::nullopt;
        }

        uint32_t out_channels = src_channels;
        F format;
        switch (src_channels) {
        case 1:
            format = F::R32F;
            break;
        case 2:
            format = F::RG32F;
            break;
        case 3:
            out_channels = 4;
            format = F::RGBA32F;
            break;
        case 4:
            format = F::RGBA32F;
            break;
        default:
            MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
                "EXR: {} channels not supported (1/2/3/4 only)", src_channels);
            FreeEXRImage(&image);
            FreeEXRHeader(&header);
            return std::nullopt;
        }

        const size_t pixel_count = static_cast<size_t>(width) * height;
        const size_t element_count = pixel_count * out_channels;

        ImageData result;
        auto& dst = result.pixels.emplace<std::vector<float>>(element_count, 0.0F);

        auto** planes = reinterpret_cast<float**>(image.images);

        for (size_t p = 0; p < pixel_count; ++p) {
            for (uint32_t c = 0; c < src_channels; ++c) {
                dst[p * out_channels + c] = planes[c][p];
            }
            if (out_channels > src_channels) {
                dst[p * out_channels + (out_channels - 1)] = 1.0F;
            }
        }

        result.width = width;
        result.height = height;
        result.channels = out_channels;
        result.format = format;

        MF_INFO(Journal::Component::IO, Journal::Context::FileIO,
            "Loaded EXR: {}x{}, {} channels{}",
            width, height, out_channels,
            (src_channels == 3) ? " [BGR→ABGR, A=1.0]" : "");

        FreeEXRImage(&image);
        FreeEXRHeader(&header);

        return result;
    }

} // namespace

[[maybe_unused]] static bool g_stb_simd_logged = []() {
#ifdef STBI_SSE2
    MF_INFO(Journal::Component::IO, Journal::Context::Init,
        "STB Image: SSE2 SIMD optimizations enabled (x64)");
#elif defined(STBI_NEON)
    MF_INFO(Journal::Component::IO, Journal::Context::Init,
        "STB Image: NEON SIMD optimizations enabled (ARM64)");
#else
    MF_INFO(Journal::Component::IO, Journal::Context::Init,
        "STB Image: No SIMD optimizations (scalar fallback)");
#endif
    return true;
}();

bool ImageData::is_consistent() const
{
    using F = Portal::Graphics::ImageFormat;

    const bool has_u8 = std::holds_alternative<std::vector<uint8_t>>(pixels);
    const bool has_u16 = std::holds_alternative<std::vector<uint16_t>>(pixels);
    const bool has_f32 = std::holds_alternative<std::vector<float>>(pixels);

    switch (format) {
    case F::R8:
    case F::RG8:
    case F::RGBA8:
    case F::BGRA8:
        return has_u8;

    case F::R16:
    case F::RG16:
    case F::RGBA16:
        return has_u16;

    case F::R16F:
    case F::RG16F:
    case F::RGBA16F:
        return has_u16;

    case F::R32F:
    case F::RG32F:
    case F::RGBA32F:
        return has_f32;

    default:
        return false;
    }
}

ImageReader::ImageReader()
    : m_is_open(false)
{
}

ImageReader::~ImageReader()
{
    close();
}

bool ImageReader::can_read(const std::string& filepath) const
{
    auto ext = std::filesystem::path(filepath).extension().string();
    if (!ext.empty() && ext[0] == '.') {
        ext = ext.substr(1);
    }

    static const std::vector<std::string> supported = {
        "png", "jpg", "jpeg", "bmp", "tga", "psd", "gif", "hdr", "pic", "pnm",
        "exr"
    };

    return std::ranges::find(supported, ext) != supported.end();
}

bool ImageReader::open(const std::string& filepath, FileReadOptions /*options*/)
{
    if (m_is_open) {
        close();
    }

    if (!can_read(filepath)) {
        m_last_error = "Unsupported image format: " + filepath;
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO, m_last_error);
        return false;
    }

    auto resolved = resolve_path(filepath);

    m_image_data = load(resolved, 4); // Force RGBA

    if (!m_image_data) {
        m_last_error = "Failed to load image data";
        return false;
    }

    m_filepath = filepath;
    m_is_open = true;

    MF_INFO(Journal::Component::IO, Journal::Context::FileIO,
        "Opened image: {} ({}x{}, {} channels)",
        filepath, m_image_data->width, m_image_data->height, m_image_data->channels);

    return true;
}

void ImageReader::close()
{
    if (m_is_open) {
        m_image_data.reset();
        m_filepath.clear();
        m_is_open = false;
    }
}

bool ImageReader::is_open() const
{
    return m_is_open;
}

std::optional<FileMetadata> ImageReader::get_metadata() const
{
    if (!m_is_open || !m_image_data) {
        return std::nullopt;
    }

    FileMetadata meta;
    meta.format = "8-bit";

    meta.attributes["width"] = m_image_data->width;
    meta.attributes["height"] = m_image_data->height;
    meta.attributes["modality"] = Kakshya::DataModality::IMAGE_COLOR;

    return meta;
}

std::vector<FileRegion> ImageReader::get_regions() const
{
    // Images don't typically have regions
    return {};
}

std::vector<Kakshya::DataVariant> ImageReader::read_all()
{
    if (!m_is_open || !m_image_data) {
        m_last_error = "No image open";
        return {};
    }

    return std::visit(
        [](const auto& vec) -> std::vector<Kakshya::DataVariant> {
            return { Kakshya::DataVariant { vec } };
        },
        m_image_data->pixels);
}

std::vector<Kakshya::DataVariant> ImageReader::read_region(const FileRegion& region)
{
    if (!m_is_open || !m_image_data) {
        m_last_error = "No image open";
        return {};
    }

    if (region.start_coordinates.size() < 2 || region.end_coordinates.size() < 2) {
        m_last_error = "Invalid region coordinates for image";
        return {};
    }

    auto x_start = static_cast<uint32_t>(region.start_coordinates[0]);
    auto y_start = static_cast<uint32_t>(region.start_coordinates[1]);
    auto x_end = static_cast<uint32_t>(region.end_coordinates[0]);
    auto y_end = static_cast<uint32_t>(region.end_coordinates[1]);

    if (x_end > m_image_data->width || y_end > m_image_data->height) {
        m_last_error = "Region out of bounds";
        return {};
    }

    uint32_t region_width = x_end - x_start;
    uint32_t region_height = y_end - y_start;

    const size_t bytes_per_pixel = m_image_data->byte_size() / m_image_data->element_count() * m_image_data->channels;
    const size_t bytes_per_elem = m_image_data->byte_size() / m_image_data->element_count();
    const size_t pixel_stride_bytes = bytes_per_elem * m_image_data->channels;

    std::vector<uint8_t> region_data(
        static_cast<size_t>(region_width) * region_height * pixel_stride_bytes);

    const auto* src = static_cast<const uint8_t*>(m_image_data->data());

    for (uint32_t y = 0; y < region_height; ++y) {
        size_t src_offset = (static_cast<size_t>((y_start + y) * m_image_data->width + x_start)) * pixel_stride_bytes;
        size_t dst_offset = static_cast<size_t>(y * region_width) * pixel_stride_bytes;
        size_t row_size = static_cast<size_t>(region_width) * pixel_stride_bytes;
        std::memcpy(
            region_data.data() + dst_offset,
            src + src_offset,
            row_size);
    }

    return { region_data };
}

std::shared_ptr<Kakshya::SignalSourceContainer> ImageReader::create_container()
{
    m_last_error = "Images use direct GPU texture creation, not containers";
    return nullptr;
}

bool ImageReader::load_into_container(std::shared_ptr<Kakshya::SignalSourceContainer> /*container*/)
{
    m_last_error = "Images cannot be loaded into SignalSourceContainer";
    return false;
}

std::vector<uint64_t> ImageReader::get_read_position() const
{
    return { 0, 0 };
}

bool ImageReader::seek(const std::vector<uint64_t>& /*position*/)
{
    return true;
}

std::vector<std::string> ImageReader::get_supported_extensions() const
{
    return { "png", "jpg", "jpeg", "bmp", "tga", "psd", "gif", "hdr", "pic", "pnm",
        "exr" };
}

std::type_index ImageReader::get_data_type() const
{
    return typeid(std::vector<uint8_t>);
}

std::type_index ImageReader::get_container_type() const
{
    return typeid(void);
}

std::string ImageReader::get_last_error() const
{
    return m_last_error;
}

bool ImageReader::supports_streaming() const
{
    return false;
}

uint64_t ImageReader::get_preferred_chunk_size() const
{
    return 0;
}

size_t ImageReader::get_num_dimensions() const
{
    return 2;
}

std::vector<uint64_t> ImageReader::get_dimension_sizes() const
{
    if (!m_is_open || !m_image_data) {
        return {};
    }
    return { m_image_data->width, m_image_data->height };
}

//==============================================================================
// Static Utility Methods
//==============================================================================

std::optional<ImageData> ImageReader::load(const std::filesystem::path& path, int desired_channels)
{
    if (!std::filesystem::exists(path)) {
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "Image file not found: {}", path.string());
        return std::nullopt;
    }

    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "Failed to open image file: {}", path.string());
        return std::nullopt;
    }

    std::streamsize file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<unsigned char> file_buffer(file_size);
    if (!file.read(reinterpret_cast<char*>(file_buffer.data()), file_size)) {
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "Failed to read image file: {}", path.string());
        return std::nullopt;
    }
    file.close();

    if (extension_of(path) == "exr") {
        (void)desired_channels;
        return load_exr_from_memory(file_buffer.data(),
            static_cast<size_t>(file_size));
    }

    int width {}, height {}, channels {};

    if (desired_channels == 0) {
        stbi_info_from_memory(file_buffer.data(), static_cast<int>(file_buffer.size()),
            &width, &height, &channels);
        if (channels == 3) {
            desired_channels = 4;
        }
    }

    unsigned char* pixels = stbi_load_from_memory(
        file_buffer.data(),
        static_cast<int>(file_buffer.size()),
        &width, &height, &channels,
        desired_channels);

    if (!pixels) {
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "Failed to decode image: {} - {}",
            path.string(), stbi_failure_reason());
        return std::nullopt;
    }

    int result_channels = (desired_channels != 0) ? desired_channels : channels;

    MF_INFO(Journal::Component::IO, Journal::Context::FileIO,
        "Loaded image: {} ({}x{}, {} channels{})",
        path.filename().string(), width, height, result_channels,
        (channels == 3 && result_channels == 4) ? " [RGB→RGBA]" : "");

    ImageData result;
    auto& buf = result.pixels.emplace<std::vector<uint8_t>>();
    size_t data_size = static_cast<size_t>(width) * height * result_channels;
    buf.resize(data_size);
    std::memcpy(buf.data(), pixels, data_size);

    result.width = width;
    result.height = height;
    result.channels = result_channels;

    switch (result_channels) {
    case 1:
        result.format = Portal::Graphics::ImageFormat::R8;
        break;
    case 2:
        result.format = Portal::Graphics::ImageFormat::RG8;
        break;
    case 4:
        result.format = Portal::Graphics::ImageFormat::RGBA8;
        break;
    default:
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "Unsupported channel count: {}", result_channels);
        stbi_image_free(pixels);
        return std::nullopt;
    }

    stbi_image_free(pixels);
    return result;
}

std::optional<ImageData> ImageReader::load(const std::string& path, int desired_channels)
{
    return load(std::filesystem::path(path), desired_channels);
}

std::optional<ImageData> ImageReader::load_from_memory(const void* data, size_t size)
{
    if (!data || size == 0) {
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "Invalid memory buffer for image loading");
        return std::nullopt;
    }

    const auto* bytes = static_cast<const unsigned char*>(data);

    if (size >= 4
        && bytes[0] == 0x76 && bytes[1] == 0x2F
        && bytes[2] == 0x31 && bytes[3] == 0x01) {
        return load_exr_from_memory(bytes, size);
    }

    int width {}, height {}, channels {};

    stbi_info_from_memory(bytes, static_cast<int>(size),
        &width, &height, &channels);

    int load_as = (channels == 3) ? 4 : 0;

    unsigned char* pixels = stbi_load_from_memory(
        bytes,
        static_cast<int>(size),
        &width, &height, &channels,
        load_as);

    if (!pixels) {
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "Failed to decode image from memory: {}",
            stbi_failure_reason());
        return std::nullopt;
    }

    int result_channels = (load_as != 0) ? load_as : channels;

    MF_INFO(Journal::Component::IO, Journal::Context::FileIO,
        "Loaded image from memory ({}x{}, {} channels{})",
        width, height, result_channels,
        (channels == 3 && result_channels == 4) ? " [RGB→RGBA]" : "");

    ImageData result;
    auto& buf = result.pixels.emplace<std::vector<uint8_t>>();
    size_t data_size = static_cast<size_t>(width) * height * result_channels;
    buf.resize(data_size);
    std::memcpy(buf.data(), pixels, data_size);

    result.width = width;
    result.height = height;
    result.channels = result_channels;

    switch (result_channels) {
    case 1:
        result.format = Portal::Graphics::ImageFormat::R8;
        break;
    case 2:
        result.format = Portal::Graphics::ImageFormat::RG8;
        break;
    case 4:
        result.format = Portal::Graphics::ImageFormat::RGBA8;
        break;
    default:
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "Unsupported channel count: {}", result_channels);
        stbi_image_free(pixels);
        return std::nullopt;
    }

    stbi_image_free(pixels);
    return result;
}

std::shared_ptr<Core::VKImage> ImageReader::load_texture(const std::string& path)
{
    auto image_data = load(path, 4);
    if (!image_data) {
        return nullptr;
    }

    auto& mgr = Portal::Graphics::get_texture_manager();
    auto texture = mgr.create_2d(
        image_data->width,
        image_data->height,
        image_data->format,
        image_data->data());

    if (texture) {
        MF_INFO(Journal::Component::IO, Journal::Context::FileIO,
            "Created GPU texture from image: {}", path);
    }

    return texture;
}

std::optional<ImageData> ImageReader::get_image_data() const
{
    return m_image_data;
}

std::shared_ptr<Buffers::TextureBuffer> ImageReader::create_texture_buffer()
{
    if (!m_is_open || !m_image_data) {
        m_last_error = "No image open";
        return nullptr;
    }

    auto tex_buffer = std::make_shared<Buffers::TextureBuffer>(
        m_image_data->width,
        m_image_data->height,
        m_image_data->format,
        m_image_data->data());

    MF_INFO(Journal::Component::IO, Journal::Context::FileIO,
        "Created TextureBuffer from image: {}x{} ({} bytes)",
        m_image_data->width, m_image_data->height, tex_buffer->get_size_bytes());

    return tex_buffer;
}

bool ImageReader::load_into_buffer(const std::shared_ptr<Buffers::VKBuffer>& buffer)
{
    if (!m_is_open || !m_image_data) {
        m_last_error = "No image open";
        return false;
    }

    if (!buffer || !buffer->is_initialized()) {
        m_last_error = "Invalid or uninitialized buffer";
        return false;
    }

    size_t required_size = m_image_data->byte_size();
    if (buffer->get_size_bytes() < required_size) {
        m_last_error = "Buffer too small for image data";
        return false;
    }

    Buffers::upload_to_gpu(
        m_image_data->data(),
        required_size,
        buffer);

    MF_INFO(Journal::Component::IO, Journal::Context::FileIO,
        "Loaded image into VKBuffer: {}x{} ({} bytes)",
        m_image_data->width, m_image_data->height, required_size);

    return true;
}

} // namespace MayaFlux::IO
