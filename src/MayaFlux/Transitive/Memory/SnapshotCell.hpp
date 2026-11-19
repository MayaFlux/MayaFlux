#pragma once

#include "SweepList.hpp"

namespace MayaFlux::Memory {

/**
 * @class SnapshotCell
 * @brief Lock-free cell holding one immutable snapshot of a value, read without blocking.
 *
 * read() returns a Guard on the snapshot that was current at that moment. It stays
 * valid and unchanged while the Guard lives, whatever writers publish meanwhile.
 * update() copies the current snapshot, edits the copy and swaps it in, retrying
 * when another writer got there first. publish() and reset() replace it
 * unconditionally.
 *
 * Replaced snapshots are freed by a later writer once no Guard is held, so a Guard
 * kept for long delays freeing. Hold a Guard for the lookup only, and never past
 * the cell's lifetime.
 *
 * Suited to values read often and replaced rarely, since every update copies the
 * whole value.
 *
 * @tparam T Snapshot type. Readers see it as const, so it needs no internal
 *           synchronisation. update() needs it default constructible and copyable.
 *
 * @code{.cpp}
 * Memory::SnapshotCell<std::unordered_map<int, float>> table;
 *
 * table.update([](auto& map) { map[7] = 0.5F; });
 *
 * if (auto view = table.read()) {
 *     auto it = view->find(7);
 * }
 * @endcode
 */
template <typename T>
class SnapshotCell {
public:
    /**
     * @class Guard
     * @brief Keeps one snapshot alive and unchanged while it exists.
     *
     * Empty when the cell held nothing at read(). Movable, not copyable.
     */
    class Guard {
    public:
        Guard() noexcept = default;

        ~Guard() { release(); }

        Guard(const Guard&) = delete;
        Guard& operator=(const Guard&) = delete;

        Guard(Guard&& other) noexcept
            : m_cell(std::exchange(other.m_cell, nullptr))
            , m_value(std::exchange(other.m_value, nullptr))
        {
        }

        Guard& operator=(Guard&& other) noexcept
        {
            if (this != &other) {
                release();
                m_cell = std::exchange(other.m_cell, nullptr);
                m_value = std::exchange(other.m_value, nullptr);
            }
            return *this;
        }

        /** @brief The snapshot, or null when the cell was empty. */
        [[nodiscard]] const T* get() const noexcept { return m_value; }

        [[nodiscard]] const T* operator->() const noexcept { return m_value; }

        [[nodiscard]] const T& operator*() const noexcept { return *m_value; }

        [[nodiscard]] explicit operator bool() const noexcept { return m_value != nullptr; }

    private:
        friend class SnapshotCell;

        Guard(const SnapshotCell* cell, const T* value) noexcept
            : m_cell(cell)
            , m_value(value)
        {
        }

        void release() noexcept
        {
            if (m_cell) {
                m_cell->m_readers.fetch_sub(1);
            }
            m_cell = nullptr;
            m_value = nullptr;
        }

        const SnapshotCell* m_cell { nullptr };
        const T* m_value { nullptr };
    };

    /** @brief Creates an empty cell. */
    SnapshotCell() noexcept = default;

    /** @brief Creates a cell already holding @p initial. */
    explicit SnapshotCell(T initial)
        : m_head(std::make_unique<T>(std::move(initial)).release())
    {
    }

    SnapshotCell(const SnapshotCell&) = delete;
    SnapshotCell& operator=(const SnapshotCell&) = delete;
    SnapshotCell(SnapshotCell&&) = delete;
    SnapshotCell& operator=(SnapshotCell&&) = delete;

    /**
     * @brief Frees the current snapshot and every retired one.
     *
     * No Guard may be alive and no other thread may be inside the cell.
     */
    ~SnapshotCell()
    {
        delete m_head.load();
    }

    /**
     * @brief Pins the current snapshot.
     * @return A Guard over it, empty when the cell holds nothing.
     */
    [[nodiscard]] Guard read() const noexcept
    {
        m_readers.fetch_add(1);
        const T* value = m_head.load();
        if (!value) {
            m_readers.fetch_sub(1);
            return {};
        }
        return Guard(this, value);
    }

    /**
     * @brief Replaces the snapshot with @p value, whatever it was.
     *
     * Concurrent publishes are ordered arbitrarily and the last one wins. Use
     * update() when the new value depends on the old one.
     */
    void publish(T value)
    {
        replace(std::make_unique<T>(std::move(value)).release());
    }

    /** @brief Empties the cell. */
    void reset()
    {
        replace(nullptr);
    }

    /**
     * @brief Edits a copy of the current snapshot and publishes it.
     *
     * Starts from a default constructed T when the cell is empty. If another
     * writer publishes first, the copy is discarded and @p edit runs again on
     * the newer snapshot, so it may run more than once and must depend only on
     * the value it is given.
     *
     * @tparam Edit Callable (T&) returning void, or bool.
     * @param edit  Mutates the copy. A bool return of false abandons the edit
     *              and publishes nothing.
     * @return true if a new snapshot was published, false if @p edit declined.
     */
    template <typename Edit>
        requires std::invocable<Edit&, T&> && std::default_initializable<T> && std::copy_constructible<T>
    bool update(Edit edit)
    {
        using Result = std::invoke_result_t<Edit&, T&>;
        static_assert(std::is_void_v<Result> || std::is_same_v<Result, bool>,
            "SnapshotCell::update edit must return void or bool");

        for (;;) {
            Guard pin = read();
            const T* current = pin.get();

            auto draft = current ? std::make_unique<T>(*current) : std::make_unique<T>();

            if constexpr (std::is_void_v<Result>) {
                std::invoke(edit, *draft);
            } else if (!std::invoke(edit, *draft)) {
                return false;
            }

            const T* expected = current;
            if (m_head.compare_exchange_strong(expected, draft.get())) {
                draft.release();
                pin = Guard {};
                retire(current);
                return true;
            }
        }
    }

private:
    void replace(const T* next)
    {
        retire(m_head.exchange(next));
    }

    void retire(const T* old)
    {
        if (old) {
            m_retired.push(std::unique_ptr<const T>(old));
        }
        reclaim();
    }

    void reclaim()
    {
        if (m_retired.empty()) {
            return;
        }
        m_retired.sweep(
            [this](std::unique_ptr<const T>&) { return m_readers.load() == 0; },
            [](std::unique_ptr<const T>& snapshot) { snapshot.reset(); });
    }

    alignas(64) std::atomic<const T*> m_head { nullptr };
    alignas(64) mutable std::atomic<uint32_t> m_readers { 0 };
    SweepList<std::unique_ptr<const T>> m_retired;
};

} // namespace MayaFlux::Memory
