#pragma once

#include "Sink.hpp"

namespace MayaFlux::Journal {

class ConsoleSink : public Sink {
public:
    void write(const JournalEntry& entry) override
    {
        std::lock_guard lock(m_mutex);
        write_to_stream(std::cout, entry);
    }

    void write(const RealtimeEntry& entry) override
    {
        std::lock_guard lock(m_mutex);
        write_to_stream(std::cout, entry);
    }

    void flush() override
    {
        std::lock_guard lock(m_mutex);
        std::cout.flush();
    }

    [[nodiscard]] bool is_available() const override
    {
        return std::cout.good();
    }

private:
    template <typename Entry>

    void write_to_stream(std::ostream& os, const Entry& entry)
    {
        os << "[" << Utils::enum_to_string(entry.severity) << "]"
           << "[" << Utils::enum_to_string(entry.component) << "]"
           << "[" << Utils::enum_to_string(entry.context) << "] ";

        if constexpr (std::is_same_v<Entry, JournalEntry>) {
            os << entry.message;
            if (entry.location.file_name() != nullptr) {
                os << " (" << entry.location.file_name()
                   << ":" << entry.location.line() << ")";
            }
        } else {
            os << entry.message;
            if (entry.file_name != nullptr) {
                os << " (" << entry.file_name
                   << ":" << entry.line << ")";
            }
        }

        os << std::endl;
    }

    std::mutex m_mutex;
};

} // namespace MayaFlux::Journal
