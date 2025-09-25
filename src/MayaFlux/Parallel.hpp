#pragma once

#ifdef MAYAFLUX_PLATFORM_MACOS
#include "oneapi/dpl/algorithm"
#include "oneapi/dpl/execution"
#else
#include <execution>
#endif

namespace MayaFlux::Parallel {
using std::for_each;
using std::sort;
using std::transform;
using std::execution::par;
using std::execution::par_unseq;
using std::execution::seq;
}
