#pragma once

#include "NDData.hpp"

namespace MayaFlux::Kakshya {

/**
 * @struct CompositeField
 * @brief Description of one named value in a Composite.
 *
 * Numeric fields use their exact C++ scalar type. A std::string field stores
 * an offset and length into the array's UTF-8 byte storage, not a string
 * object in the fixed-width element buffer. Offsets are assigned when a
 * CompositeArray finalizes its layout.
 */
struct CompositeField {
    std::string name;
    std::type_index type { typeid(void) };
    size_t offset_bytes {};
    size_t size_bytes {};
};

/**
 * @class CompositeLayout
 * @brief Shared field description and byte layout for a CompositeArray.
 *
 * Declare fields before constructing an array. Construction finalizes the
 * layout, placing a presence bitmap before the fields in each element.
 * Field order determines byte offsets. A field may be an arithmetic type or
 * std::string; text uses the array's separate UTF-8 byte storage.
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
     * @brief Declare a named arithmetic or UTF-8 text field.
     * @tparam T Exact stored type; use std::string for text.
     * @param name Nonempty unique field name.
     * @return True if added; false if the name is invalid, duplicated, or
     * the layout has already been finalized.
     */
    template <typename T>
        requires(std::is_arithmetic_v<T> || std::same_as<std::remove_cvref_t<T>, std::string>)
    bool add_field(std::string name)
    {
        if constexpr (std::same_as<std::remove_cvref_t<T>, std::string>) {
            return add_field_impl(std::move(name), typeid(std::string), 2 * sizeof(uint64_t));
        } else {
            return add_field_impl(std::move(name), typeid(T), sizeof(T));
        }
    }

    /** @brief Declared fields in element order, with finalized byte offsets. */
    [[nodiscard]] const std::vector<CompositeField>& fields() const noexcept { return m_fields; }
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
    void finalize();
};

class CompositeArray;
class CompositeAccess;
class CompositeSlice;
class CompositeInsertion;
struct CompositeProjection;
struct Region;

/**
 * @class Composite
 * @brief Read view of one value in a CompositeArray.
 *
 * Borrows a layout and two DataVariant stores. Arithmetic access copies one
 * value from packed bytes, so the field need not be naturally aligned.
 * Numeric getters require an exact declared type. Missing and mismatched
 * fields return std::nullopt.
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
 * A string_view returned by text() may be invalidated by later text writes.
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
     * @brief Read a present arithmetic field by exact declared type.
     * @tparam T Arithmetic type declared through CompositeLayout::add_field().
     * @param field_name Field to read.
     * @return Copied value, or std::nullopt for an absent field or type mismatch.
     */
    template <typename T>
        requires std::is_arithmetic_v<T>
    [[nodiscard]] std::optional<T> get(std::string_view field_name) const noexcept;

    /**
     * @brief Read a present std::string field as borrowed UTF-8 bytes.
     * @param field_name Field to read.
     * @return Text view, or std::nullopt for an absent field, type mismatch,
     * or invalid stored text bounds. An empty string is a present value.
     */
    [[nodiscard]] std::optional<std::string_view> text(std::string_view field_name) const noexcept;

private:
    friend class CompositeArray;
    friend class CompositeAccess;
    friend class CompositeInsertion;

    Composite(const CompositeLayout& layout, const DataVariant& rows,
        const DataVariant& text, size_t index) noexcept
        : m_layout(&layout)
        , m_rows(&rows)
        , m_text(&text)
        , m_index(index)
    {
    }

    const CompositeLayout* m_layout;
    const DataVariant* m_rows;
    const DataVariant* m_text;
    size_t m_index;
};

/**
 * @class CompositeArray
 * @brief Owns a sequence of values that share a CompositeLayout.
 *
 * Owns two uint8_t DataVariants: fixed-stride packed elements and variable-
 * length UTF-8 text. The layout carries field names, types, offsets, and
 * stride; the presence bitmap distinguishes absent fields from zero or
 * empty values. Composite is not a SignalSourceContainer and has no
 * processing state.
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
     * @brief Validate and borrow the packed NDData stores and layout.
     * @return Access view, or std::nullopt if storage is incompatible.
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
        requires(std::is_arithmetic_v<T> && DataVariantElement<T>)
    [[nodiscard]] std::optional<CompositeProjection> to_nddata(std::string_view field_name) const;

    /**
     * @brief Append one zero-initialized element with no fields present.
     * @return Index of the new element.
     * @throws std::length_error If the packed storage cannot grow.
     */
    size_t append();

    /**
     * @brief Write an arithmetic field of an existing element.
     * @tparam T Exact type declared for the field.
     * @param index Element index.
     * @param field_name Field to write.
     * @param value Value to store.
     * @return True on success; false for an invalid index, name, or type.
     */
    template <typename T>
        requires std::is_arithmetic_v<T>
    bool set(size_t index, std::string_view field_name, T value)
    {
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

private:
    friend class Composite;
    friend class CompositeInsertion;

    CompositeLayout m_layout;
    DataVariant m_rows { std::vector<uint8_t> {} };
    DataVariant m_text { std::vector<uint8_t> {} };

    [[nodiscard]] bool is_present(size_t index, size_t field_index) const noexcept;
    void set_present(size_t index, size_t field_index, bool present) noexcept;
    [[nodiscard]] std::optional<size_t> validate_write(
        size_t index, std::string_view field_name, std::type_index type) const;
};

template <typename T>
    requires std::is_arithmetic_v<T>
std::optional<T> Composite::get(std::string_view field_name) const noexcept
{
    const auto field_index = m_layout->find_field(field_name);
    if (!field_index || !has(field_name))
        return std::nullopt;

    const auto& field = m_layout->fields()[*field_index];
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
