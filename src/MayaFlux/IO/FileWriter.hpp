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
