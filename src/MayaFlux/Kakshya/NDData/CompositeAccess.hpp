#pragma once

#include "Composite.hpp"
#include "MayaFlux/Kakshya/Region/Region.hpp"

namespace MayaFlux::Kakshya {

/**
 * @struct CompositeProjection
 * @brief Owning homogeneous NDData projection of one Composite field.
 *
 * Presence contains one uint8_t per element: 1 for present, 0 for absent.
 * Values for absent elements are zero-filled and must be interpreted using
 * the presence variant. The dimension describes the projected field as a
 * one-dimensional TENSOR_ND value sequence.
 *
 * Usage:
 * @code
 * auto projection = items.to_nddata<double>("score");
 * const auto& scores = std::get<std::vector<double>>(projection->values);
 * const auto& present = std::get<std::vector<uint8_t>>(projection->presence);
 * @endcode
 */
struct CompositeProjection {
    DataVariant values; ///< Homogeneous values of the requested field type.
    DataVariant presence { std::vector<uint8_t> {} }; ///< One byte per value.
    std::vector<DataDimension> dimensions; ///< One CUSTOM axis named for the field.
    DataModality modality { DataModality::TENSOR_ND }; ///< Projection modality.
};

/**
 * @brief Validate and borrow packed Composite-compatible NDData.
 * @param elements DataVariant holding vector<uint8_t> packed elements.
 * @param text DataVariant holding vector<uint8_t> UTF-8 bytes.
 * @param layout Finalized layout for the packed elements.
 * @return Access view, or std::nullopt for incompatible storage or layout.
 * @note This validates storage types, stride, and field extents. Individual
 * text offset bounds are checked when Composite::text() is called.
 */
[[nodiscard]] MAYAFLUX_API std::optional<CompositeAccess> as_composite_access(
    const DataVariant& elements, const DataVariant& text, const CompositeLayout& layout);

/**
 * @class CompositeAccess
 * @brief Validated, non-owning access to packed Composite NDData storage.
 *
 * The element and text stores remain DataVariant instances holding
 * vector<uint8_t>. The layout interprets field offsets within each packed
 * element. No byte or field conversion occurs when creating this view.
 *
 * Usage:
 * @code
 * auto access = items.access();
 * auto first = access->at(0);
 * auto bytes = access->element_bytes();
 * auto dimensions = access->byte_dimensions();
 * @endcode
 *
 * Access to externally owned packed variants:
 * @code
 * auto access = as_composite_access(elements, text, layout);
 * @endcode
 *
 * @note The backing DataVariant objects and layout must outlive this view.
 * Do not replace their variant alternatives or mutate their storage while
 * borrowed views are in use.
 */
class MAYAFLUX_API CompositeAccess {
public:
    /** @brief Layout used to interpret packed elements. */
    [[nodiscard]] const CompositeLayout& layout() const noexcept { return *m_layout; }
    /** @brief Number of packed elements in the current element store. */
    [[nodiscard]] size_t size() const noexcept
    {
        return std::get<std::vector<uint8_t>>(*m_elements).size() / m_layout->stride_bytes();
    }
    /**
     * @brief Borrow an element by index.
     * @param index Zero-based element index.
     * @return Element view, or std::nullopt if out of range.
     */
    [[nodiscard]] std::optional<Composite> at(size_t index) const noexcept;
    /** @brief Borrow all packed element bytes without conversion. */
    [[nodiscard]] std::span<const uint8_t> element_bytes() const noexcept;
    /** @brief Borrow the packed-element DataVariant without conversion. */
    [[nodiscard]] const DataVariant& element_data() const noexcept { return *m_elements; }
    /** @brief Borrow the UTF-8 byte DataVariant without conversion. */
    [[nodiscard]] const DataVariant& text_data() const noexcept { return *m_text; }
    /**
     * @brief Describe packed bytes as [elements, bytes-per-element].
     * @return Two CUSTOM DataDimensions for the uint8_t element store.
     * @note These dimensions do not replace the field schema in layout().
     */
    [[nodiscard]] std::vector<DataDimension> byte_dimensions() const;
    /**
     * @brief Borrow @p count consecutive elements starting at @p start.
     * @param start First element index.
     * @param count Number of elements to select.
     * @return Slice, including an empty slice, or std::nullopt if out of range.
     */
    [[nodiscard]] std::optional<CompositeSlice> slice(size_t start, size_t count) const noexcept;
    /**
     * @brief Borrow the elements selected by a one-dimensional Region.
     * @param region Inclusive element bounds and optional Region attributes.
     * @return Slice retaining the Region, or std::nullopt for invalid bounds.
     */
    [[nodiscard]] std::optional<CompositeSlice> slice(const Region& region) const;

private:
    friend std::optional<CompositeAccess> as_composite_access(
        const DataVariant&, const DataVariant&, const CompositeLayout&);

