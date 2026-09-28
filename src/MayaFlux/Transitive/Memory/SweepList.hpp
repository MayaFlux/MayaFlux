#pragma once

namespace MayaFlux::Memory {

/**
 * @class SweepList
 * @brief Lock-free, unbounded, multi-producer collection of in-flight items.
 *
 * Holds items whose readiness is checked out of order and reclaimed out of
 * order, such as `std::future<T>` handles for asynchronous work. Unlike
 * `RingBuffer`'s lock-free policies, which require a fixed power-of-two
 * capacity, SweepList grows without bound: every push() allocates one link,
 * and every reclaimed item frees its link. There is no capacity to exhaust
 * and no size policy to choose.
 *
 * The concurrency shape is "any thread may push, any thread may sweep."
 * There is no reserved single-consumer role: sweep() and drain() both take
 * the entire current chain atomically via a single exchange(), so once a
 * thread holds that chain no other thread can observe or mutate any link in
 * it. This sidesteps the classic hazard of lock-free list removal, where
 * unlinking one link out of the middle of a shared list races against
 * concurrent traversal, by never unlinking a single link. A sweep always
 * takes everything, decides per item, and republishes the survivors as one
 * new chain.
 *
 * push() is wait-free under the same CAS-retry bound as any Treiber stack.
 * sweep() and drain() are lock-free: no thread blocks on another thread's
 * mutex, though a CAS loop may retry under contention from concurrent
 * pushes.
 *
 * Ownership of each link is a std::unique_ptr everywhere except the single
 * word actually under CAS. That word, m_head, is necessarily a raw pointer:
 * no atomic smart pointer gives genuine lock-free compare-exchange. The
 * standard library's own std::atomic<std::shared_ptr<T>> is typically
 * implemented with an internal lock on every major implementation, since a
 * shared_ptr CAS would need to update an object pointer and a control-block
 * pointer as one indivisible step, which no hardware CAS instruction does.
 * Using it here would silently reintroduce a lock behind an API that no
 * longer looks like one, defeating the reason this type exists. m_head is
 * therefore raw for the same reason LiveArena's backing storage is raw: a
 * narrowly scoped exception at the exact point the type system cannot help,
 * never exposed past this class's own implementation.
 *
 * approx_size() is named approx deliberately. Concurrent pushes and sweeps
 * can make it transiently stale, the same caveat RingBuffer's MPSCPolicy
 * documents for its own size().
 *
 * @tparam T Element type. Move-only types such as std::future<T> are fully
 *           supported; SweepList never copies an element.
 *
 * @code{.cpp}
 * Memory::SweepList<std::future<bool>> tasks;
 *
 * tasks.push(std::async(std::launch::async, [] { return do_work(); }));
 *
 * // Opportunistic, non-blocking prune of completed work.
 * tasks.sweep(
 *     [](std::future<bool>& f) {
 *         return f.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
 *     },
 *     [](std::future<bool>& f) { (void)f.get(); });
 *
 * // Blocking drain of everything still in flight.
 * tasks.drain([](std::future<bool>& f) { (void)f.get(); });
 * @endcode
 */
template <typename T>
class SweepList {
public:
    SweepList() noexcept = default;

    SweepList(const SweepList&) = delete;
    SweepList& operator=(const SweepList&) = delete;
    SweepList(SweepList&&) = delete;
    SweepList& operator=(SweepList&&) = delete;

    /**
     * @brief Drains and discards any remaining items without inspecting them.
     *
     * Callers that need every item finalized, exceptions observed, results
     * consumed, must call drain() with a finish functor before destruction.
     * The destructor exists only to free links a caller genuinely abandoned.
     */
    ~SweepList()
    {
        drain([](T&) noexcept { });
    }

