#include "CompositeForest.hpp"

#include "CompositeInsertion.hpp"

namespace MayaFlux::Kakshya {

std::optional<ForestIndex> index_composite_forest(
    const CompositeArray& rows, std::string_view parent_field,
    std::string& error, uint64_t* error_row)
{
    const auto field = rows.layout().find_field(parent_field);
    if (!field || rows.layout().fields()[*field].type != std::type_index(typeid(uint64_t))) {
        error = "Composite has no uint64_t field " + std::string(parent_field);
        return std::nullopt;
    }

    const std::string name(parent_field);
    return build_forest_index(rows,
        [&name](const Composite& row) { return row.get<uint64_t>(name); },
        error, error_row);
}

std::optional<Region> forest_subtree_region(const ForestIndex& index, size_t row)
{
    const auto range = index.subtree_range(row);
    if (!range)
        return std::nullopt;
    return Region(std::vector<uint64_t> { range->first },
        std::vector<uint64_t> { range->first + range->count - 1 });
}

std::optional<CompositeArray> copy_forest_subtree(
    const CompositeArray& rows, const ForestIndex& index,
    std::string_view parent_field, size_t row, std::string& error)
{
    const auto field = rows.layout().find_field(parent_field);
    if (!field || rows.layout().fields()[*field].type != std::type_index(typeid(uint64_t))) {
        error = "Composite has no uint64_t field " + std::string(parent_field);
        return std::nullopt;
    }
    const auto range = index.subtree_range(row);
    if (!range || index.size() != rows.size()) {
        error = "subtree rows are not preorder, the row is out of range, or the index does not match";
        return std::nullopt;
    }

    CompositeArray copy(rows.layout());
    CompositeInsertion insertion(copy);
    for (size_t source_row = range->first; source_row < range->first + range->count; ++source_row) {
        const auto source = rows.at(source_row);
        const auto appended = source ? insertion.append(*source) : std::nullopt;
        if (!appended) {
            error = "Could not copy a subtree row";
            return std::nullopt;
        }
        const auto parent = index.parent(source_row);
        const bool written = source_row == range->first || !parent
            ? insertion.clear(*appended, parent_field)
            : insertion.set(*appended, parent_field, static_cast<uint64_t>(*parent - range->first));
        if (!written) {
            error = "Could not rebase a subtree parent link";
            return std::nullopt;
        }
    }
    return copy;
}

std::shared_ptr<const CompositeForest> CompositeForest::build(
    CompositeArray rows, std::string_view parent_field,
    std::string& error, uint64_t* error_row)
{
    auto index = index_composite_forest(rows, parent_field, error, error_row);
    if (!index)
        return nullptr;
    return std::shared_ptr<const CompositeForest>(
        new CompositeForest(std::move(rows), std::move(*index), std::string(parent_field)));
}

}
