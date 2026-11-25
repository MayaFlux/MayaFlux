#pragma once

#include "SnapshotCell.hpp"

namespace MayaFlux::Memory {

/**
 * @class OwnedCell
 * @brief A value that any number of readers take and one driver at a time writes.
 *
 * A driver joins and gets an id. Joining decides nothing: a driver writes only after
 * hand_to() has named it, and a write from any other is refused. hand_to() first gives the
 * new driver the value as it stands, so it carries on from there.
 *
 * A driver either writes when it has something new, or gives join() a pull function that is
 * asked each time the value is read while it drives.
 *
 * @code
 * auto cell = std::make_shared<Memory::OwnedCell<float>>(0.0F);
 * const auto knob = cell->join();
 * cell->hand_to(knob);
 * cell->write(knob, 0.5F);
 * @endcode
 *
 * read() and write() may be called from any thread. One driver is expected to write from one
 * thread at a time.
 *
 * @tparam T Copyable value.
 */
template <typename T>
class OwnedCell {
public:
    using Driver = uint32_t;
    using Resume = std::function<void(const T&)>;
    using Pull = std::function<T()>;

    explicit OwnedCell(const T& initial)
        : m_initial(initial)
        , m_value(initial)
    {
    }

    /**
     * @brief Register a driver. It drives nothing until hand_to().
     * @param resume Called by hand_to() with the value the driver takes over from.
     * @param pull Asked for the value on every read while the driver drives.
     */
    [[nodiscard]] Driver join(Resume resume = {}, Pull pull = {})
    {
        const std::scoped_lock lock(m_mutex);
        const Driver id = ++m_last;
        m_drivers.emplace(id, std::make_shared<const Callbacks>(std::move(resume), std::move(pull)));
        return id;
    }

    /** @brief Forget a driver, leaving nothing driving when it was. */
    void leave(Driver driver)
    {
        const std::scoped_lock lock(m_mutex);
        m_drivers.erase(driver);
        if (m_driver.load(std::memory_order_acquire) == driver) {
            m_driver.store(0, std::memory_order_release);
        }
    }

    /**
     * @brief Make a driver the one that writes. Its resume function must not call this cell.
     * @return False, changing nothing, for a driver that never joined or has left.
     */
    bool hand_to(Driver driver)
    {
        const T current = read();
        const std::scoped_lock lock(m_mutex);
        const auto found = m_drivers.find(driver);
        if (found == m_drivers.end()) {
            return false;
        }
        if (found->second->first) {
            found->second->first(current);
        }
        m_driver.store(driver, std::memory_order_release);
        return true;
    }

    [[nodiscard]] bool drives(Driver driver) const noexcept
    {
        return driver != 0 && m_driver.load(std::memory_order_acquire) == driver;
    }

    /** @return False, changing nothing, when the driver does not drive. */
    bool write(Driver driver, const T& value)
    {
        if (!drives(driver)) {
            return false;
        }
        m_value.publish(value);
        return true;
    }

    /** @brief The value now, asking the driving pull function first when it has one. */
    [[nodiscard]] T read() const
    {
        std::shared_ptr<const Callbacks> callbacks;
        {
            const std::scoped_lock lock(m_mutex);
            if (const auto found = m_drivers.find(m_driver.load(std::memory_order_acquire)); found != m_drivers.end()) {
                callbacks = found->second;
            }
        }
        if (callbacks && callbacks->second) {
            const T pulled = callbacks->second();
            m_value.publish(pulled);
            return pulled;
        }
        const auto guard = m_value.read();
        return guard ? *guard : m_initial;
    }

private:
    using Callbacks = std::pair<Resume, Pull>;

    T m_initial;
    mutable SnapshotCell<T> m_value;
    mutable std::mutex m_mutex;
    std::unordered_map<Driver, std::shared_ptr<const Callbacks>> m_drivers;
    std::atomic<Driver> m_driver { 0 };
    Driver m_last { 0 };
};

} // namespace MayaFlux::Memory
