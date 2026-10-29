#pragma once

namespace MayaFlux::Memory {

/**
 * @class FlatMap
 * @brief Map kept as one sorted vector, cheap to copy.
 *
 * Intended as the value of a SnapshotCell, whose every update copies the whole
 * value: copying this is one allocation and a memcpy for trivially copyable
 * entries, where a node based map allocates once per entry. Lookup is a binary
 * search; insert and erase shift the tail, so it suits maps of up to a few
 * thousand entries that are read far more often than they change.
 *
 * Iteration is in key order. Insert and erase invalidate iterators.
 *
 * @tparam K       Key type.
 * @tparam V       Mapped type.
 * @tparam Compare Strict weak ordering on K.
 */
template <typename K, typename V, typename Compare = std::less<K>>
class FlatMap {
public:
    using value_type = std::pair<K, V>;
    using const_iterator = typename std::vector<value_type>::const_iterator;

    [[nodiscard]] const_iterator begin() const noexcept { return m_items.begin(); }
    [[nodiscard]] const_iterator end() const noexcept { return m_items.end(); }
    [[nodiscard]] size_t size() const noexcept { return m_items.size(); }
    [[nodiscard]] bool empty() const noexcept { return m_items.empty(); }

    /** @brief The entry for @p key, or end() when absent. */
    [[nodiscard]] const_iterator find(const K& key) const
    {
        const size_t index = lower_index(key);
        return index < m_items.size() && !m_less(key, m_items[index].first)
            ? m_items.begin() + static_cast<std::ptrdiff_t>(index)
            : m_items.end();
    }

    /**
     * @brief Sets the value for @p key, adding the entry if it is absent.
     * @return true if the entry was added, false if an existing one was overwritten.
     */
    bool insert_or_assign(const K& key, V value)
    {
        const size_t index = lower_index(key);
        if (index < m_items.size() && !m_less(key, m_items[index].first)) {
            m_items[index].second = std::move(value);
            return false;
        }
        m_items.insert(m_items.begin() + static_cast<std::ptrdiff_t>(index), value_type(key, std::move(value)));
        return true;
    }

    /** @brief Removes the entry for @p key. @return true if there was one. */
    bool erase(const K& key)
    {
        const size_t index = lower_index(key);
        if (index >= m_items.size() || m_less(key, m_items[index].first)) {
            return false;
        }
        m_items.erase(m_items.begin() + static_cast<std::ptrdiff_t>(index));
        return true;
    }

    void clear() noexcept { m_items.clear(); }

private:
    [[nodiscard]] size_t lower_index(const K& key) const
    {
        const auto it = std::ranges::lower_bound(m_items, key, m_less, &value_type::first);
        return static_cast<size_t>(it - m_items.begin());
    }

    std::vector<value_type> m_items;
    [[no_unique_address]] Compare m_less {};
};

} // namespace MayaFlux::Memory
