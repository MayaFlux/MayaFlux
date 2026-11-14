#pragma once

#include "CompositeAccess.hpp"

#include "MayaFlux/Kakshya/DataForestTraversal.hpp"

namespace MayaFlux::Kakshya {

/**
 * @brief Index the parent links of a Composite array.
 * @param rows Array with a uint64_t parent field; a row without it present is a root.
 * @param parent_field Name of the parent field.
 * @param error Receives the reason when nullopt is returned.
 * @param error_row Receives the failing row when given.
 * @return The index, or nullopt when the field is missing or not uint64_t, or a
 * parent does not precede its child.
 */
[[nodiscard]] MAYAFLUX_API std::optional<ForestIndex> index_composite_forest(
    const CompositeArray& rows, std::string_view parent_field,
    std::string& error, uint64_t* error_row = nullptr);

/**
 * @brief One-axis Region covering the subtree of row, inclusive.
 *
 * Any Region consumer then applies to a branch: CompositeContainer region
 * copies and slices, and Yantra region inference.
 *
 * @return The Region, or nullopt when the rows are not preorder or row is out of range.
 */
[[nodiscard]] MAYAFLUX_API std::optional<Region> forest_subtree_region(
    const ForestIndex& index, size_t row);

/**
 * @brief Copy the subtree of row into a self-contained array.
 *
 * Rows are copied in order and their parent links are rebased to the copy:
 * the subtree root becomes a root and every other row points at its parent's
 * new position. A copy made through a container Region keeps the original
 * links instead, so it is not a forest on its own.
 *
 * @param rows Source array the index was built from.
 * @param index Index of rows.
 * @param parent_field Name of the uint64_t parent field.
 * @param row Root of the subtree to copy.
 * @param error Receives the reason when nullopt is returned.
 * @return The copy, or nullopt when the rows are not preorder, row is out of
 * range, or the field is missing or not uint64_t.
 */
[[nodiscard]] MAYAFLUX_API std::optional<CompositeArray> copy_forest_subtree(
    const CompositeArray& rows, const ForestIndex& index,
    std::string_view parent_field, size_t row, std::string& error);

/**
 * @class CompositeForest
 * @brief A Composite array together with the index of its parent links.
 *
 * Owns both so a hierarchical document can be held, shared and walked without
 * a separate reader or tree copy. Instances are immutable and shared, which
 * keeps the array at a stable address for borrowed views.
 *
 * Usage:
 * @code
 * auto forest = CompositeForest::build(std::move(rows), "parent", error);
 * auto traversal = forest->traversal();
 * auto branch = forest->subtree_region(3);
 * @endcode
 */
class MAYAFLUX_API CompositeForest {
public:
    /**
     * @brief Index rows and take ownership of them.
     * @param rows Array with a uint64_t parent field.
     * @param parent_field Name of the parent field.
     * @return The forest, or nullptr with the reason in error.
     */
    [[nodiscard]] static std::shared_ptr<const CompositeForest> build(
        CompositeArray rows, std::string_view parent_field,
        std::string& error, uint64_t* error_row = nullptr);

    /** @brief The owned rows. */
    [[nodiscard]] const CompositeArray& rows() const noexcept { return m_rows; }

    /** @brief The index of the owned rows. */
    [[nodiscard]] const ForestIndex& index() const noexcept { return m_index; }

    /** @brief A traversal borrowing the owned rows and index. */
    [[nodiscard]] DataForestTraversal<CompositeArray> traversal() const noexcept
    {
        return DataForestTraversal<CompositeArray>(m_rows, m_index);
    }

    /** @brief Region covering the subtree of row, or nullopt as forest_subtree_region. */
    [[nodiscard]] std::optional<Region> subtree_region(size_t row) const
    {
        return forest_subtree_region(m_index, row);
    }

    /** @brief Self-contained copy of the subtree of row, as copy_forest_subtree. */
    [[nodiscard]] std::optional<CompositeArray> copy_subtree(
        size_t row, std::string& error) const
    {
        return copy_forest_subtree(m_rows, m_index, m_parent_field, row, error);
    }

private:
    CompositeForest(CompositeArray rows, ForestIndex index, std::string parent_field)
        : m_rows(std::move(rows))
        , m_index(std::move(index))
        , m_parent_field(std::move(parent_field))
    {
    }

    CompositeArray m_rows;
    ForestIndex m_index;
    std::string m_parent_field;
};

}
