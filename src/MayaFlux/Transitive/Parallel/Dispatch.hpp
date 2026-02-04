#pragma once

#ifdef MAYAFLUX_PLATFORM_MACOS
#include <dispatch/dispatch.h>
#endif

#ifdef MAYAFLUX_PLATFORM_WINDOWS
#include <windows.h>
#ifdef ERROR
#undef ERROR
#endif // ERROR

#define MAYAFLUX_WM_DISPATCH (WM_USER + 0x0001)
#endif // MAYAFLUX_PLATFORM_WINDOWS

namespace MayaFlux::Parallel {

#ifdef MAYAFLUX_PLATFORM_MACOS
/**
 * @brief Execute a function on the main dispatch queue (macOS only)
 * @tparam Func Callable type
 * @tparam Args Argument types
 * @param func Function to execute
 * @param args Arguments to forward to the function
 *
 * Schedules work on the main queue asynchronously. Returns immediately.
 * Use this for GLFW operations that must execute on the main thread.
 */
template <typename Func, typename... Args>
void dispatch_main_async(Func&& func, Args&&... args)
{
    dispatch_async(dispatch_get_main_queue(), ^{
        std::invoke(std::forward<Func>(func), std::forward<Args>(args)...);
    });
}

/**
 * @brief Execute a function on the main dispatch queue and wait (macOS only)
 * @tparam Func Callable type that returns a result
 * @tparam Args Argument types
 * @param func Function to execute
 * @param args Arguments to forward to the function
 * @return The result returned by func
 *
 * Schedules work on the main queue synchronously. Blocks until complete.
 * WARNING: Can deadlock if called from main thread or during Cocoa modal loops.
 */
template <typename Func, typename... Args>
auto dispatch_main_sync(Func&& func, Args&&... args) -> decltype(auto)
{
    using ResultType = std::invoke_result_t<Func, Args...>;

    if constexpr (std::is_void_v<ResultType>) {
        dispatch_sync(dispatch_get_main_queue(), ^{
            std::invoke(std::forward<Func>(func), std::forward<Args>(args)...);
        });
    } else {
        __block ResultType result;
        dispatch_sync(dispatch_get_main_queue(), ^{
            result = std::invoke(std::forward<Func>(func), std::forward<Args>(args)...);
        });
        return result;
    }
}

/**
 * @brief Execute a function on the main queue with timeout (macOS only)
 * @tparam Func Callable type
 * @tparam Args Argument types
 * @param timeout_ms Timeout in milliseconds
 * @param func Function to execute
 * @param args Arguments to forward to the function
 * @return true if completed before timeout, false if timed out
 *
 * Schedules work asynchronously and waits with timeout.
 * Returns false if the operation doesn't complete in time.
 */
template <typename Func, typename... Args>
bool dispatch_main_async_with_timeout(std::chrono::milliseconds timeout_ms, Func&& func, Args&&... args)
{
    auto completed = std::make_shared<std::atomic<bool>>(false); // Shared pointer for capture

    dispatch_async(dispatch_get_main_queue(), ^{
        std::invoke(std::forward<Func>(func), std::forward<Args>(args)...);
        completed->store(true, std::memory_order_release);
    });

    auto start = std::chrono::steady_clock::now();
    while (!completed->load(std::memory_order_acquire)) {
        if (std::chrono::steady_clock::now() - start > timeout_ms) {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    return true;
}

#elif defined(MAYAFLUX_PLATFORM_WINDOWS)

inline DWORD g_MainThreadId = 0; /// < Main thread ID, must be set at startup

/**
 * @brief Execute a function on the main thread asynchronously (Windows only)
 * @tparam Func Callable type
 * @tparam Args Argument types
 * @param func Function to execute
 * @param args Arguments to forward to the function
 *
 * Posts a message to the main thread's message queue to execute the function.
 * Use this for GLFW operations that must execute on the main thread.
 */
template <typename Func, typename... Args>
void dispatch_main_async(Func&& func, Args&&... args)
{
    auto task = [f = std::forward<Func>(func), ... args = std::forward<Args>(args)]() mutable {
        std::invoke(std::move(f), std::move(args)...);
    };

    auto* task_ptr = new std::function<void()>(std::move(task));

    PostThreadMessage(g_MainThreadId, MAYAFLUX_WM_DISPATCH, 0, (LPARAM)task_ptr);
}

template <typename Func, typename... Args>
auto dispatch_main_sync(Func&& func, Args&&... args) -> decltype(auto)
{
    return std::invoke(std::forward<Func>(func), std::forward<Args>(args)...);
}

#else
// On linux platforms, these just execute directly
template <typename Func, typename... Args>
void dispatch_main_async(Func&& func, Args&&... args)
{
    std::invoke(std::forward<Func>(func), std::forward<Args>(args)...);
}

template <typename Func, typename... Args>
auto dispatch_main_sync(Func&& func, Args&&... args) -> decltype(auto)
{
    return std::invoke(std::forward<Func>(func), std::forward<Args>(args)...);
}

template <typename Func, typename... Args>
bool dispatch_main_async_with_timeout(std::chrono::milliseconds /*timeout_ms*/, Func&& func, Args&&... args)
{
    std::invoke(std::forward<Func>(func), std::forward<Args>(args)...);
    return true;
}
#endif

} // namespace MayaFlux::Parallel
