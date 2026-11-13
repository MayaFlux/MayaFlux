#pragma once

#include "CompositeInsertion.hpp"

#include "MayaFlux/Transitive/Reflect/DescribedBinding.hpp"
#include "MayaFlux/Transitive/Reflect/Mirror.hpp"

namespace MayaFlux::Kakshya {

template <Reflect::Described T>
[[nodiscard]] CompositeLayout described_composite_layout();

template <Reflect::Described T>
bool write_described(CompositeInsertion& insertion, size_t row, const T& value,
    std::string& error);

template <Reflect::Described T>
bool overlay_described(const Composite& source, T& target, std::string& error);

/**
 * @brief Mapping of one described member to a native Composite field.
 * Specialize for application types that need another exact storage mapping.
 */
template <typename T>
struct CompositeFieldCodec {
    static_assert(always_false_v<T>, "No Composite field mapping for this described member type");
};

namespace detail {

inline bool described_field_error(std::string_view name, std::string& error)
{
    error = "Cannot bind described field '" + std::string(name) + "'";
    return false;
}

}

template <typename T>
    requires(ArithmeticData<T> || GlmType<T>)
struct CompositeFieldCodec<T> {
    static bool add(CompositeLayout& layout, std::string name)
    {
        return layout.add_field<T>(std::move(name));
    }

    static bool write(CompositeInsertion& insertion, size_t row,
        std::string_view name, T value, std::string& error)
    {
        return insertion.set(row, name, value)
            || detail::described_field_error(name, error);
    }

    static bool read(const Composite& source, std::string_view name,
        T& value, std::string& error)
    {
        const auto decoded = source.get<T>(name);
        if (!decoded)
            return detail::described_field_error(name, error);
        value = *decoded;
        return true;
    }
};

template <>
struct CompositeFieldCodec<std::string> {
    static bool add(CompositeLayout& layout, std::string name)
    {
        return layout.add_field<std::string>(std::move(name));
    }

    static bool write(CompositeInsertion& insertion, size_t row,
        std::string_view name, const std::string& value, std::string& error)
    {
        return insertion.set_text(row, name, value)
            || detail::described_field_error(name, error);
    }

    static bool read(const Composite& source, std::string_view name,
        std::string& value, std::string& error)
    {
        const auto decoded = source.text(name);
        if (!decoded)
            return detail::described_field_error(name, error);
        value = *decoded;
        return true;
    }
};

template <>
struct CompositeFieldCodec<std::vector<uint8_t>> {
    static bool add(CompositeLayout& layout, std::string name)
    {
        return layout.add_field<std::vector<uint8_t>>(std::move(name));
    }

    static bool write(CompositeInsertion& insertion, size_t row,
        std::string_view name, const std::vector<uint8_t>& value, std::string& error)
    {
        return insertion.set_blob(row, name, value)
            || detail::described_field_error(name, error);
    }

    static bool read(const Composite& source, std::string_view name,
        std::vector<uint8_t>& value, std::string& error)
    {
        const auto decoded = source.blob(name);
        if (!decoded)
            return detail::described_field_error(name, error);
        value.assign(decoded->begin(), decoded->end());
        return true;
    }
};

template <typename T>
    requires std::is_enum_v<T>
struct CompositeFieldCodec<T> {
    using Underlying = std::underlying_type_t<T>;

    static bool add(CompositeLayout& layout, std::string name)
    {
        return layout.add_field<Underlying>(std::move(name));
    }

    static bool write(CompositeInsertion& insertion, size_t row,
        std::string_view name, T value, std::string& error)
    {
        return CompositeFieldCodec<Underlying>::write(
            insertion, row, name, static_cast<Underlying>(value), error);
    }

    static bool read(const Composite& source, std::string_view name,
        T& value, std::string& error)
    {
        Underlying decoded {};
        if (!CompositeFieldCodec<Underlying>::read(source, name, decoded, error))
            return false;
        value = static_cast<T>(decoded);
        return true;
    }
};

template <Reflect::Described T>
struct CompositeFieldCodec<T> {
    static bool add(CompositeLayout& layout, std::string name)
    {
        return layout.add_compound_field(std::move(name), described_composite_layout<T>());
    }

    static bool write(CompositeInsertion& insertion, size_t row,
        std::string_view name, const T& value, std::string& error)
    {
        CompositeArray nested(described_composite_layout<T>());
        CompositeInsertion nested_insertion(nested);
        const size_t nested_row = nested_insertion.append();
        if (!write_described(nested_insertion, nested_row, value, error))
            return false;
        return insertion.set_compound(row, name, *nested.at(nested_row))
            || detail::described_field_error(name, error);
    }

    static bool read(const Composite& source, std::string_view name,
        T& value, std::string& error)
    {
        const auto* nested = source.compound(name);
        static const CompositeLayout expected = described_composite_layout<T>();
        if (!nested || nested->size() != 1
            || !nested->layout().same_structure(expected))
            return detail::described_field_error(name, error);
        return overlay_described(*nested->at(0), value, error);
    }
};

template <Reflect::Described T>
struct CompositeFieldCodec<std::vector<T>> {
    static bool add(CompositeLayout& layout, std::string name)
    {
        return layout.add_list_field(std::move(name), described_composite_layout<T>());
    }

