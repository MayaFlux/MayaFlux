#pragma once


#ifdef MAYAFLUX_PLATFORM_WINDOWS
#include <windows.h>

#ifdef ERROR
#undef ERROR
#endif // ERROR

#endif // MAYAFLUX_PLATFORM_WINDOWS

namespace MayaFlux::Journal::AnsiColors {
// Reset
static constexpr std::string_view Reset = "\033[0m";

// Regular colors
static constexpr std::string_view Black = "\033[30m";
static constexpr std::string_view Red = "\033[31m";
static constexpr std::string_view Green = "\033[32m";
static constexpr std::string_view Yellow = "\033[33m";
static constexpr std::string_view Blue = "\033[34m";
static constexpr std::string_view Magenta = "\033[35m";
static constexpr std::string_view Cyan = "\033[36m";
static constexpr std::string_view White = "\033[37m";

// Bright colors
static constexpr std::string_view BrightRed = "\033[91m";
static constexpr std::string_view BrightGreen = "\033[92m";
static constexpr std::string_view BrightYellow = "\033[93m";
static constexpr std::string_view BrightBlue = "\033[94m";
static constexpr std::string_view BrightMagenta = "\033[95m";
static constexpr std::string_view BrightCyan = "\033[96m";

// Background colors
static constexpr std::string_view BgRed = "\033[41m";
static constexpr std::string_view BgGreen = "\033[42m";
static constexpr std::string_view BgYellow = "\033[43m";

static bool initialize_console_colors()
{
#ifdef MAYAFLUX_PLATFORM_WINDOWS
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            return SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
    }
    return false;
#else
    return true;
#endif
}

}
