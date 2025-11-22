#include "ImageReader.hpp"

#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKImage.hpp"

#include "MayaFlux/Buffers/Staging/StagingUtils.hpp"
#include "MayaFlux/Buffers/Textures/TextureBuffer.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#include "stb/stb_image.h"

#include <cstddef>
#include <fstream>

namespace MayaFlux::IO {

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
        "png", "jpg", "jpeg", "bmp", "tga", "psd", "gif", "hdr", "pic", "pnm"
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

    m_image_data = load(filepath, 4); // Force RGBA

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

    return { m_image_data->pixels };
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
    size_t region_size = static_cast<size_t>(region_width * region_height) * m_image_data->channels;
    std::vector<uint8_t> region_data(region_size);

    for (uint32_t y = 0; y < region_height; ++y) {
        size_t src_offset = (static_cast<size_t>((y_start + y) * m_image_data->width + x_start)) * m_image_data->channels;
        size_t dst_offset = static_cast<size_t>(y * region_width) * m_image_data->channels;
        size_t row_size = static_cast<size_t>(region_width) * m_image_data->channels;
        std::memcpy(
            region_data.data() + dst_offset,
            m_image_data->pixels.data() + src_offset,
            row_size);
    }

    return { region_data };
}

std::shared_ptr<Kakshya::SignalSourceContainer> ImageReader::create_container()
{
    // Images don't use SignalSourceContainer - they go directly to GPU
    m_last_error = "Images use direct GPU texture creation, not containers";
    return nullptr;
}

bool ImageReader::load_into_container(std::shared_ptr<Kakshya::SignalSourceContainer> /*container*/)
{
    // Not applicable for images
    m_last_error = "Images cannot be loaded into SignalSourceContainer";
    return false;
}

std::vector<uint64_t> ImageReader::get_read_position() const
{
    return { 0, 0 }; // Images don't have read position
}

bool ImageReader::seek(const std::vector<uint64_t>& /*position*/)
{
    return true; // No-op for static images
}

std::vector<std::string> ImageReader::get_supported_extensions() const
{
    return { "png", "jpg", "jpeg", "bmp", "tga", "psd", "gif", "hdr", "pic", "pnm" };
}

std::type_index ImageReader::get_data_type() const
{
    return typeid(std::vector<uint8_t>);
}

std::type_index ImageReader::get_container_type() const
{
    return typeid(void); // No container for images
}

std::string ImageReader::get_last_error() const
{
    return m_last_error;
}

bool ImageReader::supports_streaming() const
{
    return false; // Images are loaded entirely into memory
}

uint64_t ImageReader::get_preferred_chunk_size() const
{
    return 0; // Not applicable
}

size_t ImageReader::get_num_dimensions() const
{
    return 2; // width, height (channels are separate)
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

    int width {}, height {}, channels {};
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

    Portal::Graphics::ImageFormat format {};
    switch (channels) {
    case 1:
        format = Portal::Graphics::ImageFormat::R8;
        break;
    case 2:
        format = Portal::Graphics::ImageFormat::RG8;
        break;
    case 3:
        format = Portal::Graphics::ImageFormat::RGB8;
        break;
    case 4:
        format = Portal::Graphics::ImageFormat::RGBA8;
        break;
    default:
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "Unsupported channel count: {}", channels);
        stbi_image_free(pixels);
        return std::nullopt;
    }

    ImageData result;

    size_t data_size = static_cast<long>(width * height) * channels;
    result.pixels.resize(data_size);
    std::memcpy(result.pixels.data(), pixels, data_size);

    result.width = width;
    result.height = height;
    result.format = format;

    stbi_image_free(pixels);

    MF_INFO(Journal::Component::IO, Journal::Context::FileIO,
        "Loaded image: {} ({}x{}, {} channels)",
        path.filename().string(), width, height, channels);

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

    int width {}, height {}, channels {};
    unsigned char* pixels = stbi_load_from_memory(
        static_cast<const unsigned char*>(data),
        static_cast<int>(size),
        &width, &height, &channels,
        0);

    if (!pixels) {
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "Failed to decode image from memory: {}",
            stbi_failure_reason());
        return std::nullopt;
    }

    Portal::Graphics::ImageFormat format {};
    switch (channels) {
    case 1:
        format = Portal::Graphics::ImageFormat::R8;
        break;
    case 2:
        format = Portal::Graphics::ImageFormat::RG8;
        break;
    case 3:
        format = Portal::Graphics::ImageFormat::RGB8;
        break;
    case 4:
        format = Portal::Graphics::ImageFormat::RGBA8;
        break;
    default:
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "Unsupported channel count: {}", channels);
        stbi_image_free(pixels);
        return std::nullopt;
    }

    ImageData result;
    size_t data_size = static_cast<long>(width * height) * channels;
    result.pixels.resize(data_size);
    std::memcpy(result.pixels.data(), pixels, data_size);

    result.width = width;
    result.height = height;
    result.format = format;

    stbi_image_free(pixels);

    MF_INFO(Journal::Component::IO, Journal::Context::FileIO,
        "Loaded image from memory ({}x{}, {} channels)",
        width, height, channels);

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
        image_data->pixels.data());

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
        m_image_data->pixels.data());

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

    size_t required_size = m_image_data->pixels.size();
    if (buffer->get_size_bytes() < required_size) {
        m_last_error = "Buffer too small for image data";
        return false;
    }

    Buffers::upload_to_gpu(
        m_image_data->pixels.data(),
        required_size,
        buffer);

    MF_INFO(Journal::Component::IO, Journal::Context::FileIO,
        "Loaded image into VKBuffer: {}x{} ({} bytes)",
        m_image_data->width, m_image_data->height, required_size);

    return true;
}

} // namespace MayaFlux::IO
