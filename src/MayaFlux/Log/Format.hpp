#pragma once

#if MAYAFLUX_USE_STD_FORMAT
#include <format>
namespace MayaFlux::Log::fmt {
using std::format;
using std::format_string;
using std::make_format_args;
using std::vformat;
}
#elif MAYAFLUX_USE_FMT
#include <fmt/core.h>
#include <fmt/format.h>
namespace MayaFlux::Log::fmt {
// Wrap fmt library
using ::fmt::format;
using ::fmt::format_string;
using ::fmt::make_format_args;
using ::fmt::vformat;
}
#else
#error "No formatting library available. Either std::format or fmt is required."
#endif

namespace MayaFlux::Log {

template <typename... Args>
using format_string = fmt::format_string<Args...>;

template <typename... Args>
inline std::string format(format_string<Args...> fmt_str, Args&&... args)
{
    return fmt::format(fmt_str, std::forward<Args>(args)...);
}

inline std::string vformat(std::string_view fmt_str, auto fmt_args)
{
    return fmt::vformat(fmt_str, fmt_args);
}

} // namespace MayaFlux::Log
