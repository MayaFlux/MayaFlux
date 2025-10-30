#pragma once

#include "MayaFlux/IO/FileReader.hpp"
#include "MayaFlux/Portal/Graphics/TextureLoom.hpp"

namespace MayaFlux::IO {

/**
 * @struct ImageData
 * @brief Raw image data loaded from file
 */
struct ImageData {
    std::vector<uint8_t> pixels;
    uint32_t width;
    uint32_t height;
    uint32_t channels;
    Portal::Graphics::ImageFormat format;
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

private:
    std::string m_filepath;
    std::optional<ImageData> m_image_data;
    std::string m_last_error;
    bool m_is_open;
};

} // namespace MayaFlux::IO
