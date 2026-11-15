#pragma once

#include "XmlReader.hpp"

#include "MayaFlux/Transitive/Reflect/RecordBinding.hpp"

namespace MayaFlux::Reflect {

template <>
struct RecordAccess<IO::XmlElementView> {
    using Source = IO::XmlElementView;

    static std::string_view name(const Source& source) { return source.name(); }
    static uint64_t location(const Source& source) { return source.row(); }

    static std::optional<std::string_view> attribute(const Source& source,
        std::string_view name)
    {
        return source.attribute(name);
    }

    template <typename Visitor>
    static bool for_each_attribute(const Source& source, Visitor&& visit)
    {
        return source.for_each_field([&](const IO::XmlFieldView& field) {
            return field.kind == IO::XmlFieldKind::Element || visit(field.name, field.value);
        });
    }

    template <typename Visitor>
    static bool for_each_child(const Source& source, Visitor&& visit)
    {
        return source.for_each_field([&](const IO::XmlFieldView& field) {
            return field.kind == IO::XmlFieldKind::Attribute || visit(*field.element);
        });
    }
};

}
