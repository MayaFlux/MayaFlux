#pragma once

namespace MayaFlux {

namespace internal {

    inline std::vector<std::shared_ptr<void>>& persistent_store()
    {
        static std::vector<std::shared_ptr<void>> s_store;
        return s_store;
    }

    inline void cleanup_persistent_store()
    {
        persistent_store().clear();
    }

} // namespace MayaFlux::internal

/**
 * @brief Transfer ownership of an existing object to the persistent store for process lifetime.
 * @tparam T Any type managed by shared_ptr.
 * @param obj Object to retain.
 * @return The same shared_ptr, allowing inline chaining.
 */
template <typename T>
std::shared_ptr<T> store(std::shared_ptr<T> obj)
{
    internal::persistent_store().push_back(std::static_pointer_cast<void>(obj));
    return obj;
}

/**
 * @brief Transfer ownership of a value to the persistent store for process lifetime.
 * @tparam T Any movable type.
 * @param obj Value to store. Moved into a shared_ptr internally.
 */
template <typename T>
void store(T obj)
{
    internal::persistent_store().push_back(
        std::make_shared<T>(std::move(obj)));
}

/**
 * @brief Construct a T in place, retain it for process lifetime, and return a handle.
 * @tparam T Type to construct.
 * @tparam Args Constructor argument types.
 * @param args Forwarded to std::make_shared<T>.
 * @return Shared pointer to the newly constructed object.
 */
template <typename T, typename... Args>
std::shared_ptr<T> create_persistent(Args&&... args)
{
    return store(std::make_shared<T>(std::forward<Args>(args)...));
}

} // namespace MayaFlux
