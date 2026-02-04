#pragma once

#include "MayaFlux/IO/TextFileWriter.hpp"
#include "Sink.hpp"

namespace MayaFlux::Journal {

class MAYAFLUX_API FileSink : public Sink {
public:
    explicit FileSink(const std::string& filepath,
        size_t max_file_size_mb = 10)
        : writer_(std::make_unique<IO::TextFileWriter>())
    {
        writer_->set_max_file_size(max_file_size_mb * 1024 * 1024);
        writer_->open(filepath,
            IO::FileWriteOptions::CREATE | IO::FileWriteOptions::APPEND);
    }

    ~FileSink() override
    {
        if (writer_) {
            writer_->close();
        }
    }

    void write(const JournalEntry& entry) override
    {
        if (!writer_ || !writer_->is_open())
            return;

        auto line = format_entry(entry);
        writer_->write_line(line);
    }

    void write(const RealtimeEntry& entry) override
    {
        if (!writer_ || !writer_->is_open())
            return;

        auto line = format_entry(entry);
        writer_->write_line(line);
    }

    void flush() override
    {
        if (writer_) {
            writer_->flush();
        }
    }

    [[nodiscard]] bool is_available() const override
    {
        return writer_ && writer_->is_open();
    }

private:
    template <typename Entry>
    std::string format_entry(const Entry& entry)
    {
        std::ostringstream oss;

        auto time = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(time);
        oss << "[" << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") << "]";

        oss << "[" << Reflect::enum_to_string(entry.severity) << "]"
            << "[" << Reflect::enum_to_string(entry.component) << "]"
            << "[" << Reflect::enum_to_string(entry.context) << "] ";

        if constexpr (std::is_same_v<Entry, JournalEntry>) {
            oss << entry.message;
            if (entry.location.file_name() != nullptr) {
                oss << " (" << entry.location.file_name()
                    << ":" << entry.location.line() << ")";
            }
        } else {
            oss << entry.message;
            if (entry.file_name != nullptr) {
                oss << " (" << entry.file_name
                    << ":" << entry.line << ")";
            }
        }

        return oss.str();
    }

    std::unique_ptr<IO::TextFileWriter> writer_;
};

} // namespace MayaFlux::Journal
