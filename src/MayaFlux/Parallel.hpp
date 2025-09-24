#pragma once

#ifdef MAYAFLUX_PLATFORM_MACOS
#include "oneapi/dpl/execution"
namespace MayaFlux::Parallel {
using oneapi::dpl::for_each;
using oneapi::dpl::sort;
using oneapi::dpl::transform;
using std::execution::par;
using std::execution::par_unseq;
using std::execution::seq;
}
#else
#include <execution>
namespace MayaFlux::Parallel {
using std::for_each;
using std::sort;
using std::transform;
using std::execution::par;
using std::execution::par_unseq;
using std::execution::seq;
}
#endif
