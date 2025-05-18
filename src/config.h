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

#ifdef _WIN32
#include <windows.h>
#include <shlwapi.h>
#pragma comment(lib, "shlwapi.lib")
#endif
