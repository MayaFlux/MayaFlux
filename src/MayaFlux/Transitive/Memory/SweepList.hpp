#pragma once

namespace MayaFlux::Memory {

/**
 * @class SweepList
 * @brief Lock-free, unbounded, multi-producer collection of in-flight items.
 *
 * Holds items that are checked and reclaimed out of order, such as `std::future<T>`
 * handles. Any thread may push() or sweep(). It grows without bound, one allocation
 * per push(), so there is no capacity to choose. push() never blocks, and sweep()
 * and drain() take no lock.
 *
 * approx_size() can be briefly stale under concurrent push() and sweep().
 *
 * @tparam T Element type. Move-only types are supported.
 *
 * @code{.cpp}
 * Memory::SweepList<std::future<bool>> tasks;
 *
 * tasks.push(std::async(std::launch::async, [] { return do_work(); }));
 *
 * tasks.sweep(
 *     [](std::future<bool>& f) {
 *         return f.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
 *     },
 *     [](std::future<bool>& f) { (void)f.get(); });
 *
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
     * @brief Discards any remaining items without finishing them.
     *
     * Call drain() first when items must be finished, such as observing exceptions
     * or consuming results.
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

        Link* node = owned.release();

        node->next = m_head.load(std::memory_order_relaxed);
        while (!m_head.compare_exchange_weak(
            node->next, node,
            std::memory_order_release, std::memory_order_relaxed)) {
        }

        m_count.fetch_add(1, std::memory_order_relaxed);
    }

    /**
     * @brief Non-blocking pass over the held items.
     *
     * Calls @p finish on each item for which @p ready returns true and frees it. The
     * rest stay for a later sweep() or drain(), as do items pushed meanwhile.
     *
     * @tparam Pred Callable (T&) -> bool.
     * @tparam Finish Callable (T&) -> void.
     * @param ready True when the item is done.
     * @param finish Called once per finished item.
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
     * @brief Blocking pass that finishes every held item.
     *
     * Calls @p finish on each item, in unspecified order. For teardown and waiting
     * for everything, where @p finish does the waiting. Items pushed meanwhile stay.
     *
     * @tparam Finish Callable (T&) -> void.
     * @param finish Called once per item.
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
     * @brief Remove and return one item, the most recently pushed of those it sees.
     *
     * With several takers one may get nothing although items exist.
     *
     * @return The item, or an empty optional if none was taken.
     */
    [[nodiscard]] std::optional<T> try_take()
    {
        std::optional<T> taken;
        sweep(
            [&taken](T&) { return !taken.has_value(); },
            [&taken](T& item) { taken.emplace(std::move(item)); });
        return taken;
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
