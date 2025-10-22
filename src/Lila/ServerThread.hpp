#pragma once

#if __has_include(<stop_token>)
#include <stop_token>
#endif

/**
 * @file ServerThread.hpp
 * @brief Platform-specific threading wrapper for Server class
 *
 * This header provides a surgical fix for std::jthread issues on specific platforms
 * (notably Apple Clang 15 shipped with Xcode 15.x) WITHOUT disabling other C++23 features.
 *
 * Detection Strategy:
 * 1. Check if __cpp_lib_jthread feature test macro indicates support
 * 2. On macOS, check Apple Clang compiler version specifically
 * 3. Apple Clang 15.x (15000000 - 15999999) has known jthread issues despite claiming support
 * 4. Fallback to std::thread + atomic flag only when jthread is actually broken
 *
 * This does NOT affect any other C++23/C++20 features like:
 * - std::expected
 * - Coroutines (std::coroutine_handle)
 * - std::atomic_ref
 * - Structured bindings
 * - Concepts
 * - Ranges
 */

#if defined(__cpp_lib_jthread) && __cpp_lib_jthread >= 201911L

#if defined(__apple_build_version__)
// Apple Clang: need to verify version
// Apple Clang version format: major * 10000000 + minor * 100000 + patch
// Xcode 15.0 = Apple Clang 15.0.0 = 15000000
// Xcode 15.4 = Apple Clang 15.0.0 = 15000309 (example)
// Xcode 16.0 = Apple Clang 16.0.0 = 16000000

#if __apple_build_version__ >= 15000000 && __apple_build_version__ < 16000000
// Apple Clang 15.x series: known to have jthread issues
// Despite feature test macro claiming support, implementation is buggy
#define MAYAFLUX_JTHREAD_BROKEN 1
#pragma message("ServerThread: Detected Apple Clang 15.x - using std::thread fallback for jthread")
#else
// Apple Clang < 15 or >= 16: assume working jthread
#define MAYAFLUX_JTHREAD_WORKING 1
#endif
#else
// Non-Apple compilers: trust the feature test macro
#define MAYAFLUX_JTHREAD_WORKING 1
#endif
#else
// Feature test macro says jthread is not available
#define MAYAFLUX_JTHREAD_BROKEN 1
#pragma message("ServerThread: std::jthread not available - using std::thread fallback")
#endif

namespace Lila {

#ifdef MAYAFLUX_JTHREAD_WORKING

/**
 * @brief Modern std::jthread-based thread wrapper
 *
 * Direct wrapper around std::jthread for platforms with full, working support.
 * Provides zero-overhead abstraction - compiles to identical code as raw std::jthread.
 */
class ServerThread {
public:
    using Callback = std::function<void(const std::stop_token&)>;

    ServerThread() = default;

    explicit ServerThread(Callback callback)
        : m_thread(std::move(callback))
    {
    }

    ServerThread(ServerThread&&) noexcept = default;
    ServerThread& operator=(ServerThread&&) noexcept = default;

    ServerThread(const ServerThread&) = delete;
    ServerThread& operator=(const ServerThread&) = delete;

    void request_stop() noexcept
    {
        m_thread.request_stop();
    }

    void join()
    {
        if (m_thread.joinable()) {
            m_thread.join();
        }
    }

    [[nodiscard]] bool joinable() const noexcept
    {
        return m_thread.joinable();
    }

private:
    std::jthread m_thread;
};

#else // MAYAFLUX_JTHREAD_BROKEN

/**
 * @brief Fallback std::thread-based thread wrapper with manual stop signaling
 *
 * Compatible implementation for platforms where std::jthread is unavailable or broken.
 * Provides identical interface using std::thread + std::atomic<bool> for stop signaling.
 *
 * Implementation Notes:
 * - StopToken provides API-compatible interface with std::stop_token
 * - Uses shared_ptr<atomic<bool>> to ensure stop flag outlives thread
 * - Memory ordering: acquire on read, release on write (matches std::stop_token semantics)
 * - Minimal overhead: one atomic load per loop iteration (~1-2 CPU cycles)
 */
class ServerThread {
public:
    /**
     * @brief API-compatible stop token for platforms without std::stop_token
     *
     * Mimics std::stop_token's stop_requested() interface.
     */
    class StopToken {
    public:
        explicit StopToken(const std::atomic<bool>* stop_flag)
            : m_stop_flag(stop_flag)
        {
        }

        [[nodiscard]] bool stop_requested() const noexcept
        {
            return m_stop_flag && m_stop_flag->load(std::memory_order_acquire);
        }

    private:
        const std::atomic<bool>* m_stop_flag { nullptr };
    };

    using Callback = std::function<void(const StopToken&)>;

    ServerThread() = default;

    explicit ServerThread(Callback callback)
        : m_stop_flag(std::make_shared<std::atomic<bool>>(false))
    {
        // Capture stop_flag by value (shared_ptr) to ensure it outlives the thread
        // critical: the thread lambda must hold a reference to keep the flag alive
        auto stop_flag = m_stop_flag;

        m_thread = std::thread([callback = std::move(callback), stop_flag]() {
            StopToken token(stop_flag.get());
            callback(token);
        });
    }

    ServerThread(ServerThread&& other) noexcept
        : m_thread(std::move(other.m_thread))
        , m_stop_flag(std::move(other.m_stop_flag))
    {
    }

    ServerThread& operator=(ServerThread&& other) noexcept
    {
        if (this != &other) {
            if (m_thread.joinable()) {
                request_stop();
                m_thread.join();
            }

            m_thread = std::move(other.m_thread);
            m_stop_flag = std::move(other.m_stop_flag);
        }
        return *this;
    }

    ServerThread(const ServerThread&) = delete;
    ServerThread& operator=(const ServerThread&) = delete;

    ~ServerThread()
    {
        // Automatically join on destruction (mimics std::jthread behavior)
        if (m_thread.joinable()) {
            request_stop();
            m_thread.join();
        }
    }

    void request_stop() noexcept
    {
        if (m_stop_flag) {
            m_stop_flag->store(true, std::memory_order_release);
        }
    }

    void join()
    {
        if (m_thread.joinable()) {
            m_thread.join();
        }
    }

    [[nodiscard]] bool joinable() const noexcept
    {
        return m_thread.joinable();
    }

private:
    std::thread m_thread;
    std::shared_ptr<std::atomic<bool>> m_stop_flag;
};

#endif // MAYAFLUX_JTHREAD_BROKEN / MAYAFLUX_JTHREAD_WORKING

} // namespace Lila