    CompositeAccess(const DataVariant& elements, const DataVariant& text,
        const CompositeLayout& layout) noexcept
        : m_elements(&elements)
        , m_text(&text)
        , m_layout(&layout)
    {
    }

    const DataVariant* m_elements;
    const DataVariant* m_text;
    const CompositeLayout* m_layout;
};

/**
 * @class CompositeSlice
 * @brief Borrowed consecutive elements, optionally selected by a Region.
 *
 * A slice retains the original Region and its attributes when created from
 * one. Offset/count slices have no Region. Field access still uses the
 * original CompositeLayout and text store, so text offsets do not need to be
 * rewritten. The slice does not own either byte store.
 *
 * Usage:
 * @code
 * auto selection = items.slice(2, 4);
 * auto first = selection->at(0);
 * auto packed = selection->element_bytes();
 * auto scores = selection->to_nddata<double>("score");
 * @endcode
 *
 * Region usage:
 * @code
 * Region region(std::vector<uint64_t> { 2 }, std::vector<uint64_t> { 5 });
 * auto selection = items.slice(region);
 * const Region* source = selection->region();
 * @endcode
 *
 * @note element_bytes() is a zero-copy view. It is not an independent
 * CompositeArray: text offsets still refer to the original text_data().
 */
class MAYAFLUX_API CompositeSlice {
public:
    /** @brief Number of selected elements. */
    [[nodiscard]] size_t size() const noexcept { return m_count; }
    /** @brief First element index in the source access view. */
    [[nodiscard]] size_t start() const noexcept { return m_start; }
    /** @brief Shared layout of all selected elements. */
    [[nodiscard]] const CompositeLayout& layout() const noexcept { return m_access.layout(); }
    /**
     * @brief Borrow a selected element by slice-relative index.
     * @param index Zero-based index within the slice.
     * @return Element view, or std::nullopt if out of range.
     */
    [[nodiscard]] std::optional<Composite> at(size_t index) const noexcept;
    /** @brief Borrow the selected contiguous packed bytes. */
    [[nodiscard]] std::span<const uint8_t> element_bytes() const noexcept;
    /** @brief Borrow the full text store used by selected elements. */
    [[nodiscard]] const DataVariant& text_data() const noexcept { return m_access.text_data(); }
    /** @brief Original Region when selected by Region; otherwise nullptr. */
    [[nodiscard]] const Region* region() const noexcept
    {
        return m_region ? &*m_region : nullptr;
    }

    /**
     * @brief Materialize one exact-type arithmetic field as homogeneous NDData.
     * @tparam T Declared field type and supported DataVariant element type.
     * @param field_name Field to project.
     * @return Owning values, presence mask, and dimension metadata; or
     * std::nullopt if the field is absent from the layout or has another type.
     * Missing values are zero-filled and marked absent in the mask.
     */
    template <typename T>
        requires(ArithmeticData<T> && DataVariantElement<T>)
    [[nodiscard]] std::optional<CompositeProjection> to_nddata(std::string_view field_name) const;

private:
    friend class CompositeAccess;

    CompositeSlice(CompositeAccess access, size_t start, size_t count,
        std::optional<Region> region = std::nullopt)
        : m_access(access)
        , m_start(start)
        , m_count(count)
        , m_region(std::move(region))
    {
    }

    CompositeAccess m_access;
    size_t m_start;
    size_t m_count;
    std::optional<Region> m_region;
};

template <typename T>
    requires(ArithmeticData<T> && DataVariantElement<T>)
std::optional<CompositeProjection> CompositeSlice::to_nddata(std::string_view field_name) const
{
    const auto field_index = layout().find_field(field_name);
    if (!field_index || layout().fields()[*field_index].type != std::type_index(typeid(T)))
        return std::nullopt;

    std::vector<T> values(m_count);
    std::vector<uint8_t> presence(m_count, 0);
    for (size_t i = 0; i < m_count; ++i) {
        const auto element = at(i);
        if (!element)
            return std::nullopt;
        if (auto value = element->get<T>(field_name)) {
            values[i] = *value;
            presence[i] = 1;
        }
    }

    CompositeProjection result;
    result.values = std::move(values);
    result.presence = std::move(presence);
    result.dimensions.emplace_back(std::string(field_name), m_count, 1, DataDimension::Role::CUSTOM);
    return result;
}

template <typename T>
    requires(ArithmeticData<T> && DataVariantElement<T>)
std::optional<CompositeProjection> CompositeArray::to_nddata(std::string_view field_name) const
{
    const auto selection = slice(0, size());
    return selection ? selection->to_nddata<T>(field_name) : std::nullopt;
}

}