    static bool write(CompositeInsertion& insertion, size_t row,
        std::string_view name, const std::vector<T>& value, std::string& error)
    {
        CompositeArray nested(described_composite_layout<T>());
        CompositeInsertion nested_insertion(nested);
        for (const T& item : value) {
            if (!write_described(nested_insertion, nested_insertion.append(), item, error))
                return false;
        }
        return insertion.set_list(row, name, nested)
            || detail::described_field_error(name, error);
    }

    static bool read(const Composite& source, std::string_view name,
        std::vector<T>& value, std::string& error)
    {
        const auto* nested = source.list(name);
        static const CompositeLayout expected = described_composite_layout<T>();
        if (!nested || !nested->layout().same_structure(expected))
            return detail::described_field_error(name, error);
        std::vector<T> decoded;
        decoded.reserve(nested->size());
        for (size_t i = 0; i < nested->size(); ++i) {
            T item {};
            const auto record = nested->at(i);
            if (!record || !overlay_described(*record, item, error))
                return false;
            decoded.push_back(std::move(item));
        }
        value = std::move(decoded);
        return true;
    }
};

template <typename T>
    requires(!Reflect::Described<T> && !std::same_as<T, uint8_t>)
struct CompositeFieldCodec<std::vector<T>> {
    static CompositeLayout item_layout()
    {
        CompositeLayout layout;
        CompositeFieldCodec<T>::add(layout, "value");
        return layout;
    }

    static bool add(CompositeLayout& layout, std::string name)
    {
        return layout.add_list_field(std::move(name), item_layout());
    }

    static bool write(CompositeInsertion& insertion, size_t row,
        std::string_view name, const std::vector<T>& value, std::string& error)
    {
        CompositeArray nested(item_layout());
        CompositeInsertion nested_insertion(nested);
        for (const T& item : value) {
            if (!CompositeFieldCodec<T>::write(
                    nested_insertion, nested_insertion.append(), "value", item, error))
                return false;
        }
        return insertion.set_list(row, name, nested)
            || detail::described_field_error(name, error);
    }

    static bool read(const Composite& source, std::string_view name,
        std::vector<T>& value, std::string& error)
    {
        const auto* nested = source.list(name);
        static const CompositeLayout expected = item_layout();
        if (!nested || !nested->layout().same_structure(expected))
            return detail::described_field_error(name, error);
        std::vector<T> decoded;
        decoded.reserve(nested->size());
        for (size_t i = 0; i < nested->size(); ++i) {
            const auto record = nested->at(i);
            if (!record)
                return detail::described_field_error(name, error);
            T item {};
            if (record->has("value")) {
                if (!CompositeFieldCodec<T>::read(*record, "value", item, error))
                    return false;
            } else if constexpr (!Reflect::is_optional_v<T>) {
                return detail::described_field_error(name, error);
            }
            decoded.push_back(std::move(item));
        }
        value = std::move(decoded);
        return true;
    }
};

template <typename T>
struct CompositeFieldCodec<std::optional<T>> {
    static bool add(CompositeLayout& layout, std::string name)
    {
        return CompositeFieldCodec<T>::add(layout, std::move(name));
    }

    static bool write(CompositeInsertion& insertion, size_t row,
        std::string_view name, const std::optional<T>& value, std::string& error)
    {
        if (!value)
            return insertion.clear(row, name)
                || detail::described_field_error(name, error);
        return CompositeFieldCodec<T>::write(insertion, row, name, *value, error);
    }

    static bool read(const Composite& source, std::string_view name,
        std::optional<T>& value, std::string& error)
    {
        if (value)
            return CompositeFieldCodec<T>::read(source, name, *value, error);
        T decoded {};
        if (!CompositeFieldCodec<T>::read(source, name, decoded, error))
            return false;
        value = std::move(decoded);
        return true;
    }
};

/** @brief Derive a Composite layout from a generated member description. */
template <Reflect::Described T>
CompositeLayout described_composite_layout()
{
    CompositeLayout layout;
    bool added = true;
    std::apply([&](const auto&... members) {
        ((added = added && CompositeFieldCodec<typename std::remove_cvref_t<decltype(members)>::value_type>
                ::add(layout, std::string(members.name))), ...);
    }, Reflect::Generated::Description<T>::members());
    if (!added)
        throw std::invalid_argument("Cannot derive Composite layout from description");
    return layout;
}

/**
 * @brief Write all described members into an existing Composite row.
 * @note A failed write may leave earlier members present in the row.
 */
template <Reflect::Described T>
bool write_described(CompositeInsertion& insertion, size_t row, const T& value,
    std::string& error)
{
    error.clear();
    bool written = true;
    std::apply([&](const auto&... members) {
        ((written = written && CompositeFieldCodec<typename std::remove_cvref_t<decltype(members)>::value_type>
                ::write(insertion, row, members.name, members.get(value), error)), ...);
    }, Reflect::Generated::Description<T>::members());
    return written;
}

/**
 * @brief Apply present Composite fields to a described value, preserving other members.
 * @note A failed overlay may have applied earlier fields to the target.
 */
template <Reflect::Described T>
bool overlay_described(const Composite& source, T& target, std::string& error)
{
    error.clear();
    return Reflect::overlay_described_fields(target,
        [&](const auto& visit) {
            return source.for_each_present_field([&](const CompositeField& field) {
                return visit(field.name, field);
            });
        },
        [&](std::string_view, const CompositeField& field, auto& value) {
            using Value = std::remove_cvref_t<decltype(value)>;
            return CompositeFieldCodec<Value>::read(source, field.name, value, error);
        },
        [&](std::string_view name) {
            error = "Unknown described field '" + std::string(name) + "'";
            return false;
        });
}

}
