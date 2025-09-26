#pragma once

// Cross-platform definitions
#ifdef MAYAFLUX_PLATFORM_WINDOWS
using u_int = unsigned int;
using u_int8_t = uint8_t;
using u_int16_t = uint16_t;
using u_int32_t = uint32_t;
using u_int64_t = uint64_t;
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

namespace MayaFlux {

constexpr size_t node_channel_cache_size = 256;

namespace Platform {
#ifdef MAYAFLUX_PLATFORM_WINDOWS
    constexpr char PathSeparator = '\\';
#else
    constexpr char PathSeparator = '/';
#endif
}
} // namespace MayaFlux
