#pragma once

#include "MayaFlux/Kakshya/NDData/CompositeDescribe.hpp"
#include "MayaFlux/Transitive/Reflect/RecordBinding.hpp"

namespace MayaFlux::IO {

/**
 * @brief Decode the same-named children of a source element into typed Composite rows.
 *
 * Each child named child_name is read through the binding and stored as one
 * row of the layout derived from T. The source can be any type with a
 * RecordAccess specialization, so the result does not depend on the file format.
 *
 * @param binding Configured binding for T.
 * @param parent Source element whose children are read.
 * @param child_name Name of the children that hold one record each.
 * @param error Receives the reason when nullopt is returned.
 * @param error_location Receives the position of the failing element when given.
 * @return The rows, empty when no child matches, or nullopt on the first failure.
 */
template <Reflect::Described T, typename Source, typename Access>
[[nodiscard]] std::optional<Kakshya::CompositeArray> collect_records(
    const Reflect::RecordBinding<T, Source, Access>& binding,
    const Source& parent, std::string_view child_name,
    std::string& error, uint64_t* error_location = nullptr)
{
    Kakshya::CompositeArray rows(Kakshya::described_composite_layout<T>());
    Kakshya::CompositeInsertion insertion(rows);
    const bool traversed = Access::for_each_child(parent, [&](const Source& child) {
        if (Access::name(child) != child_name)
            return true;
        const auto record = binding.read(child, error, error_location);
        if (!record)
            return false;
        return Kakshya::write_described(insertion, insertion.append(), *record, error);
    });
    if (!traversed)
        return std::nullopt;
    return rows;
}

/**
 * @brief Decode every row of a typed Composite array into values.
 *
 * Works for rows from any source whose layout is the one derived from T:
 * a delimited file loaded with that layout, or rows made by collect_records.
 *
 * @param rows Array whose layout matches described_composite_layout of T.
 * @param error Receives the reason when nullopt is returned.
 */
template <Reflect::Described T>
[[nodiscard]] std::optional<std::vector<T>> records_of(
    const Kakshya::CompositeArray& rows, std::string& error)
{
    if (!rows.layout().same_structure(Kakshya::described_composite_layout<T>())) {
        error = "Composite layout does not match the described type";
        return std::nullopt;
    }
    std::vector<T> values;
    values.reserve(rows.size());
    for (size_t i = 0; i < rows.size(); ++i) {
        const auto row = rows.at(i);
        T value {};
        if (!row || !Kakshya::overlay_described(*row, value, error))
            return std::nullopt;
        values.push_back(std::move(value));
    }
    return values;
}

}
