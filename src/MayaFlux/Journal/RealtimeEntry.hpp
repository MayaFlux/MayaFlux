#pragma once

#include "JournalEntry.hpp"

namespace MayaFlux::Journal {

/**
 * @brief Lightweight entry for lock-free ring buffer
 *
 * Must be trivially copyable for lock-free operations.
 * Stores only essential data; formatting happens on worker thread.
 */
struct RealtimeEntry {
    static constexpr size_t MAX_MESSAGE_LENGTH = 256;

    Severity severity {};
    Component component {};
    Context context {};
    char message[MAX_MESSAGE_LENGTH];
    const char* file_name {};
    uint32_t line {};
    uint32_t column {};
    std::chrono::steady_clock::time_point timestamp;

    RealtimeEntry() = default;

    RealtimeEntry(Severity sev, Component comp, Context ctx,
        std::string_view msg, std::source_location loc)
        : severity(sev)
        , component(comp)
        , context(ctx)
        , file_name(loc.file_name())
        , line(loc.line())
        , column(loc.column())
        , timestamp(std::chrono::steady_clock::now())
    {
        const size_t copy_len = std::min(msg.size(), MAX_MESSAGE_LENGTH - 1);
        std::memcpy(message, msg.data(), copy_len);
        message[copy_len] = '\0';
    }
};

static_assert(std::is_trivially_copyable_v<RealtimeEntry>,
    "RealtimeEntry must be trivially copyable");

} // namespace MayaFlux::Journal
