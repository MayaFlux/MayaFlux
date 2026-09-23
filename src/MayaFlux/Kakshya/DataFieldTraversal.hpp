#pragma once

namespace MayaFlux::Kakshya {

/**
 * @brief Array with optional-like element and contiguous slice access.
 * @tparam Array Array type whose elements and slices can be borrowed.
 */
template <typename Array>
concept FieldArray = requires(const Array& array, size_t index, size_t count) {
    { array.size() } -> std::convertible_to<size_t>;
    { static_cast<bool>(array.at(index)) } -> std::same_as<bool>;
    *array.at(index);
    { static_cast<bool>(array.slice(index, count)) } -> std::same_as<bool>;
    { array.slice(index, count)->size() } -> std::convertible_to<size_t>;
    *array.slice(index, count)->at(index);
};

/**
 * @class DataFieldTraversal
 * @brief On-demand windowing and field operations over an indexed array.
 *
 * A traversal maintains an independent position and batch size without
 * owning the array. Field projection is available when the array and its
 * slices provide to_nddata<T>(field_name). The same traversal works with any
 * FieldArray; neither it nor the array needs to name a concrete record format.
 *
 * Usage:
 * @code
 * DataFieldTraversal traversal(items, 64);
 * while (!traversal.at_end()) {
 *     auto scores = traversal.window_to_nddata<double>("score");
 *     traversal.for_each_in_window([](const auto& item) {
 *         auto name = item.text("name");
 *     });
 *     traversal.advance();
 * }
 * @endcode
 *
 * @tparam Array Indexed array satisfying FieldArray.
 * @note Borrowed elements and windows remain subject to the array's own
 * lifetime and invalidation rules. Keep the array at a stable address while
 * using a traversal.
 */
template <FieldArray Array>
class DataFieldTraversal {
public:
    /**
     * @brief Borrow an array and set the initial window size.
     * @param array Array that must outlive this traversal.
     * @param batch_size Elements per window; zero is normalized to one.
     */
    explicit DataFieldTraversal(const Array& array, size_t batch_size = 1) noexcept
        : m_array(&array)
        , m_batch_size(batch_size == 0 ? 1 : batch_size)
    {
    }

    /**
     * @brief Borrow the current window, including a short final window.
     * @return Optional-like slice from the current position to the next
     * batch boundary or the end of the array.
     */
    [[nodiscard]] auto current() const
    {
        return m_array->slice(m_position, std::min(m_batch_size, remaining()));
    }

    /**
     * @brief Move forward without passing the end of the array.
     * @param rows Number of elements to advance; zero uses batch_size().
     */
    void advance(size_t rows = 0)
    {
        const auto step = rows == 0 ? m_batch_size : rows;
        m_position += std::min(step, remaining());
    }

    /**
     * @brief Set the position, clamped to the end of the array.
     * @param row Zero-based element position.
     */
    void seek(size_t row) { m_position = std::min(row, m_array->size()); }

    /** @brief Current zero-based element position. */
    [[nodiscard]] size_t position() const noexcept { return m_position; }

    /** @brief Whether there are no elements after the current position. */
    [[nodiscard]] bool at_end() const { return m_position >= m_array->size(); }

    /** @brief Number of elements after the current position. */
    [[nodiscard]] size_t remaining() const
    {
        return m_position < m_array->size() ? m_array->size() - m_position : 0;
    }

    /**
     * @brief Set the window size for subsequent operations.
     * @param rows Elements per window; zero is normalized to one.
     */
    void set_batch_size(size_t rows) noexcept { m_batch_size = rows == 0 ? 1 : rows; }

    /** @brief Current window size. */
    [[nodiscard]] size_t batch_size() const noexcept { return m_batch_size; }

    /**
     * @brief Derive homogeneous NDData from one field across the entire array.
     * @tparam ValueT Exact field value type supported by the array.
     * @param field_name Field to project.
     * @return The array's owning projection result.
     */
    template <typename ValueT>
    [[nodiscard]] auto to_nddata(std::string_view field_name) const
        requires requires(const Array& array, std::string_view name) {
            array.template to_nddata<ValueT>(name);
        }
    {
        return m_array->template to_nddata<ValueT>(field_name);
    }

    /**
     * @brief Derive homogeneous NDData from one field in the current window.
     * @tparam ValueT Exact field value type supported by the slice.
     * @param field_name Field to project.
     * @return The slice's owning projection result, or an empty result when
     * the current slice cannot be obtained.
     */
    template <typename ValueT>
    [[nodiscard]] auto window_to_nddata(std::string_view field_name) const
        requires requires(const Array& array, std::string_view name) {
            array.slice(size_t {}, size_t {})->template to_nddata<ValueT>(name);
        }
    {
        auto window = current();
        using Result = decltype(window->template to_nddata<ValueT>(field_name));
        return window ? window->template to_nddata<ValueT>(field_name) : Result {};
    }

    /**
     * @brief Apply a callable to each available element in the array.
     * @tparam Fn Callable accepting a borrowed element.
     * @param fn Operation to apply in element order.
     * @note An index where at() returns disengaged is skipped entirely, not
     * visited with a default value. fn may be called fewer than size() times.
     */
    template <typename Fn>
    void for_each(Fn&& fn) const
    {
        for (size_t i = 0; i < m_array->size(); ++i) {
            if (auto element = m_array->at(i))
                std::invoke(fn, *element);
        }
    }

    /**
     * @brief Apply a callable to each available element in the current window.
     * @tparam Fn Callable accepting a borrowed element.
     * @param fn Operation to apply in slice order.
     * @note An index where at() returns disengaged is skipped entirely, not
     * visited with a default value. fn may be called fewer than the window's
     * size() times.
     */
    template <typename Fn>
    void for_each_in_window(Fn&& fn) const
    {
        auto window = current();
        if (!window)
            return;

        for (size_t i = 0; i < window->size(); ++i) {
            if (auto element = window->at(i))
                std::invoke(fn, *element);
        }
    }

    /**
     * @brief Fold all available elements into an accumulator.
     * @tparam Acc Accumulator type.
     * @tparam Fn Callable returning the next accumulator from the current
     * accumulator and a borrowed element.
     * @param init Initial accumulator.
     * @param fn Fold operation applied in element order.
     * @return Final accumulator.
     * @note An index where at() returns disengaged is skipped entirely; fn is
     * not invoked for it, not even with a default-constructed element.
     */
    template <typename Acc, typename Fn>
    [[nodiscard]] Acc reduce(Acc init, Fn&& fn) const
    {
        for (size_t i = 0; i < m_array->size(); ++i) {
            if (auto element = m_array->at(i))
                init = std::invoke(fn, std::move(init), *element);
        }
        return init;
    }

private:
    const Array* m_array;
    size_t m_position {};
    size_t m_batch_size;
};

}
