#pragma once

#include <execution>

namespace MayaFlux::Kinesis::detail {

inline constexpr size_t k_parallel_threshold = 16384;

/**
 * @brief Call @p fn(index, item) for every item, in parallel above a size threshold.
 *
 * The index is recovered from the item's address, so no index array is built.
 * @p fn must be safe to run unsequenced when @p policy is par_unseq.
 *
 * @param grain Item count from which the work is split. Lower it when one item
 *              is a large piece of work.
 */
template <typename Policy, typename T, typename Fn>
void for_each_indexed(const Policy& policy, std::span<T> items, const Fn& fn, size_t grain = k_parallel_threshold)
{
    if (items.size() < grain) {
        for (size_t i = 0; i < items.size(); ++i) {
            fn(i, items[i]);
        }
        return;
    }
    T* const base = items.data();
    std::for_each(policy, items.begin(), items.end(),
        [&fn, base](T& item) { fn(static_cast<size_t>(&item - base), item); });
}

} // namespace MayaFlux::Kinesis::detail
