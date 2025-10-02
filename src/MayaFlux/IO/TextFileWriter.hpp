#pragma once

#include "FileWriter.hpp"
#include <fstream>

namespace MayaFlux::IO {

/**
 * @class TextFileWriter
 * @brief Simple text file writer for logs and text data
 *
 * Thread-safe text file writing with optional buffering.
 * Designed for log files, configuration files, etc.
 */
class TextFileWriter : public FileWriter {
public:
    TextFileWriter();
    ~TextFileWriter() override;

    bool can_write(const std::string& filepath) const override;
    bool open(const std::string& filepath, FileWriteOptions options) override;
    void close() override;
    bool is_open() const override;

    bool write_bytes(const void* data, size_t size) override;
    bool write_string(std::string_view str) override;
    bool write_line(std::string_view line) override;
    bool flush() override;

    size_t get_write_position() const override;
    std::string get_last_error() const override;

    /**
     * @brief Set maximum file size before rotation
     * @param max_bytes Maximum size in bytes (0 = no limit)
     */
    void set_max_file_size(size_t max_bytes);

    /**
     * @brief Get current file size
     */
    size_t get_file_size() const;

private:
    bool should_rotate() const;
    bool rotate_file();

    mutable std::mutex m_mutex;
    std::ofstream m_file;
    std::string m_filepath;
    FileWriteOptions m_options {};
    size_t m_bytes_written { 0 };
    size_t m_max_file_size { 0 };
    mutable std::string m_last_error;
    bool m_is_open {};
};

} // namespace MayaFlux::IO
