#pragma once

#if MAYAFLUX_USE_STD_FORMAT
#include <format>
namespace MayaFlux::Journal::fmt {
using std::format;
using std::format_string;
using std::make_format_args;
using std::vformat;
}
#elif MAYAFLUX_USE_FMT
#include <fmt/core.h>
#include <fmt/format.h>
namespace MayaFlux::Journal::fmt {
using ::fmt::format;
using ::fmt::format_string;
using ::fmt::make_format_args;
using ::fmt::vformat;
}
#else
#error "No formatting library available. Either std::format or fmt is required."
#endif

namespace MayaFlux::Journal {

template <typename... Args>
using format_string = fmt::format_string<Args...>;

template <typename... Args>
inline std::string format(format_string<std::remove_cvref_t<Args>...> fmt_str, Args&&... args)
{
    return fmt::format(fmt_str, std::forward<Args>(args)...);
}

template <typename... Args>
inline std::string format_runtime(std::string_view fmt_str, Args&&... args)
{
#if MAYAFLUX_USE_STD_FORMAT
    return std::vformat(fmt_str, std::make_format_args(args...));
#elif MAYAFLUX_USE_FMT
    return fmt::vformat(fmt_str, fmt::make_format_args(args...));
#endif
}

inline std::string vformat(std::string_view fmt_str, auto fmt_args)
{
    return fmt::vformat(fmt_str, fmt_args);
}

} // namespace MayaFlux::Journal
