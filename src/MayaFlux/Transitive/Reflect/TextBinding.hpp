#pragma once

#include "DescribedBinding.hpp"
#include "TextCodec.hpp"

namespace MayaFlux::Reflect {

/**
 * @brief Decode one authored text value into a described member.
 * @param label Names the owner in the diagnostic.
 */
template <typename T>
bool decode_text_field(std::string_view name, std::string_view source,
    T& value, std::string_view label, std::string& error)
{
    if constexpr (TextDecodable<T>) {
        TextError reason = TextError::Invalid;
        if (TextCodec<T>::decode(source, value, reason))
            return true;
        if (reason == TextError::Cardinality) {
            const char* shape = " numbers in ";
            if constexpr (requires { TextCodec<T>::sequence; })
                shape = " numbers per group in ";
            error = "<" + std::string(label) + "> needs "
                + std::to_string(TextCodec<T>::extent) + shape
                + std::string(name);
        } else if (reason == TextError::Unsupported) {
            error = "<" + std::string(label) + "> has an unsupported "
                + std::string(name) + " " + std::string(source);
        } else {
            error = "<" + std::string(label) + "> has an invalid "
                + std::string(name) + " attribute";
        }
    } else {
        error = "<" + std::string(label) + "> has an unsupported attribute "
            + std::string(name);
    }
    return false;
}

/**
 * @brief Overlay named text values onto a described value.
 * @param fields Callable taking a visitor of (name, text) that returns false to stop.
 * @note A failed overlay may have changed earlier members of the target.
 */
template <Described T, typename Fields>
bool overlay_text_fields(T& target, Fields&& fields,
    std::string_view label, std::string& error)
{
    return overlay_described_fields(target,
        [&](const auto& visit) {
            return fields([&](std::string_view name, std::string_view value) {
                return visit(name, value);
            });
        },
        [&](std::string_view name, std::string_view source, auto& value) {
            return decode_text_field(name, source, value, label, error);
        },
        [&](std::string_view name) {
            error = "<" + std::string(label) + "> has an unsupported attribute "
                + std::string(name);
            return false;
        });
}

}
