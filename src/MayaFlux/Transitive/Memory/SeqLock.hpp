#pragma once

namespace MayaFlux::Memory {

/**
 * @class Seqlock
 * @brief Single-writer multiple-reader sequence lock for fixed-size data regions.
 *
 * Protects a data region that is written infrequently and read frequently.
 * Readers never block writers; a reader that observes a write in progress
 * simply retries. Writers are serialized externally (e.g. by the container's
 * existing state machine or a higher-level mutex on the write path only).
 *
 * Sequence counter convention:
 *   even  — data is stable, safe to read
 *   odd   — write in progress, readers must retry
 *
 * Usage — writer side:
 * @code
 * SeqlockWriteGuard guard(lock);
 * std::memcpy(dest, src, bytes);
 * @endcode
 *
 * Usage — reader side (unbounded):
 * @code
 * uint32_t snap;
 * do {
 *     snap = lock.read_begin();
 *     // copy or sample the guarded data
 * } while (lock.read_retry(snap));
 * @endcode
 *
 * Usage — reader side (bounded, preferred in frame loops):
 * @code
 * auto result = seqlock_read(lock, max_attempts, [&]{ return read_data(); });
 * if (!result) {
 // stale read accepted or skip frame
 }
 * @endcode
 *
 * Alignment to a cache line prevents false sharing when multiple Seqlock
 * instances are stored in a contiguous array (e.g. one per texture layer).
 */
class alignas(64) Seqlock {
public:
    Seqlock() noexcept = default;

    Seqlock(const Seqlock&) = delete;
    Seqlock& operator=(const Seqlock&) = delete;
    Seqlock(Seqlock&&) = delete;
    Seqlock& operator=(Seqlock&&) = delete;
    ~Seqlock() = default;

    /**
     * @brief Begin a write: advances the counter to an odd value.
     *
     * Must be paired with end_write(). Prefer SeqlockWriteGuard.
     */
    void begin_write() noexcept
    {
        m_seq.fetch_add(1U, std::memory_order_release);
    }

    /**
     * @brief End a write: advances the counter back to an even value.
     *
     * All stores to the guarded region must be visible before this returns.
     */
    void end_write() noexcept
    {
        m_seq.fetch_add(1U, std::memory_order_release);
    }

    /**
     * @brief Begin a read: spin until the counter is even, return the snapshot.
     *
     * The caller reads the guarded data after this returns, then calls
     * read_retry() to verify no write overlapped.
     *
     * @return Even sequence snapshot to pass to read_retry().
     */
    [[nodiscard]] uint32_t read_begin() const noexcept
    {
        uint32_t v {};
        do {
            v = m_seq.load(std::memory_order_acquire);
        } while (v & 1U);
        return v;
    }

    /**
     * @brief Check whether a write overlapped the read just completed.
     *
     * @param snapshot Value returned by the matching read_begin() call.
     * @return true if the caller must retry the read, false if data is valid.
     */
    [[nodiscard]] bool read_retry(uint32_t snapshot) const noexcept
    {
        std::atomic_thread_fence(std::memory_order_acquire);
        return m_seq.load(std::memory_order_relaxed) != snapshot;
    }

    /**
     * @brief Return the raw sequence counter value.
     *
     * Primarily useful for diagnostics and assertions. Do not use for
     * synchronization logic; use read_begin() / read_retry() instead.
     */
    [[nodiscard]] uint32_t sequence() const noexcept
    {
        return m_seq.load(std::memory_order_relaxed);
    }

private:
    std::atomic<uint32_t> m_seq { 0U };
};

// =============================================================================
// RAII write guard
// =============================================================================

/**
 * @class SeqlockWriteGuard
 * @brief RAII guard that brackets a Seqlock write region.
 *
 * Calls begin_write() on construction and end_write() on destruction.
 * Non-copyable, non-movable: the guard is tied to a single lexical scope.
 *
 * @code
 * {
 *     SeqlockWriteGuard guard(m_layer_locks[layer]);
 *     std::memcpy(dest, src, byte_count);
 * }
 * @endcode
 */
class SeqlockWriteGuard {
public:
    explicit SeqlockWriteGuard(Seqlock& lock) noexcept
        : m_lock(lock)
    {
        m_lock.begin_write();
    }

    ~SeqlockWriteGuard() noexcept
    {
        m_lock.end_write();
    }

    SeqlockWriteGuard(const SeqlockWriteGuard&) = delete;
    SeqlockWriteGuard& operator=(const SeqlockWriteGuard&) = delete;
    SeqlockWriteGuard(SeqlockWriteGuard&&) = delete;
    SeqlockWriteGuard& operator=(SeqlockWriteGuard&&) = delete;

private:
    Seqlock& m_lock;
};

// =============================================================================
// Bounded reader helper
// =============================================================================

/**
 * @brief Invoke a read functor under a Seqlock with a bounded retry count.
 *
 * Retries the read up to @p max_attempts times if a concurrent write is
 * detected. On each attempt the functor is called with no arguments; its
 * return value is forwarded on success.
 *
 * Returns std::nullopt if @p max_attempts is exhausted without a clean
 * read. Callers in frame loops should treat nullopt as "use previous frame"
 * rather than blocking.
 *
 * The functor must be callable with no arguments and must return a value
 * (or void — see seqlock_read_void for void functors).
 *
 * @tparam Fn    Callable type, must satisfy std::invocable<Fn>.
 * @param  lock  The Seqlock guarding the data region.
 * @param  max_attempts  Maximum number of read attempts before giving up.
 *                       0 means unbounded (spin until clean).
 * @param  fn    Functor that reads and returns the guarded data.
 * @return       std::optional<return type of fn>, empty on exhaustion.
 *
 * @code
 * auto pixels = seqlock_read(m_layer_locks[layer], 8, [&] {
 *     return std::span<const uint8_t>(buf.data(), buf.size());
 * });
 * if (pixels) upload_to_gpu(*pixels);
 * @endcode
 */
template <std::invocable Fn>
[[nodiscard]] auto seqlock_read(
    const Seqlock& lock,
    uint32_t max_attempts,
    Fn&& fn) -> std::optional<std::invoke_result_t<Fn>>
{
    uint32_t attempts = 0;
    while (max_attempts == 0 || attempts < max_attempts) {
        uint32_t snap = lock.read_begin();
        auto result = std::invoke(std::forward<Fn>(fn));
        if (!lock.read_retry(snap))
            return result;
        ++attempts;
    }
    return std::nullopt;
}

/**
 * @brief Invoke a void read functor under a Seqlock with a bounded retry count.
 *
 * Same semantics as seqlock_read but for functors with no return value.
 * Returns true if a clean read was achieved within @p max_attempts,
 * false if the limit was exhausted.
 *
 * @tparam Fn   Callable type with void return, must satisfy std::invocable<Fn>.
 * @param  lock  The Seqlock guarding the data region.
 * @param  max_attempts  Maximum attempts; 0 = unbounded.
 * @param  fn    Functor that reads the guarded region (side-effect only).
 * @return       true on clean read, false on exhaustion.
 */
template <std::invocable Fn>
    requires std::is_void_v<std::invoke_result_t<Fn>>
bool seqlock_read_void(
    const Seqlock& lock,
    uint32_t max_attempts,
    Fn&& fn)
{
    uint32_t attempts = 0;
    while (max_attempts == 0 || attempts < max_attempts) {
        uint32_t snap = lock.read_begin();
        std::invoke(std::forward<Fn>(fn));
        if (!lock.read_retry(snap))
            return true;
        ++attempts;
    }
    return false;
}

} // namespace MayaFlux::Memory
