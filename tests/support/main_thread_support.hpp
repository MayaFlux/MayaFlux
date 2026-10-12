#pragma once

#include <chrono>
#include <future>
#include <thread>
#include <utility>

#include "MayaFlux/Transitive/Parallel/Dispatch.hpp"

namespace MayaFlux::Test {

inline void service_main_queue(std::chrono::milliseconds slice = std::chrono::milliseconds(2))
{
#if defined(MAYAFLUX_PLATFORM_MACOS)
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, std::chrono::duration<double>(slice).count(), false);
#else
    std::this_thread::sleep_for(slice);
#endif
}

template <typename Func>
auto call_servicing_main_queue(Func&& func)
{
    return Parallel::await_future(std::async(std::launch::async, std::forward<Func>(func)));
}

}
