#pragma once

#include "MayaFlux/IO/FileReader.hpp"
#include "MayaFlux/Portal/Graphics/TextureLoom.hpp"

namespace MayaFlux::Buffers {
class VKBuffer;
class TextureBuffer;
}

namespace MayaFlux::IO {

/**
 * @struct ImageData
 * @brief Raw image data loaded from file.
 *
 * Pixel storage is a variant over uint8, uint16, and float, chosen by the
 * loader based on the source format. 8-bit formats (PNG, JPG, BMP, TGA)
 * populate the uint8 variant. 16-bit PNG populates the uint16 variant.
 * Floating-point formats (EXR, HDR) populate the float variant.
 *
 * The declared ImageFormat must match the active variant. Accessors below
 * enforce this; direct member access is permitted but callers are
 * responsible for consistency.
 */
struct ImageData {
    using PixelStorage = std::variant<
        std::vector<uint8_t>,
        std::vector<uint16_t>,
        std::vector<float>>;

    PixelStorage pixels;
    uint32_t width { 0 };
    uint32_t height { 0 };
    uint32_t channels { 0 };
    Portal::Graphics::ImageFormat format { Portal::Graphics::ImageFormat::RGBA8 };

    /**
     * @brief Total byte size of pixel storage, dispatched on variant.
     */
    [[nodiscard]] size_t byte_size() const
    {
        return std::visit(
            [](const auto& vec) { return vec.size() * sizeof(typename std::decay_t<decltype(vec)>::value_type); },
            pixels);
    }

    /**
     * @brief Raw data pointer, dispatched on variant. For upload paths.
     */
    [[nodiscard]] const void* data() const
    {
        return std::visit(
            [](const auto& vec) -> const void* { return vec.data(); },
            pixels);
    }

    /**
     * @brief Number of pixel elements (not bytes), dispatched on variant.
     */
    [[nodiscard]] size_t element_count() const
    {
        return std::visit(
            [](const auto& vec) { return vec.size(); },
            pixels);
    }

    /**
     * @brief Check that the active variant matches the declared format.
     *
     * Callers producing ImageData should invoke this to validate before
     * handing the data to downstream consumers.
     */
    [[nodiscard]] bool is_consistent() const;

    /**
     * @brief Typed accessors. Return nullptr if variant does not match.
     */
    [[nodiscard]] const std::vector<uint8_t>* as_uint8() const { return std::get_if<std::vector<uint8_t>>(&pixels); }
    [[nodiscard]] const std::vector<uint16_t>* as_uint16() const { return std::get_if<std::vector<uint16_t>>(&pixels); }
    [[nodiscard]] const std::vector<float>* as_float() const { return std::get_if<std::vector<float>>(&pixels); }

    [[nodiscard]] std::vector<uint8_t>* as_uint8() { return std::get_if<std::vector<uint8_t>>(&pixels); }
    [[nodiscard]] std::vector<uint16_t>* as_uint16() { return std::get_if<std::vector<uint16_t>>(&pixels); }
    [[nodiscard]] std::vector<float>* as_float() { return std::get_if<std::vector<float>>(&pixels); }
};

/**
 * @class ImageReader
 * @brief File reader for image formats (PNG, JPG, BMP, TGA, etc.)
 *
 * Uses STB Image library for decoding. Supports:
 * - PNG, JPG, BMP, TGA, PSD, GIF, HDR, PIC, PNM
 * - Automatic format detection
 * - Channel conversion (force RGBA, etc.)
 * - Direct GPU texture creation
 *
 * Implements the FileReader interface for consistency with other readers.
 */
class MAYAFLUX_API ImageReader : public FileReader {
public:
    ImageReader();
    ~ImageReader() override;

    // FileReader interface implementation
    [[nodiscard]] bool can_read(const std::string& filepath) const override;
    bool open(const std::string& filepath, FileReadOptions options = FileReadOptions::ALL) override;
    void close() override;
    [[nodiscard]] bool is_open() const override;
    [[nodiscard]] std::optional<FileMetadata> get_metadata() const override;
    [[nodiscard]] std::vector<FileRegion> get_regions() const override;
    std::vector<Kakshya::DataVariant> read_all() override;
    std::vector<Kakshya::DataVariant> read_region(const FileRegion& region) override;
    std::shared_ptr<Kakshya::SignalSourceContainer> create_container() override;
    bool load_into_container(std::shared_ptr<Kakshya::SignalSourceContainer> container) override;
    [[nodiscard]] std::vector<uint64_t> get_read_position() const override;
    bool seek(const std::vector<uint64_t>& position) override;
    [[nodiscard]] std::vector<std::string> get_supported_extensions() const override;
    [[nodiscard]] std::type_index get_data_type() const override;
    [[nodiscard]] std::type_index get_container_type() const override;
    [[nodiscard]] std::string get_last_error() const override;
    [[nodiscard]] bool supports_streaming() const override;
    [[nodiscard]] uint64_t get_preferred_chunk_size() const override;
    [[nodiscard]] size_t get_num_dimensions() const override;
    [[nodiscard]] std::vector<uint64_t> get_dimension_sizes() const override;

    /**
     * @brief Load image from memory (static utility)
     * @param data Pointer to image data in memory
     * @param size Size of the image data in bytes
     * @return Image data or nullopt on failure
     */
    static std::optional<ImageData> load_from_memory(const void* data, size_t size);

    /**
     * @brief Load image from file (static utility)
     * @param path Image file path
     * @param desired_channels Force channel count (0 = keep original, 4 = RGBA)
     * @return Image data or nullopt on failure
     */
    static std::optional<ImageData> load(
        const std::string& path, int desired_channels = 4);

    /**
     * @brief Load image from file (static utility)
     * @param path Image file path
     * @param desired_channels Force channel count (0 = keep original, 4 = RGBA)
     * @return Image data or nullopt on failure
     */
    static std::optional<ImageData> load(
        const std::filesystem::path& path, int desired_channels = 4);

    /**
     * @brief Load image directly into GPU texture (static utility)
     * @param path Image file path
     * @return Initialized VKImage or nullptr on failure
     */
    static std::shared_ptr<Core::VKImage> load_texture(
        const std::string& path);

    /**
     * @brief Get the loaded image data
     * @return Image data or nullopt if no file open
     */
    [[nodiscard]] std::optional<ImageData> get_image_data() const;

    /**
     * @brief Create a VKBuffer containing the loaded image pixel data
     * @return VKBuffer with pixel data ready for GPU processing
     *
     * Creates a buffer suitable for:
     * - Transfer operations (upload to VKImage)
     * - Direct compute shader processing
     * - Buffer processor chains (treat pixels as samples)
     */
    std::shared_ptr<Buffers::TextureBuffer> create_texture_buffer();

    /**
     * @brief Load image directly into an existing VKBuffer
     * @param buffer Target buffer (must be large enough)
     * @return True if load succeeded
     */
    bool load_into_buffer(const std::shared_ptr<Buffers::VKBuffer>& buffer);

private:
    std::string m_filepath;
    std::optional<ImageData> m_image_data;
    std::string m_last_error;
    bool m_is_open;
};

} // namespace MayaFlux::IO
