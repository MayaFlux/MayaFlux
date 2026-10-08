#pragma once

#include <execution>

namespace MayaFlux::Parallel {

using std::for_each;
using std::sort;
using std::transform;
using std::execution::par;
using std::execution::par_unseq;
using std::execution::seq;

} // namespace MayaFlux::Parallel
