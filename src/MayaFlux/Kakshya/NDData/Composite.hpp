#pragma once

#include "NDData.hpp"

namespace MayaFlux::Kakshya {

class CompositeLayout;
class CompositeArray;

/**
 * @struct CompositeField
 * @brief Description of one named value in a Composite.
 *
 * Numeric fields use their exact C++ scalar type. Text and binary fields
 * store offsets and lengths into separate byte stores. Dynamic fields also
 * store a value kind per element. Compound and list fields refer to nested
 * arrays described by nested_layout. Offsets are assigned when a
 * CompositeArray finalizes its layout.
 */
struct CompositeField {
    std::string name;
    std::type_index type { typeid(void) };
    size_t offset_bytes {};
    size_t size_bytes {};
    std::shared_ptr<const CompositeLayout> nested_layout;
};

namespace detail {
struct CompositeDynamic { };
struct CompositeCompound { };
struct CompositeList { };
}

/** @brief Borrowed value of a present dynamic field; monostate represents NULL. */
using CompositeValue = std::variant<std::monostate, int64_t, double,
    std::string_view, std::span<const uint8_t>>;

/**
 * @class CompositeLayout
 * @brief Shared field description and byte layout for a CompositeArray.
 *
 * Declare fields before constructing an array. Construction finalizes the
 * layout, placing a presence bitmap before the fields in each element.
 * Field order determines byte offsets. A field may be an arithmetic type,
 * std::string, binary bytes, a dynamic value, a compound, or a list.
 * Every field may be absent in an individual element.
 *
 * Usage:
 * @code
 * CompositeLayout layout;
 * layout.add_field<std::string>("name");
 * layout.add_field<uint32_t>("age");
 * layout.add_field<double>("score");
 * CompositeArray items(std::move(layout));
 * @endcode
 *
 * @note stride_bytes() and field offsets are final only after construction
 * of a CompositeArray from this layout.
 */
class MAYAFLUX_API CompositeLayout {
public:
    /**
     * @brief Declare a named arithmetic, text, or binary field.
     * @tparam T Exact stored type; use std::string for text or
     * std::vector<uint8_t> for binary bytes.
     * @param name Nonempty unique field name.
     * @return True if added; false if the name is invalid, duplicated, or
     * the layout has already been finalized.
     */
    template <typename T>
        requires(ArithmeticData<T> || std::same_as<std::remove_cvref_t<T>, std::string>
            || std::same_as<std::remove_cvref_t<T>, std::vector<uint8_t>>)
    bool add_field(std::string name)
    {
        if constexpr (std::same_as<std::remove_cvref_t<T>, std::string>
            || std::same_as<std::remove_cvref_t<T>, std::vector<uint8_t>>) {
            return add_field_impl(std::move(name), typeid(T), 2 * sizeof(uint64_t));
        } else {
            return add_field_impl(std::move(name), typeid(T), sizeof(T));
        }
    }

    /** @brief Declare a field with a per-element NULL, INTEGER, REAL, TEXT, or BLOB kind. */
    bool add_dynamic_field(std::string name)
    {
        return add_field_impl(std::move(name), typeid(detail::CompositeDynamic), 3 * sizeof(uint64_t));
    }

    /**
     * @brief Declare a compound field containing one nested record.
     * @param name Nonempty unique field name.
     * @param layout Layout of the nested record; must contain a field.
     * @return True if added; false for an invalid name, empty layout, or
     * finalized parent layout.
     */
    bool add_compound_field(std::string name, CompositeLayout layout);

    /**
     * @brief Declare a list field containing ordered nested records.
     * @param name Nonempty unique field name.
     * @param item_layout Layout shared by the list's records.
     * @return True if added; false for an invalid name, empty item layout,
     * or finalized parent layout.
     */
    bool add_list_field(std::string name, CompositeLayout item_layout);

    /** @brief Declared fields in element order, with finalized byte offsets. */
    [[nodiscard]] const std::vector<CompositeField>& fields() const noexcept { return m_fields; }

    /**
     * @brief Compare declared field names, kinds, and nested layouts.
     * @param other Layout to compare recursively.
    * @return True when both layouts describe the same record structure.
     */
    [[nodiscard]] bool same_structure(const CompositeLayout& other) const noexcept;

