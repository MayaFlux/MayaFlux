#pragma once

#include "RtAudio.h"

// System
#include "algorithm"
#include "any"
#include "atomic"
#include "deque"
#include "format"
#include "functional"
#include "map"
#include "numbers"
#include "string"
#include "unordered_map"
#include "utility"
#include "vector"

// C style
#include <cmath>
#include <cassert>
#include <complex>
#include <cstdint>
#include <exception>
#include <memory>
#include <mutex>
#include <random>
#include <thread>
#include <unordered_set>

#ifdef _WIN32
#include <windows.h>
#include <shlwapi.h>
#pragma comment(lib, "shlwapi.lib")

#include <optional>
// Temporary workarounds
using u_int32_t = uint32_t;
using u_int64_t = uint64_t;

//Ugly workarounds
#ifdef CALLBACK
#undef CALLBACK
#endif

#endif
