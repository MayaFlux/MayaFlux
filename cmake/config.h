#pragma once

// Cross-platform definitions
#ifdef MAYAFLUX_PLATFORM_WINDOWS
using u_int = unsigned int;
using u_int8_t = uint8_t;
using u_int16_t = uint16_t;
using u_int32_t = uint32_t;
using u_int64_t = uint64_t;

#ifdef MOUSE_WHEELED  
#undef MOUSE_WHEELED
#endif
#ifdef KEY_EVENT
#undef KEY_EVENT
#endif
#ifdef FOCUS_EVENT
#undef FOCUS_EVENT
#endif
#ifdef ERROR
#undef ERROR
#endif // ERROR
#define MAYAFLUX_FORCEINLINE __forceinline
#else
#define MAYAFLUX_FORCEINLINE __attribute__((always_inline))
#endif

// Unified export macro
#if defined(_WIN32) || defined(_WIN64)
#if defined(MAYAFLUX_STATIC)
#define MAYAFLUX_API
#elif defined(MAYAFLUX_EXPORTS)
#define MAYAFLUX_API __declspec(dllexport)
#elif defined(MAYAFLUX_IMPORTS)
#define MAYAFLUX_API __declspec(dllimport)
#else
#define MAYAFLUX_API
#endif
#else
#if defined(__GNUC__) && (__GNUC__ >= 4)
#if defined(MAYAFLUX_EXPORTS)
#define MAYAFLUX_API __attribute__((visibility("default")))
#else
#define MAYAFLUX_API
#endif
#else
#define MAYAFLUX_API
#endif
#endif

// C++20 std::format availability detection
// On macOS, we use fmt library due to runtime availability issues with macOS 14
#if defined(__APPLE__) && defined(__MACH__)
#define MAYAFLUX_USE_FMT 1
#define MAYAFLUX_USE_STD_FORMAT 0
#else
// Check if std::format is available on other platforms
#if __has_include(<format>) && defined(__cpp_lib_format)
#define MAYAFLUX_USE_STD_FORMAT 1
#define MAYAFLUX_USE_FMT 0
#else
#define MAYAFLUX_USE_FMT 1
#define MAYAFLUX_USE_STD_FORMAT 0
#endif
#endif

namespace MayaFlux::Platform {

#ifdef MAYAFLUX_PLATFORM_WINDOWS
constexpr char PathSeparator = '\\';
#else
constexpr char PathSeparator = '/';
#endif

} // namespace MayaFlux::Platform
