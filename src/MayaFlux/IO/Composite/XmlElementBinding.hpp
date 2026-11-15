#pragma once

#include "XmlReader.hpp"

#include "MayaFlux/Transitive/Reflect/Mirror.hpp"
#include "MayaFlux/Transitive/Reflect/TextBinding.hpp"

namespace MayaFlux::IO {

template <Reflect::Described T>
bool overlay_xml_element(T& target, const XmlElementView& element,
    std::string& error, uint64_t* error_row = nullptr);

/** @brief Decode one XML child element into a described member. */
template <typename T>
struct XmlElementCodec { };

template <Reflect::Described T>
struct XmlElementCodec<T> {
    static bool decode(const XmlElementView& element, T& value,
        std::string& error, uint64_t* error_row)
    {
        return overlay_xml_element(value, element, error, error_row);
    }
};

template <Reflect::Described T>
struct XmlElementCodec<std::vector<T>> {
    static bool decode(const XmlElementView& element, std::vector<T>& value,
        std::string& error, uint64_t* error_row)
    {
        T item {};
        if (!overlay_xml_element(item, element, error, error_row))
            return false;
        value.push_back(std::move(item));
        return true;
    }
};

template <Reflect::Described T>
struct XmlElementCodec<std::optional<T>> {
    static bool decode(const XmlElementView& element, std::optional<T>& value,
        std::string& error, uint64_t* error_row)
    {
        T item = value.value_or(T {});
        if (!overlay_xml_element(item, element, error, error_row))
            return false;
        value = std::move(item);
        return true;
    }
};

template <typename T>
concept XmlElementDecodable = requires(const XmlElementView& element, T& value,
    std::string& error, uint64_t* error_row) {
    { XmlElementCodec<T>::decode(element, value, error, error_row) }
        -> std::same_as<bool>;
};

/** @brief Overlay XML attributes and same-named children onto a described value. */
template <Reflect::Described T>
bool overlay_xml_element(T& target, const XmlElementView& element,
    std::string& error, uint64_t* error_row)
{
    if (error_row)
        *error_row = element.row();
    return Reflect::overlay_described_fields(target,
        [&](const auto& visit) {
            return element.for_each_field([&](const XmlFieldView& field) {
                if (error_row)
                    *error_row = field.row;
                return visit(field.name, field);
            });
        },
        [&](std::string_view name, const XmlFieldView& field, auto& value) {
            using Value = std::remove_cvref_t<decltype(value)>;
            if (field.kind == XmlFieldKind::Attribute)
                return Reflect::decode_text_field(name, field.value, value, element.name(), error);
            if constexpr (XmlElementDecodable<Value>)
                return XmlElementCodec<Value>::decode(*field.element, value, error, error_row);
            error = "<" + std::string(element.name()) + "> has an unsupported child <"
                + std::string(name) + ">";
            return false;
        },
        [&](std::string_view name) {
            error = "<" + std::string(element.name()) + "> has an unsupported field "
                + std::string(name);
            return false;
        });
}

}
