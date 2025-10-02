#include "TextFileWriter.hpp"

#include <filesystem>

namespace MayaFlux::IO {

TextFileWriter::TextFileWriter() = default;

TextFileWriter::~TextFileWriter()
{
    close();
}

bool TextFileWriter::can_write(const std::string& filepath) const
{
    std::filesystem::path path(filepath);

    auto parent = path.parent_path();
    if (parent.empty()) {
        parent = std::filesystem::current_path();
    }

    return std::filesystem::exists(parent) && !std::filesystem::is_directory(path);
}

bool TextFileWriter::open(const std::string& filepath, FileWriteOptions options)
{
    std::lock_guard lock(m_mutex);

    if (m_is_open) {
        close();
    }

    m_filepath = filepath;
    m_options = options;

    std::ios_base::openmode mode = std::ios_base::out;

    if ((options & FileWriteOptions::APPEND) != FileWriteOptions::NONE) {
        mode |= std::ios_base::app;
    }

    if ((options & FileWriteOptions::TRUNCATE) != FileWriteOptions::NONE) {
        mode |= std::ios_base::trunc;
    }

    try {
        std::filesystem::path path(filepath);
        if (auto parent = path.parent_path(); !parent.empty()) {
            std::filesystem::create_directories(parent);
        }
    } catch (const std::filesystem::filesystem_error& e) {
        m_last_error = std::string("Failed to create directories: ") + e.what();
        return false;
    }

    m_file.open(filepath, mode);

    if (!m_file.is_open()) {
        m_last_error = "Failed to open file: " + filepath;
        return false;
    }

    m_is_open = true;
    m_bytes_written = 0;

    if ((options & FileWriteOptions::APPEND) != FileWriteOptions::NONE) {
        try {
            m_bytes_written = std::filesystem::file_size(filepath);
        } catch (...) {
            m_bytes_written = 0;
        }
    }

    return true;
}

void TextFileWriter::close()
{
    std::lock_guard lock(m_mutex);

    if (m_file.is_open()) {
        m_file.flush();
        m_file.close();
    }

    m_is_open = false;
}

bool TextFileWriter::is_open() const
{
    std::lock_guard lock(m_mutex);
    return m_is_open && m_file.is_open();
}

bool TextFileWriter::write_bytes(const void* data, size_t size)
{
    std::lock_guard lock(m_mutex);

    if (!m_is_open || !m_file.is_open()) {
        m_last_error = "File not open";
        return false;
    }

    if (should_rotate()) {
        if (!rotate_file()) {
            return false;
        }
    }

    m_file.write(static_cast<const char*>(data), size);

    if (!m_file.good()) {
        m_last_error = "Write failed";
        return false;
    }

    m_bytes_written += size;

    if ((m_options & FileWriteOptions::SYNC) != FileWriteOptions::NONE) {
        m_file.flush();
    }

    return true;
}

bool TextFileWriter::write_string(std::string_view str)
{
    return write_bytes(str.data(), str.size());
}

bool TextFileWriter::write_line(std::string_view line)
{
    if (!write_string(line)) {
        return false;
    }
    return write_bytes("\n", 1);
}

bool TextFileWriter::flush()
{
    std::lock_guard lock(m_mutex);

    if (!m_is_open || !m_file.is_open()) {
        return false;
    }

    m_file.flush();
    return m_file.good();
}

size_t TextFileWriter::get_write_position() const
{
    std::lock_guard lock(m_mutex);
    return m_bytes_written;
}

std::string TextFileWriter::get_last_error() const
{
    std::lock_guard lock(m_mutex);
    return m_last_error;
}

void TextFileWriter::set_max_file_size(size_t max_bytes)
{
    std::lock_guard lock(m_mutex);
    m_max_file_size = max_bytes;
}

size_t TextFileWriter::get_file_size() const
{
    std::lock_guard lock(m_mutex);
    return m_bytes_written;
}

bool TextFileWriter::should_rotate() const
{
    return m_max_file_size > 0 && m_bytes_written >= m_max_file_size;
}

bool TextFileWriter::rotate_file()
{
    m_file.flush();
    m_file.close();

    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);

    std::ostringstream oss;
    oss << m_filepath << "."
        << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S");

    std::string backup_path = oss.str();

    try {
        std::filesystem::rename(m_filepath, backup_path);
    } catch (const std::filesystem::filesystem_error& e) {
        m_last_error = std::string("Failed to rotate file: ") + e.what();
        return false;
    }

    std::ios_base::openmode mode = std::ios_base::out | std::ios_base::trunc;
    m_file.open(m_filepath, mode);

    if (!m_file.is_open()) {
        m_last_error = "Failed to reopen file after rotation";
        return false;
    }

    m_bytes_written = 0;
    return true;
}

} // namespace MayaFlux::IO
