#pragma once

#include "Sink.hpp"

#include "Ansi.hpp"

namespace MayaFlux::Journal {

class MAYAFLUX_API ConsoleSink : public Sink {
private:
    bool m_colors_enabled;

public:
    ConsoleSink()
    {
        m_colors_enabled = AnsiColors::initialize_console_colors();
    }

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
        if (m_colors_enabled) {
            switch (entry.severity) {
            case Severity::TRACE:
                os << AnsiColors::Cyan;
                break;
            case Severity::DEBUG:
                os << AnsiColors::Blue;
                break;
            case Severity::INFO:
                os << AnsiColors::Green;
                break;
            case Severity::WARN:
                os << AnsiColors::Yellow;
                break;
            case Severity::ERROR:
                os << AnsiColors::BrightRed;
                break;
            case Severity::FATAL:
                os << AnsiColors::BgRed << AnsiColors::White;
                break;
            case Severity::NONE:
                os << AnsiColors::Reset;
                break;
            default:
                os << AnsiColors::Reset;
                break;
            }
        }

        os << "[" << Utils::enum_to_string(entry.severity) << "]" << AnsiColors::Reset;

        os << AnsiColors::Magenta << "[" << Utils::enum_to_string(entry.component) << "]" << AnsiColors::Reset;

        os << AnsiColors::Cyan << "[" << Utils::enum_to_string(entry.context) << "]" << AnsiColors::Reset << " ";

        if constexpr (std::is_same_v<Entry, JournalEntry>) {
            os << entry.message;
            if (entry.location.file_name() != nullptr) {
                os << AnsiColors::BrightBlue << " (" << entry.location.file_name()
                   << ":" << entry.location.line() << ")" << AnsiColors::Reset;
            }
        } else {
            os << entry.message;
            if (entry.file_name != nullptr) {
                os << AnsiColors::BrightBlue << " (" << entry.file_name
                   << ":" << entry.line << ")" << AnsiColors::Reset;
            }
        }

        if (m_colors_enabled) {
            os << AnsiColors::Reset;
        }

        os << std::endl;
    }

    std::mutex m_mutex;

    void feed_severity(std::ostream& os, Severity severity) const
    {
        switch (severity) {
        case Severity::TRACE:
            os << AnsiColors::Cyan;
            break;
        case Severity::DEBUG:
            os << AnsiColors::Blue;
            break;
        case Severity::INFO:
            os << AnsiColors::Green;
            break;
        case Severity::WARN:
            os << AnsiColors::Yellow;
            break;
        case Severity::ERROR:
            os << AnsiColors::BrightRed;
            break;
        case Severity::FATAL:
            os << AnsiColors::BgRed << AnsiColors::White;
            break;
        case Severity::NONE:
            os << AnsiColors::Reset;
            break;
        default:
            os << AnsiColors::Reset;
            break;
        }
    }
};

} // namespace MayaFlux::Journal
