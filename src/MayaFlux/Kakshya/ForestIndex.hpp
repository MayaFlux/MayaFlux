#pragma once

namespace MayaFlux::Kakshya {

/**
 * @struct RowRange
 * @brief Consecutive rows starting at first.
 */
struct RowRange {
    size_t first {};
    size_t count {};
};

/**
 * @class ForestIndex
 * @brief Parent, child, depth and subtree structure over rows linked by parent index.
 *
 * Built once from one parent per row. A row with no parent is a root. Every
 * parent precedes its children, so the links form a forest and cannot loop.
 * When each subtree also occupies consecutive rows (document preorder) the
 * index reports preorder() and a subtree is the row range
 * [row, row + subtree_size(row)). Such a range is an ordinary slice or
 * one-axis Region of the source, so existing windows, traversals and
 * projections apply to a subtree without any copy.
 *
 * The index stores no row data and does not refer to its source; the caller
 * keeps it matched to the array it was built from.
 *
 * Usage:
 * @code
 * std::vector<uint64_t> parents { ForestIndex::no_parent, 0, 0, 2 };
 * std::string error;
 * auto index = ForestIndex::build(parents, error);
 * auto kids = index->children(0);
 * auto range = index->subtree_range(2);
 * @endcode
 */
class MAYAFLUX_API ForestIndex {
public:
    /** @brief Parent value of a root row. */
    static constexpr uint64_t no_parent = std::numeric_limits<uint64_t>::max();

    /**
     * @brief Build the index from one parent per row.
     * @param parents Parent row of each row, or no_parent for a root.
     * @param error Receives the reason when nullopt is returned.
     * @param error_row Receives the failing row when given.
     * @return The index, or nullopt when a parent does not precede its child.
     */
    [[nodiscard]] static std::optional<ForestIndex> build(
        std::span<const uint64_t> parents, std::string& error,
        uint64_t* error_row = nullptr);

    /** @brief Number of rows. */
    [[nodiscard]] size_t size() const noexcept { return m_parent.size(); }

    /** @brief Parent row, or nullopt for a root or an out of range row. */
    [[nodiscard]] std::optional<uint64_t> parent(size_t row) const noexcept;

    /** @brief Direct children in row order; empty for a leaf or an out of range row. */
    [[nodiscard]] std::span<const uint64_t> children(size_t row) const noexcept;

    /** @brief Rows without a parent, in row order. */
    [[nodiscard]] std::span<const uint64_t> roots() const noexcept { return m_roots; }

    /** @brief Rows in the subtree of row, including row itself; zero when out of range. */
    [[nodiscard]] size_t subtree_size(size_t row) const noexcept;

    /** @brief Distance from the root of row; zero when out of range. */
    [[nodiscard]] uint32_t depth(size_t row) const noexcept;

    /** @brief Whether every subtree occupies consecutive rows. */
    [[nodiscard]] bool preorder() const noexcept { return m_preorder; }

    /**
     * @brief Consecutive rows of the subtree of row.
     * @return The range, or nullopt when the rows are not preorder or row is out of range.
     */
    [[nodiscard]] std::optional<RowRange> subtree_range(size_t row) const noexcept;

private:
    std::vector<uint64_t> m_parent;
    std::vector<uint64_t> m_subtree_size;
    std::vector<uint32_t> m_depth;
    std::vector<uint64_t> m_child_offset;
    std::vector<uint64_t> m_child_row;
    std::vector<uint64_t> m_roots;
    bool m_preorder { true };
};

}
