#pragma once

#ifdef RTAUDIO_BACKEND
#include "RtAudio.h"
#endif

// System
#include "algorithm"
#include "any"
#include "atomic"
#include "deque"
#include "format"
#include "functional"
#include "list"
#include "map"
#include "numbers"
#include "numeric"
#include "optional"
#include "shared_mutex"
#include "span"
#include "string"
#include "unordered_map"
#include "utility"
#include "variant"
#include "vector"

// C style
#include <cassert>
#include <cmath>
#include <complex>
#include <cstdint>
#include <exception>
#include <memory>
#include <mutex>
#include <random>
#include <thread>
#include <unordered_set>

// Windows-specific includes
#ifdef MAYAFLUX_PLATFORM_WINDOWS
#include <shlwapi.h>
#include <windows.h>
#pragma comment(lib, "shlwapi.lib")

// Type compatibility
using u_int8_t = uint8_t;
using u_int16_t = uint16_t;
using u_int32_t = uint32_t;
using u_int64_t = uint64_t;

#ifdef CALLBACK
#undef CALLBACK
#endif

#ifdef TRUE
#undef TRUE
#endif

#ifdef FALSE
#undef FALSE
#endif

#ifdef ERROR
#undef ERROR
#endif
#endif

// Cross-platform definitions
#ifdef MAYAFLUX_PLATFORM_WINDOWS
#define MAYAFLUX_EXPORT __declspec(dllexport)
#define MAYAFLUX_IMPORT __declspec(dllimport)
#define MAYFALUX_FORCEINLINE __forceinline
#else
#define MAYAFLUX_EXPORT __attribute__((visibility("default")))
#define MAYAFLUX_IMPORT
#define MAYAFLUX_FORCEINLINE __attribute__((always_inline))
#endif

namespace MayaFlux {
namespace Platform {

#ifdef MAYAFLUX_PLATFORM_WINDOWS
    constexpr char PathSeparator = '\\'; // Fixed the escape
#else
    constexpr char PathSeparator = '/';
#endif

} // namespace Platform
} // namespace MayaFlux