    /** @brief Find a field index by name, or std::nullopt if absent. */
    [[nodiscard]] std::optional<size_t> find_field(std::string_view name) const noexcept;
    /** @brief Bytes per packed element after finalization. */
    [[nodiscard]] size_t stride_bytes() const noexcept { return m_stride_bytes; }
    /** @brief Bytes reserved for the per-element presence bitmap. */
    [[nodiscard]] size_t presence_bytes() const noexcept { return m_presence_bytes; }

private:
    friend class CompositeArray;

    std::vector<CompositeField> m_fields;
    size_t m_stride_bytes {};
    size_t m_presence_bytes {};
    bool m_finalized {};

    bool add_field_impl(std::string name, std::type_index type, size_t size_bytes);
    bool add_nested_field(std::string name, CompositeLayout layout, std::type_index type);
    void finalize();
};

class CompositeAccess;
class CompositeSlice;
class CompositeInsertion;
struct CompositeProjection;
struct Region;

/**
 * @class Composite
 * @brief Read view of one value in a CompositeArray.
 *
 * Borrows a layout, byte stores, and any nested arrays. Arithmetic access
 * copies one value from packed bytes, so the field need not be naturally aligned.
 * Numeric getters require an exact declared type, except dynamic INTEGER
 * and REAL fields which accept int64_t and double respectively. Missing
 * and mismatched fields return std::nullopt.
 *
 * Usage:
 * @code
 * auto item = items.at(0);
 * auto name = item->text("name");
 * auto score = item->get<double>("score");
 * bool has_age = item->has("age");
 * @endcode
 *
 * @note The backing layout and DataVariant objects must outlive this view.
 * Text and byte views may be invalidated by later writes to their stores.
 */
class MAYAFLUX_API Composite {
public:
    /** @brief Layout used to interpret this element's packed bytes. */
    [[nodiscard]] const CompositeLayout& layout() const noexcept;
    /**
     * @brief Whether a named field exists and is present in this element.
     * @param field_name Field to inspect.
     */
    [[nodiscard]] bool has(std::string_view field_name) const noexcept;

    /**
     * @brief Read a present arithmetic field by exact value type.
     * @tparam T Declared arithmetic type, or int64_t/double for a dynamic field.
     * @param field_name Field to read.
     * @return Copied value, or std::nullopt for an absent field or type mismatch.
     */
    template <typename T>
        requires ArithmeticData<T>
    [[nodiscard]] std::optional<T> get(std::string_view field_name) const noexcept;

    /**
     * @brief Read a present std::string field as borrowed UTF-8 bytes.
     * @param field_name Field to read.
     * @return Text view, or std::nullopt for an absent field, type mismatch,
     * or invalid stored text bounds. An empty string is a present value.
     */
    [[nodiscard]] std::optional<std::string_view> text(std::string_view field_name) const noexcept;
    /** @brief Borrow a dynamic value; an absent field returns std::nullopt. */
    [[nodiscard]] std::optional<CompositeValue> value(std::string_view field_name) const noexcept;
    /** @brief Borrow the bytes of a fixed or dynamic BLOB field. */
    [[nodiscard]] std::optional<std::span<const uint8_t>> blob(std::string_view field_name) const noexcept;

    /**
     * @brief Borrow a present compound member as a one-record array.
     * @param field_name Compound field to read.
     * @return Nested array, or nullptr for an absent or mismatched field.
     * @note The parent array owns the returned value.
     */
    [[nodiscard]] const CompositeArray* compound(std::string_view field_name) const noexcept;

    /**
     * @brief Borrow a present ordered list of nested records.
     * @param field_name List field to read.
     * @return Nested array, or nullptr for an absent or mismatched field.
     * An empty array is a present list.
     * @note The parent array owns the returned value.
     */
    [[nodiscard]] const CompositeArray* list(std::string_view field_name) const noexcept;

private:
    friend class CompositeArray;
    friend class CompositeAccess;
    friend class CompositeInsertion;

    Composite(const CompositeLayout& layout, const DataVariant& rows,
        const DataVariant& text, const DataVariant& blob,
        const std::vector<std::shared_ptr<CompositeArray>>* nested, size_t index) noexcept
        : m_layout(&layout)
        , m_rows(&rows)
        , m_text(&text)
        , m_blob(&blob)
        , m_nested(nested)
        , m_index(index)
    {
    }

