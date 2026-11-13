#pragma once

#include "XmlReader.hpp"
#include "XmlTextCodec.hpp"

#include "MayaFlux/Transitive/Reflect/DescribedBinding.hpp"

namespace MayaFlux::IO {

template <typename T>
bool decode_xml_attribute(std::string_view name, std::string_view source,
    T& value, std::string_view label, std::string& error)
{
    if constexpr (XmlTextDecodable<T>) {
        XmlTextError reason = XmlTextError::Invalid;
        if (XmlTextCodec<T>::decode(source, value, reason))
            return true;
        if (reason == XmlTextError::Cardinality) {
            error = "<" + std::string(label) + "> needs "
                + std::to_string(XmlTextCodec<T>::extent) + " numbers in "
                + std::string(name);
        } else if (reason == XmlTextError::Unsupported) {
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

/** @brief Overlay XML text attributes onto a described value. */
template <Reflect::Described T, typename Attributes>
bool overlay_xml_attributes(T& target, Attributes&& attributes,
    std::string_view label, std::string& error)
{
    return Reflect::overlay_described_fields(target,
        [&](const auto& visit) {
            return attributes([&](std::string_view name, std::string_view value) {
                return visit(name, value);
            });
        },
        [&](std::string_view name, std::string_view source, auto& value) {
            return decode_xml_attribute(name, source, value, label, error);
        },
        [&](std::string_view name) {
            error = "<" + std::string(label) + "> has an unsupported attribute "
                + std::string(name);
            return false;
        });
}

/** @brief Overlay an XML element's attributes onto a described value. */
template <Reflect::Described T>
bool overlay_xml_attributes(T& target, const XmlElementView& element,
    std::string& error)
{
    return overlay_xml_attributes(target,
        [&](const auto& visit) {
            return element.for_each_field([&](const XmlFieldView& field) {
                return field.kind == XmlFieldKind::Element
                    || visit(field.name, field.value);
            });
        }, element.name(), error);
}

}
