#include "ForestIndex.hpp"

namespace MayaFlux::Kakshya {

std::optional<ForestIndex> ForestIndex::build(
    std::span<const uint64_t> parents, std::string& error, uint64_t* error_row)
{
    const size_t count = parents.size();
    for (size_t row = 0; row < count; ++row) {
        if (parents[row] != no_parent && parents[row] >= row) {
            if (error_row)
                *error_row = row;
            error = "row parent must precede its child";
            return std::nullopt;
        }
    }

    ForestIndex index;
    index.m_parent.assign(parents.begin(), parents.end());

    index.m_subtree_size.assign(count, 1);
    for (size_t row = count; row-- > 0;) {
        if (parents[row] != no_parent)
            index.m_subtree_size[parents[row]] += index.m_subtree_size[row];
    }

    index.m_depth.assign(count, 0);
    index.m_child_offset.assign(count + 1, 0);
    for (size_t row = 0; row < count; ++row) {
        if (parents[row] == no_parent) {
            index.m_roots.push_back(row);
        } else {
            index.m_depth[row] = index.m_depth[parents[row]] + 1;
            ++index.m_child_offset[parents[row] + 1];
        }
    }
    for (size_t row = 0; row < count; ++row)
        index.m_child_offset[row + 1] += index.m_child_offset[row];

    index.m_child_row.resize(index.m_child_offset[count]);
    std::vector<uint64_t> fill(index.m_child_offset.begin(), index.m_child_offset.end() - 1);
    for (size_t row = 0; row < count; ++row) {
        if (parents[row] != no_parent)
            index.m_child_row[fill[parents[row]]++] = row;
    }

    std::vector<uint64_t> expected(count);
    for (size_t row = 0; row < count; ++row)
        expected[row] = row + 1;
    uint64_t expected_root = 0;
    for (size_t row = 0; row < count; ++row) {
        uint64_t& next = parents[row] == no_parent ? expected_root : expected[parents[row]];
        if (next != row) {
            index.m_preorder = false;
            break;
        }
        next += index.m_subtree_size[row];
    }
    return index;
}

std::optional<uint64_t> ForestIndex::parent(size_t row) const noexcept
{
    if (row >= m_parent.size() || m_parent[row] == no_parent)
        return std::nullopt;
    return m_parent[row];
}

std::span<const uint64_t> ForestIndex::children(size_t row) const noexcept
{
    if (row >= m_parent.size())
        return {};
    const uint64_t begin = m_child_offset[row];
    return { m_child_row.data() + begin, static_cast<size_t>(m_child_offset[row + 1] - begin) };
}

size_t ForestIndex::subtree_size(size_t row) const noexcept
{
    return row < m_subtree_size.size() ? static_cast<size_t>(m_subtree_size[row]) : 0;
}

uint32_t ForestIndex::depth(size_t row) const noexcept
{
    return row < m_depth.size() ? m_depth[row] : 0;
}

std::optional<RowRange> ForestIndex::subtree_range(size_t row) const noexcept
{
    if (!m_preorder || row >= m_parent.size())
        return std::nullopt;
    return RowRange { .first = row, .count = static_cast<size_t>(m_subtree_size[row]) };
}

}