    const CompositeLayout* m_layout;
    const DataVariant* m_rows;
    const DataVariant* m_text;
    const DataVariant* m_blob;
    const std::vector<std::shared_ptr<CompositeArray>>* m_nested;
    size_t m_index;
};

/**
 * @class CompositeArray
 * @brief Owns a sequence of values that share a CompositeLayout.
 *
 * Owns packed elements, text and binary byte stores, and any nested arrays.
 * The layout carries field names, types, offsets, and stride. Presence bits
 * distinguish absent fields from zero and empty values.
 *
 * Usage:
 * @code
 * CompositeLayout layout;
 * layout.add_field<std::string>("name");
 * layout.add_field<double>("score");
 * CompositeArray items(std::move(layout));
 * auto index = items.append();
 * items.set_text(index, "name", "Ada");
 * items.set(index, "score", 0.95);
 * auto item = items.at(index);
 * @endcode
 *
 * @note Views returned by at(), access(), and slice() borrow this array's
 * stores and layout. Keep the array alive and at the same address while
 * using them. Include CompositeAccess.hpp for slice and projection APIs.
 */
class MAYAFLUX_API CompositeArray {
public:
    /**
     * @brief Finalize a layout and construct an empty array.
     * @param layout Field declarations to take ownership of.
     * @throws std::invalid_argument If the layout has no fields.
     * @throws std::length_error If its packed stride exceeds addressable size.
     */
    explicit CompositeArray(CompositeLayout layout);

    /** @brief Copy the packed rows and all nested values into independent storage. */
    CompositeArray(const CompositeArray& other);

    /** @brief Replace this array with an independent copy of another array. */
    CompositeArray& operator=(const CompositeArray& other);
    CompositeArray(CompositeArray&&) noexcept = default;
    CompositeArray& operator=(CompositeArray&&) noexcept = default;

    /** @brief Finalized layout shared by all elements. */
    [[nodiscard]] const CompositeLayout& layout() const noexcept { return m_layout; }
    /** @brief Number of packed elements. */
    [[nodiscard]] size_t size() const noexcept;
    /** @brief Whether this array contains no elements. */
    [[nodiscard]] bool empty() const noexcept { return size() == 0; }
    /**
     * @brief Borrow an element by index.
     * @param index Zero-based element index.
     * @return Element view, or std::nullopt if out of range.
     */
    [[nodiscard]] std::optional<Composite> at(size_t index) const noexcept;
    /**
     * @brief Borrow this array's packed stores and layout.
     * @return Access view over this array's storage.
     */
    [[nodiscard]] std::optional<CompositeAccess> access() const;
    /**
     * @brief Borrow consecutive elements without copying.
     * @param start First element index.
     * @param count Number of elements, including zero.
     * @return Slice, or std::nullopt if the range exceeds the array.
     */
    [[nodiscard]] std::optional<CompositeSlice> slice(size_t start, size_t count) const;
    /**
     * @brief Borrow an inclusive, one-dimensional Region of elements.
     * @param region Element bounds and optional attributes to retain.
     * @return Slice, or std::nullopt for invalid bounds.
     */
    [[nodiscard]] std::optional<CompositeSlice> slice(const Region& region) const;

    /**
     * @brief Materialize one exact-type numeric field as homogeneous NDData.
     * @tparam T Declared field type and supported DataVariant element type.
     * @param field_name Field to project.
     * @return Values, presence mask, and dimension metadata; std::nullopt if
     * the field is absent from the layout or its type differs from T.
     * @note This copies selected values. Packed storage remains available
     * without copying through access() and element_data().
     */
    template <typename T>
        requires(ArithmeticData<T> && DataVariantElement<T>)
    [[nodiscard]] std::optional<CompositeProjection> to_nddata(std::string_view field_name) const;

    /**
     * @brief Append one zero-initialized element with no fields present.
     * @return Index of the new element.
     * @throws std::length_error If the packed storage cannot grow.
     */
    size_t append();

    /**
     * @brief Write an arithmetic field of an existing element.
     * @tparam T Declared arithmetic type, or int64_t/double for a dynamic field.
     * @param index Element index.
     * @param field_name Field to write.
     * @param value Value to store.
     * @return True on success; false for an invalid index, name, or type.
     */
    template <typename T>
        requires ArithmeticData<T>
    bool set(size_t index, std::string_view field_name, T value)
    {
        if constexpr (std::same_as<T, int64_t> || std::same_as<T, double>) {
            const auto field_index = m_layout.find_field(field_name);
            if (field_index && m_layout.fields()[*field_index].type == typeid(detail::CompositeDynamic)) {
                if constexpr (std::same_as<T, int64_t>)
                    return set_integer(index, field_name, value);
                else
                    return set_real(index, field_name, value);
            }
        }
        const auto field_index = validate_write(index, field_name, typeid(T));
        if (!field_index)
            return false;

        const auto& field = m_layout.fields()[*field_index];
        auto& rows = std::get<std::vector<uint8_t>>(m_rows);
        std::memcpy(rows.data() + index * m_layout.stride_bytes() + field.offset_bytes,
            &value, sizeof(T));
        set_present(index, *field_index, true);
        return true;
    }

