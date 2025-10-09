#pragma once

#include "MayaFlux/Kakshya/Region.hpp"

#include "filesystem"
#include "typeindex"

namespace MayaFlux::Kakshya {

class SignalSourceContainer;
struct RegionGroup;
}

namespace MayaFlux::IO {

/**
 * @struct FileMetadata
 * @brief Generic metadata structure for any file type.
 *
 * Stores both standard and type-specific metadata for files, including format,
 * MIME type, size, timestamps, and arbitrary key-value attributes.
 */
struct FileMetadata {
    std::string format; ///< File format identifier (e.g., "wav", "mp3", "hdf5")
    std::string mime_type; ///< MIME type if applicable (e.g., "audio/wav")
    uint64_t file_size = 0; ///< Size in bytes
    std::chrono::system_clock::time_point creation_time; ///< File creation time
    std::chrono::system_clock::time_point modification_time; ///< Last modification time

    /// Type-specific metadata stored as key-value pairs (e.g., sample rate, channels)
    std::unordered_map<std::string, std::any> attributes;

    /**
     * @brief Get a typed attribute value by key.
     * @tparam T Expected type.
     * @param key Attribute key.
     * @return Optional value if present and convertible.
     */
    template <typename T>
    std::optional<T> get_attribute(const std::string& key) const
    {
        auto it = attributes.find(key);
        if (it != attributes.end()) {
            try {
                return safe_any_cast<T>(it->second);
            } catch (const std::bad_any_cast&) {
                return std::nullopt;
            }
        }
        return std::nullopt;
    }
};

/**
 * @enum FileReadOptions
 * @brief Generic options for file reading behavior.
 *
 * Bitmask flags to control file reading, metadata extraction, streaming, and more.
 */
enum class FileReadOptions : uint32_t {
    NONE = 0, ///< No special options
    EXTRACT_METADATA = 1 << 0, ///< Extract file metadata
    EXTRACT_REGIONS = 1 << 1, ///< Extract semantic regions (format-specific)
    LAZY_LOAD = 1 << 2, ///< Don't load all data immediately
    STREAMING = 1 << 3, ///< Enable streaming mode
    HIGH_PRECISION = 1 << 4, ///< Use highest precision available
    VERIFY_INTEGRITY = 1 << 5, ///< Verify file integrity/checksums
    DECOMPRESS = 1 << 6, ///< Decompress if compressed
    PARSE_STRUCTURE = 1 << 7, ///< Parse internal structure
    ALL = 0xFFFFFFFF ///< All options enabled
};

inline FileReadOptions operator|(FileReadOptions a, FileReadOptions b)
{
    return static_cast<FileReadOptions>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline FileReadOptions operator&(FileReadOptions a, FileReadOptions b)
{
    return static_cast<FileReadOptions>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

/**
 * @struct FileRegion
 * @brief Generic region descriptor for any file type.
 *
 * Describes a logical region or segment within a file, such as a cue, marker,
 * chapter, scene, or data block. Used for both audio/video and scientific data.
 */
struct FileRegion {
    std::string type; ///< Region type identifier (e.g., "cue", "scene", "block")
    std::string name; ///< Human-readable name for the region
    std::vector<uint64_t> start_coordinates; ///< N-dimensional start position (e.g., frame, x, y)
    std::vector<uint64_t> end_coordinates; ///< N-dimensional end position (inclusive)
    std::unordered_map<std::string, std::any> attributes; ///< Region-specific metadata

    /**
     * @brief Convert this FileRegion to a Region for use in processing.
     * @return Region with equivalent coordinates and attributes.
     */
    Kakshya::Region to_region() const;
};

/**
 * @class FileReader
 * @brief Abstract interface for reading various file formats into containers.
 *
 * FileReader provides a type-agnostic interface for loading file data into
 * the MayaFlux container system. It supports a wide range of structured data:
 * - Audio files (WAV, MP3, FLAC, etc.)
 * - Video files (MP4, AVI, MOV, etc.)
 * - Image sequences or multi-dimensional image data
 * - Scientific data formats (HDF5, NetCDF, etc.)
 * - Custom binary formats
 * - Text-based structured data (JSON, XML, CSV as regions)
 *
 * The interface is designed for flexibility, supporting region extraction,
 * metadata parsing, streaming, and container creation for any data type.
 */
class FileReader {
public:
    virtual ~FileReader() = default;

    /**
     * @brief Check if a file can be read by this reader.
     * @param filepath Path to the file.
     * @return true if the file format is supported.
     */
    [[nodiscard]] virtual bool can_read(const std::string& filepath) const = 0;

    /**
     * @brief Open a file for reading.
     * @param filepath Path to the file.
     * @param options Reading options (see FileReadOptions).
     * @return true if file was successfully opened.
     */
    virtual bool open(const std::string& filepath, FileReadOptions options = FileReadOptions::ALL) = 0;

    /**
     * @brief Close the currently open file.
     */
    virtual void close() = 0;

    /**
     * @brief Check if a file is currently open.
     * @return true if a file is open.
     */
    [[nodiscard]] virtual bool is_open() const = 0;

    /**
     * @brief Get metadata from the open file.
     * @return File metadata or nullopt if no file is open.
     */
    [[nodiscard]] virtual std::optional<FileMetadata> get_metadata() const = 0;

    /**
     * @brief Get semantic regions from the file.
     * @return Vector of regions found in the file.
     *
     * Regions are format-specific:
     * - Audio: cues, markers, loops, chapters
     * - Video: scenes, chapters, keyframes
     * - Images: layers, selections, annotations
     * - Data: chunks, blocks, datasets
     */
    [[nodiscard]] virtual std::vector<FileRegion> get_regions() const = 0;

    /**
     * @brief Read all data from the file into memory.
     * @return DataVariant vector containing the file data.
     */
    virtual std::vector<Kakshya::DataVariant> read_all() = 0;

    /**
     * @brief Read a specific region of data.
     * @param region Region descriptor.
     * @return DataVariant vector containing the requested data.
     */
    virtual std::vector<Kakshya::DataVariant> read_region(const FileRegion& region) = 0;

    /**
     * @brief Create and initialize a container from the file.
     * @return Initialized container appropriate for the file type.
     *
     * The specific container type returned depends on the file format:
     * - Audio files -> SoundFileContainer
     * - Video files -> VideoContainer (future)
     * - Image files -> ImageContainer (future)
     * - Data files -> DataContainer variants
     */
    virtual std::shared_ptr<Kakshya::SignalSourceContainer> create_container() = 0;

    /**
     * @brief Load file data into an existing container.
     * @param container Target container (must be compatible type).
     * @return true if successful.
     */
    virtual bool load_into_container(std::shared_ptr<Kakshya::SignalSourceContainer> container) = 0;

    /**
     * @brief Get current read position in primary dimension.
     * @return Current position (interpretation is format-specific).
     */
    [[nodiscard]] virtual std::vector<uint64_t> get_read_position() const = 0;

    /**
     * @brief Seek to a specific position in the file.
     * @param position Target position in N-dimensional space.
     * @return true if seek was successful.
     */
    virtual bool seek(const std::vector<uint64_t>& position) = 0;

    /**
     * @brief Get supported file extensions for this reader.
     * @return Vector of supported extensions (without dots).
     */
    [[nodiscard]] virtual std::vector<std::string> get_supported_extensions() const = 0;

    /**
     * @brief Get the data type this reader produces.
     * @return Type info for the data variant content.
     */
    [[nodiscard]] virtual std::type_index get_data_type() const = 0;

    /**
     * @brief Get the container type this reader creates.
     * @return Type info for the container type.
     */
    [[nodiscard]] virtual std::type_index get_container_type() const = 0;

    /**
     * @brief Get the last error message.
     * @return Error string or empty if no error.
     */
    [[nodiscard]] virtual std::string get_last_error() const = 0;

    /**
     * @brief Check if streaming is supported for the current file.
     * @return true if file can be streamed.
     */
    [[nodiscard]] virtual bool supports_streaming() const = 0;

    /**
     * @brief Get the preferred chunk size for streaming.
     * @return Chunk size in primary dimension units.
     */
    [[nodiscard]] virtual uint64_t get_preferred_chunk_size() const = 0;

    /**
     * @brief Get the dimensionality of the file data.
     * @return Number of dimensions.
     */
    [[nodiscard]] virtual size_t get_num_dimensions() const = 0;

    /**
     * @brief Get size of each dimension in the file data.
     * @return Vector of dimension sizes.
     */
    [[nodiscard]] virtual std::vector<uint64_t> get_dimension_sizes() const = 0;

protected:
    /**
     * @brief Convert file regions to region groups.
     * @param regions Vector of file regions.
     * @return Region groups organized by type.
     *
     * Groups regions by their type field, producing a map from type to RegionGroup.
     */
    static std::unordered_map<std::string, Kakshya::RegionGroup>
    regions_to_groups(const std::vector<FileRegion>& regions);
};

// Type alias for factory function
using FileReaderFactory = std::function<std::unique_ptr<FileReader>()>;

/**
 * @class FileReaderRegistry
 * @brief Registry for file reader implementations.
 *
 * Allows registration of different FileReader implementations
 * and automatic selection based on file extension or content.
 */
class FileReaderRegistry {
public:
    /**
     * @brief Get the singleton instance of the registry.
     */
    static FileReaderRegistry& instance()
    {
        static FileReaderRegistry registry;
        return registry;
    }

    /**
     * @brief Register a file reader factory for one or more extensions.
     * @param extensions Supported file extensions (without dots).
     * @param factory Factory function to create reader.
     */
    void register_reader(const std::vector<std::string>& extensions, const FileReaderFactory& factory)
    {
        for (const auto& ext : extensions) {
            m_factories[ext] = factory;
        }
    }

    /**
     * @brief Create appropriate reader for a file based on extension.
     * @param filepath Path to file.
     * @return Reader instance or nullptr if no suitable reader.
     */
    std::unique_ptr<FileReader> create_reader(const std::string& filepath) const
    {
        auto ext = std::filesystem::path(filepath).extension().string();
        if (!ext.empty() && ext[0] == '.') {
            ext = ext.substr(1);
        }

        auto it = m_factories.find(ext);
        if (it != m_factories.end()) {
            return it->second();
        }
        return nullptr;
    }

private:
    std::unordered_map<std::string, FileReaderFactory> m_factories;
};

} // namespace MayaFlux::Kakshya
