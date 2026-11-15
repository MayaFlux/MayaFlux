#pragma once

#include "DataFieldTraversal.hpp"
#include "ForestIndex.hpp"

namespace MayaFlux::Kakshya {

/**
 * @brief Build a ForestIndex from an array whose elements carry a parent row.
 * @tparam Array Indexed array satisfying FieldArray.
 * @tparam Parent Callable taking a borrowed element and returning
 * std::optional<uint64_t>; an empty result marks a root.
 * @param error Receives the reason when nullopt is returned.
 * @param error_row Receives the failing row when given.
 * @return The index, or nullopt for an unreadable row or a parent that does
 * not precede its child.
 */
template <FieldArray Array, typename Parent>
[[nodiscard]] std::optional<ForestIndex> build_forest_index(
    const Array& array, Parent&& parent, std::string& error,
    uint64_t* error_row = nullptr)
{
    std::vector<uint64_t> parents;
    parents.reserve(array.size());
    for (size_t row = 0; row < array.size(); ++row) {
        const auto element = array.at(row);
        if (!element) {
            if (error_row)
                *error_row = row;
            error = "row is unreadable";
            return std::nullopt;
        }
        const std::optional<uint64_t> link = std::invoke(parent, *element);
        parents.push_back(link.value_or(ForestIndex::no_parent));
    }
    return ForestIndex::build(parents, error, error_row);
}

/**
 * @class DataForestTraversal
 * @brief On-demand walks over the parent links of an indexed array.
 *
 * The sibling of DataFieldTraversal for hierarchical rows. It borrows an
 * array and a ForestIndex built from that array's parent links, and walks
 * children, ancestors, descendants and subtrees without owning either. When
 * the rows are preorder, subtree() is an ordinary slice of the array, so a
 * DataFieldTraversal over it windows, projects and reduces one branch with
 * no copy. The same walks work with any FieldArray; neither this class nor
 * the array names a record format.
 *
 * Usage:
 * @code
 * auto index = build_forest_index(rows, [](const auto& row) {
 *     return row.template get<uint64_t>("parent");
 * }, error);
 * DataForestTraversal<CompositeArray> forest(rows, *index);
 * forest.for_each_child(0, [](size_t row, const auto& child) { });
 * if (auto branch = forest.subtree(2)) {
 *     DataFieldTraversal<std::remove_cvref_t<decltype(*branch)>> window(*branch, 64);
 * }
 * @endcode
 *
 * Visitors take the row index and a borrowed element. A visitor returning
 * bool stops the walk by returning false, and the walk then returns false.
 * A row whose element cannot be borrowed is skipped, as in DataFieldTraversal.
 *
 * @tparam Array Indexed array satisfying FieldArray.
 * @note The array and index must outlive the traversal and stay matched.
 */
template <FieldArray Array>
class DataForestTraversal {
public:
    using Element = std::remove_cvref_t<decltype(*std::declval<const Array&>().at(size_t {}))>;

    /**
     * @brief Borrow an array and the index built from it.
     * @param array Array that must outlive this traversal.
     * @param index Index built from array's parent links; it must outlive this traversal.
     */
    DataForestTraversal(const Array& array, const ForestIndex& index) noexcept
        : m_array(&array)
        , m_index(&index)
    {
    }

    /** @brief The borrowed index. */
    [[nodiscard]] const ForestIndex& index() const noexcept { return *m_index; }

    /** @brief Whether the index and array have the same number of rows. */
    [[nodiscard]] bool matches() const { return m_array->size() == m_index->size(); }

    /** @brief Borrow one element by row. */
    [[nodiscard]] auto element(size_t row) const { return m_array->at(row); }

    /**
     * @brief Borrow the rows of the subtree of row, including row itself.
     * @return The array's slice, or an empty result when the rows are not
     * preorder, row is out of range, or the index does not match the array.
     */
    [[nodiscard]] auto subtree(size_t row) const
    {
        using Slice = decltype(m_array->slice(size_t {}, size_t {}));
        const auto range = m_index->subtree_range(row);
        if (!range || !matches())
            return Slice {};
        return m_array->slice(range->first, range->count);
    }

    /** @brief Visit the rows without a parent, in row order. */
    template <typename Fn>
    bool for_each_root(Fn&& fn) const
    {
        for (const uint64_t root : m_index->roots()) {
            if (!visit(root, fn))
                return false;
        }
        return true;
    }

    /** @brief Visit the direct children of row, in row order. */
    template <typename Fn>
    bool for_each_child(size_t row, Fn&& fn) const
    {
        for (const uint64_t child : m_index->children(row)) {
            if (!visit(child, fn))
                return false;
        }
        return true;
    }

    /** @brief Visit the ancestors of row, nearest first. */
    template <typename Fn>
    bool for_each_ancestor(size_t row, Fn&& fn) const
    {
        for (auto parent = m_index->parent(row); parent; parent = m_index->parent(*parent)) {
            if (!visit(*parent, fn))
                return false;
        }
        return true;
    }

    /**
     * @brief Visit every descendant of row in document order, excluding row.
     * @note Preorder rows take a sequential pass; other forests walk the
     * child links depth first.
     */
    template <typename Fn>
    bool for_each_descendant(size_t row, Fn&& fn) const
    {
        if (const auto range = m_index->subtree_range(row)) {
            for (size_t next = range->first + 1; next < range->first + range->count; ++next) {
                if (!visit(next, fn))
                    return false;
            }
            return true;
        }

        std::vector<uint64_t> pending;
        push_children(pending, row);
        while (!pending.empty()) {
            const uint64_t next = pending.back();
            pending.pop_back();
            if (!visit(next, fn))
                return false;
            push_children(pending, next);
        }
        return true;
    }

    /**
     * @brief Fold row and its descendants into an accumulator.
     * @tparam Fn Callable returning the next accumulator from the accumulator,
     * a row index and a borrowed element.
     * @return Final accumulator.
     */
    template <typename Acc, typename Fn>
    [[nodiscard]] Acc reduce_subtree(size_t row, Acc init, Fn&& fn) const
    {
        if (const auto element = m_array->at(row))
            init = std::invoke(fn, std::move(init), row, *element);
        for_each_descendant(row, [&](size_t next, const Element& value) {
            init = std::invoke(fn, std::move(init), next, value);
        });
        return init;
    }

private:
    template <typename Fn>
    bool visit(size_t row, Fn& fn) const
    {
        const auto element = m_array->at(row);
        if (!element)
            return true;
        if constexpr (std::same_as<std::invoke_result_t<Fn&, size_t, const Element&>, bool>) {
            return std::invoke(fn, row, *element);
        } else {
            std::invoke(fn, row, *element);
            return true;
        }
    }

    void push_children(std::vector<uint64_t>& pending, size_t row) const
    {
        const auto children = m_index->children(row);
        pending.insert(pending.end(), children.rbegin(), children.rend());
    }

    const Array* m_array;
    const ForestIndex* m_index;
};

}