    /**
     * @brief Write UTF-8 bytes to a std::string field.
     * @param index Element index.
     * @param field_name Text field to write.
     * @param value UTF-8 bytes to append to the text store.
     * @return True on success; false for an invalid index, name, type, or
     * text storage size. Existing text views may be invalidated.
     */
    bool set_text(size_t index, std::string_view field_name, std::string_view value);
    /** @brief Write a present NULL to a dynamic field. */
    bool set_null(size_t index, std::string_view field_name);
    /** @brief Write binary bytes to a fixed or dynamic BLOB field. */
    bool set_blob(size_t index, std::string_view field_name, std::span<const uint8_t> value);
    /**
     * @brief Copy one record into a compound field.
     * @param index Parent element index.
     * @param field_name Compound field to write.
     * @param value Record with the field's declared nested layout.
     * @return True on success; false for an invalid index, field, or layout.
     */
    bool set_compound(size_t index, std::string_view field_name, const Composite& value);

    /**
     * @brief Copy ordered records into a list field.
     * @param index Parent element index.
     * @param field_name List field to write.
     * @param values Records with the field's declared item layout.
     * @return True on success; false for an invalid index, field, or layout.
     * An empty array becomes a present empty list.
     */
    bool set_list(size_t index, std::string_view field_name, const CompositeArray& values);

    /**
     * @brief Mark a field absent without removing its stored bytes.
     * @param index Element index.
     * @param field_name Field to clear.
     * @return True on success; false for an invalid index or name.
     */
    bool clear(size_t index, std::string_view field_name);

    /** @brief Borrow the full packed-element DataVariant without conversion. */
    [[nodiscard]] const DataVariant& element_data() const noexcept { return m_rows; }
    /** @brief Borrow the UTF-8 byte DataVariant used by text fields. */
    [[nodiscard]] const DataVariant& text_data() const noexcept { return m_text; }
    /** @brief Borrow the binary byte store used by BLOB fields. */
    [[nodiscard]] const DataVariant& blob_data() const noexcept { return m_blob; }

private:
    friend class Composite;
    friend class CompositeInsertion;

    CompositeLayout m_layout;
    DataVariant m_rows { std::vector<uint8_t> {} };
    DataVariant m_text { std::vector<uint8_t> {} };
    DataVariant m_blob { std::vector<uint8_t> {} };
    std::vector<std::shared_ptr<CompositeArray>> m_nested;

    [[nodiscard]] bool is_present(size_t index, size_t field_index) const noexcept;
    void set_present(size_t index, size_t field_index, bool present) noexcept;
    [[nodiscard]] std::optional<size_t> validate_write(
        size_t index, std::string_view field_name, std::type_index type) const;
    [[nodiscard]] std::optional<size_t> validate_dynamic_write(
        size_t index, std::string_view field_name) const;
    bool set_integer(size_t index, std::string_view field_name, int64_t value);
    bool set_real(size_t index, std::string_view field_name, double value);
    bool set_nested(size_t index, std::string_view field_name,
        const CompositeArray& value, std::type_index type);
};

template <typename T>
    requires ArithmeticData<T>
std::optional<T> Composite::get(std::string_view field_name) const noexcept
{
    const auto field_index = m_layout->find_field(field_name);
    if (!field_index || !has(field_name))
        return std::nullopt;

    const auto& field = m_layout->fields()[*field_index];
    if (field.type == typeid(detail::CompositeDynamic)) {
        if constexpr (std::same_as<T, int64_t> || std::same_as<T, double>) {
            const auto dynamic = value(field_name);
            if (dynamic) {
                if (const auto* typed = std::get_if<T>(&*dynamic))
                    return *typed;
            }
        }
        return std::nullopt;
    }
    if (field.type != std::type_index(typeid(T)))
        return std::nullopt;

    const auto& rows = std::get<std::vector<uint8_t>>(*m_rows);
    T value {};

    std::memcpy(&value,
        rows.data() + m_index * m_layout->stride_bytes() + field.offset_bytes,
        sizeof(T));

    return value;
}

}
