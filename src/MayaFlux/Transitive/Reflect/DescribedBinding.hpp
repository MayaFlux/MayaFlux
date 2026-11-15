#pragma once

#include "Describe.hpp"

namespace MayaFlux::Reflect {

/** @brief Visit a generated member by its authored name. */
template <Described T, typename Visitor>
bool visit_described_member(std::string_view name, Visitor&& visitor)
{
    return std::apply([&](const auto&... members) {
        return ((members.name == name && (visitor(members), true)) || ...);
    }, Generated::Description<T>::members());
}

/**
 * @brief Apply present named fields to a described value.
 * @note A failed overlay may have changed earlier members of the target.
 */
template <Described T, typename Fields, typename Decode, typename Unknown>
bool overlay_described_fields(T& target, Fields&& fields, Decode&& decode,
    Unknown&& unknown)
{
    return fields([&](std::string_view name, const auto& field) {
        bool decoded = false;
        const bool found = visit_described_member<T>(name, [&](const auto& member) {
            decoded = decode(name, field, member.get(target));
        });
        return found ? decoded : unknown(name);
    });
}

}
