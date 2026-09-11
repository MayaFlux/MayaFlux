#pragma once

namespace MayaFlux::IO {

enum class FileWriteOptions : uint32_t {
    NONE = 0,
    APPEND = 1 << 0, ///< Append to existing file
    CREATE = 1 << 1, ///< Create if doesn't exist
    TRUNCATE = 1 << 2, ///< Truncate existing file
    SYNC = 1 << 3, ///< Sync after each write (slow but safe)
    BUFFER = 1 << 4, ///< Use internal buffering
    ALL = 0xFFFFFFFF
};

inline FileWriteOptions operator|(FileWriteOptions a, FileWriteOptions b)
{
    return static_cast<FileWriteOptions>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline FileWriteOptions operator&(FileWriteOptions a, FileWriteOptions b)
{
    return static_cast<FileWriteOptions>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

/**
 * @brief Anchor a relative output path to Config::SOURCE_DIR.
 *
 * Absolute paths are returned unchanged. Relative paths are prefixed
 * with Config::SOURCE_DIR so output files land in the project source
 * tree rather than the binary CWD (which varies by platform and IDE).
 *
 * @param filepath Path as supplied by the caller.
 * @return Resolved path string.
 */
[[nodiscard]] inline std::string resolve_write_path(const std::string& filepath)
{
    namespace fs = std::filesystem;
    auto normalized = std::string(filepath);
    std::ranges::replace(normalized, '\\', '/');

    if (fs::path(normalized).is_absolute())
        return normalized;

    return (fs::path(Config::SOURCE_DIR) / normalized).string();
}

/**
 * @brief Substitute a frame index into a numbered output pattern.
 *
 * Wraps resolve_write_path, so the same directory rules apply and a bare
 * pattern lands wherever a bare filename would.
 *
 * The pattern carries exactly one std::format replacement field, which the
 * index is formatted into: "smoke.{:04}.vdb" at frame 7 gives
 * "smoke.0007.vdb". Zero padding matters. A DCC reading a sequence matches
 * on a fixed-width numeric suffix, so an unpadded pattern sorts frame 10
 * before frame 2 and plays out of order.
 *
 * A pattern with no replacement field returns the same path every frame,
 * which silently overwrites. A pattern with more than one, or with a field
 * the index cannot format into, throws std::format_error.
 *
 * @param pattern Format string with one index field.
 * @param frame   Zero-based frame index.
 * @return Resolved absolute path.
 */
[[nodiscard]] inline std::string resolve_sequence_path(std::string_view pattern, uint64_t frame)
{
    return resolve_write_path(std::vformat(pattern, std::make_format_args(frame)));
}

/**
 * @class FileWriter
 * @brief Abstract base class for file writing operations
 *
 * Provides interface for writing various data types to files.
 * Concrete implementations handle specific formats (text, binary, audio, etc.)
 */
class FileWriter {
public:
    virtual ~FileWriter() = default;

    /**
     * @brief Check if this writer can handle the given file path
     */
    [[nodiscard]] virtual bool can_write(const std::string& filepath) const = 0;

    /**
     * @brief Open a file for writing
     * @param filepath Path to the file
     * @param options Write options (append, create, truncate, etc.)
     * @return true if successful
     */
    virtual bool open(const std::string& filepath,
        FileWriteOptions options = FileWriteOptions::CREATE | FileWriteOptions::TRUNCATE)
        = 0;

    /**
     * @brief Close the currently open file
     */
    virtual void close() = 0;

    /**
     * @brief Check if a file is currently open for writing
     */
    [[nodiscard]] virtual bool is_open() const = 0;

    /**
     * @brief Write raw bytes
     */
    virtual bool write_bytes(const void* data, size_t size) = 0;

    /**
     * @brief Write a string
     */
    virtual bool write_string(std::string_view str) = 0;

    /**
     * @brief Write a line (appends newline)
     */
    virtual bool write_line(std::string_view line) = 0;

    /**
     * @brief Flush buffered writes to disk
     */
    virtual bool flush() = 0;

    /**
     * @brief Get current write position (bytes written)
     */
    [[nodiscard]] virtual size_t get_write_position() const = 0;

    /**
     * @brief Get last error message
     */
    [[nodiscard]] virtual std::string get_last_error() const = 0;
};

} // namespace MayaFlux::IO
