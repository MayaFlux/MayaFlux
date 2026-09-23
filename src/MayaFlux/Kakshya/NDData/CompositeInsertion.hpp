#pragma once

#include "Composite.hpp"

namespace MayaFlux::Kakshya {

/**
 * @class CompositeInsertion
 * @brief Mutable insertion adapter for a CompositeArray.
 *
 * Appends empty elements, writes fields by name, and copies compatible
 * Composite values from another array. Copying an element also copies its
 * present text values and rebases their offsets into the destination's text
 * store. The adapter borrows its destination array and does not own storage.
 *
 * Usage:
 * @code
 * CompositeInsertion insert(items);
 * auto index = insert.append();
 * insert.set_text(index, "name", "Ada");
 * insert.set(index, "score", 0.95);
 * @endcode
 *
 * Copying between arrays with the same finalized layout:
 * @code
 * CompositeInsertion destination_insert(destination);
 * auto source = items.at(0);
 * auto copied_index = destination_insert.append(*source);
 * @endcode
 *
 * @note The destination CompositeArray must outlive this adapter. Field
 * writes require exact declared types; they never coerce numeric values.
 */
class MAYAFLUX_API CompositeInsertion {
public:
    /**
     * @brief Borrow the array into which elements and fields will be written.
     * @param array Destination array, which must outlive this adapter.
     */
    explicit CompositeInsertion(CompositeArray& array) noexcept
        : m_array(&array)
    {
    }

    /**
     * @brief Append an empty element.
     * @return Index of the new element.
     * @throws std::length_error If the packed storage cannot grow.
     */
    [[nodiscard]] size_t append() { return m_array->append(); }
    /**
     * @brief Copy one Composite with a matching layout into this array.
     * @param source Element to copy, including its present text fields.
     * @return New element index, or std::nullopt for incompatible layout,
     * invalid source text bounds, or insufficient storage capacity.
     */
    [[nodiscard]] std::optional<size_t> append(const Composite& source);

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
        return m_array->set(index, field_name, value);
    }

    /**
     * @brief Write UTF-8 bytes to a std::string field.
     * @param index Element index.
     * @param field_name Text field to write.
     * @param value UTF-8 bytes to append to the destination text store.
     * @return True on success; false for an invalid index, name, type, or
     * text storage size.
     */
    bool set_text(size_t index, std::string_view field_name, std::string_view value)
    {
        return m_array->set_text(index, field_name, value);
    }

    /**
     * @brief Mark a named field absent in an existing element.
     * @param index Element index.
     * @param field_name Field to clear.
     * @return True on success; false for an invalid index or field name.
     */
    bool clear(size_t index, std::string_view field_name)
    {
        return m_array->clear(index, field_name);
    }

private:
    CompositeArray* m_array;
};

}