    /**
     * @brief Publishes @p value for later sweep() or drain().
     *
     * Safe to call from any number of concurrent threads. Allocates one
     * link; the link is freed when the item is reclaimed by sweep() or
     * drain().
     *
     * @param value Item to retain. Moved into internal storage.
     */
    void push(T value) noexcept
    {
        std::unique_ptr<Link> owned = std::make_unique<Link>(std::move(value), nullptr);

        // The atomic word alone decides ownership from here: once the CAS
        // below succeeds, m_head is the link's only owner. release() marks
        // that handoff explicitly rather than leaving a raw pointer implied.
        Link* node = owned.release();

        node->next = m_head.load(std::memory_order_relaxed);
        while (!m_head.compare_exchange_weak(
            node->next, node,
            std::memory_order_release, std::memory_order_relaxed)) {
        }

        m_count.fetch_add(1, std::memory_order_relaxed);
    }

    /**
     * @brief Non-blocking pass over every currently held item.
     *
     * Atomically takes the entire chain, then for each item calls
     * @p ready(item). If it returns true, calls @p finish(item) and frees
     * the link. If false, the item is republished so a later sweep() or
     * drain() will see it again.
     *
     * Items pushed by other threads while this sweep is running are not
     * part of the taken chain and are left untouched; they surface on a
     * later sweep().
     *
     * @tparam Pred    Callable (T&) -> bool. True means the item is done
     *                 and safe to finalize now.
     * @tparam Finish  Callable (T&) -> void. Invoked exactly once per item
     *                 for which @p ready returned true, before the link is
     *                 freed.
     * @param ready    Readiness check.
     * @param finish   Finalizer, invoked once per reclaimed item.
     */
    template <typename Pred, typename Finish>
        requires std::invocable<Pred, T&> && std::invocable<Finish, T&>
    void sweep(Pred ready, Finish finish)
    {
        Link* chain = m_head.exchange(nullptr, std::memory_order_acquire);

        Link* survivors = nullptr;
        size_t reclaimed = 0;

        while (chain) {
            Link* next = chain->next;

            if (std::invoke(ready, chain->value)) {
                std::unique_ptr<Link> owned(chain);
                std::invoke(finish, owned->value);
                ++reclaimed;
            } else {
                chain->next = survivors;
                survivors = chain;
            }

            chain = next;
        }

        if (survivors) {
            reattach(survivors);
        }

        m_count.fetch_sub(reclaimed, std::memory_order_relaxed);
    }

    /**
     * @brief Blocking pass that finalizes every item currently held.
     *
     * Atomically takes the entire chain and calls @p finish(item) on every
     * one of them unconditionally, freeing each link as it goes. Intended
     * for teardown paths, destructors, and explicit "wait for everything"
     * calls, where @p finish itself performs the blocking wait (for example
     * `future.get()`).
     *
     * Items pushed by other threads after the chain is taken are not
     * included and remain for a later call.
     *
     * @tparam Finish Callable (T&) -> void.
     * @param finish  Finalizer, invoked once per item, in unspecified order.
     */
    template <typename Finish>
        requires std::invocable<Finish, T&>
    void drain(Finish finish)
    {
        Link* chain = m_head.exchange(nullptr, std::memory_order_acquire);

        while (chain) {
            Link* next = chain->next;
            std::unique_ptr<Link> owned(chain);
            std::invoke(finish, owned->value);
            chain = next;
        }

        m_count.store(0, std::memory_order_relaxed);
    }

    /**
     * @brief Approximate number of items currently held.
     *
     * May be transiently stale under concurrent push()/sweep()/drain().
     * Use for diagnostics, not for synchronization decisions.
     */
    [[nodiscard]] size_t approx_size() const noexcept
    {
        return m_count.load(std::memory_order_relaxed);
    }

    /**
     * @brief Whether the list currently holds no items.
     *
     * Same staleness caveat as approx_size().
     */
    [[nodiscard]] bool empty() const noexcept
    {
        return m_head.load(std::memory_order_acquire) == nullptr;
    }

private:
    struct Link {
        T value;
        Link* next;
    };

    void reattach(Link* survivors) noexcept
    {
        Link* tail = survivors;
        while (tail->next)
            tail = tail->next;

        tail->next = m_head.load(std::memory_order_relaxed);
        while (!m_head.compare_exchange_weak(
            tail->next, survivors,
            std::memory_order_release, std::memory_order_relaxed)) {
        }
    }

    alignas(64) std::atomic<Link*> m_head { nullptr };
    alignas(64) std::atomic<size_t> m_count { 0 };
};

} // namespace MayaFlux::Memory
