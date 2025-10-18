#pragma once

#include "JournalEntry.hpp"
#include "RealtimeEntry.hpp"

namespace MayaFlux::Journal {

/**
 * @class LogSink
 * @brief Abstract interface for log output destinations
 *
 * Allows pluggable log outputs (console, file, network, etc.)
 */
class MAYAFLUX_API Sink {
public:
    virtual ~Sink() = default;

    /**
     * @brief Write a journal entry to this sink
     */
    virtual void write(const JournalEntry& entry) = 0;

    /**
     * @brief Write a realtime entry to this sink
     */
    virtual void write(const RealtimeEntry& entry) = 0;

    /**
     * @brief Flush any buffered writes
     */
    virtual void flush() = 0;

    /**
     * @brief Check if sink is available/healthy
     */
    [[nodiscard]] virtual bool is_available() const = 0;
};

} // namespace MayaFlux::Journal
