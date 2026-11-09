#pragma once

#include "Tensor.hpp"

namespace MayaFlux::Kakshya {

/**
 * @class TensorInsertion
 * @brief Mutable adapter for writing into a TensorData.
 *
 * Writes typed values or raw bytes, copies a whole view into the tensor,
 * reshapes, resizes, and attaches axes or quantization. Every call validates
 * against the tensor's spec and returns false, leaving the tensor unchanged,
 * on a mismatch.
 *
 * Usage:
 * @code
 * TensorInsertion insert(weights);
 * insert.write<float>(values, 0);
 * insert.reshape({ 64, 3, 7, 7 });
 * @endcode
 *
 * @note The destination must outlive the adapter.
 */
class MAYAFLUX_API TensorInsertion {
public:
    /** @brief Borrow the tensor to write into. */
    explicit TensorInsertion(TensorData& tensor) noexcept
        : m_tensor(&tensor)
    {
    }

    /**
     * @brief Write typed values into a dense tensor.
     * @tparam T Element type equal to the tensor dtype.
     * @param values Elements to write.
     * @param first_element Index of the first element to overwrite.
     * @return False for a dtype mismatch, a strided layout or a range past the end.
     */
    template <TensorElement T>
    bool write(std::span<const T> values, uint64_t first_element = 0)
    {
        if (!m_tensor->m_spec.dtype.is<T>())
            return false;
        return write_bytes(std::as_bytes(values), first_element * sizeof(T));
    }

    /**
     * @brief Write raw bytes into the elements of a dense tensor.
     * @param bytes Bytes to write.
     * @param byte_offset Offset from element zero.
     * @return False for a strided layout or a range past the end.
     */
    bool write_bytes(std::span<const std::byte> bytes, uint64_t byte_offset = 0);

    /**
     * @brief Overwrite the tensor with a copy of a view of the same dtype and shape.
     *
     * Strided views are gathered into the tensor's own layout.
     */
    bool assign(const TensorAccess& view);

    /**
     * @brief Change the shape without moving data.
     * @param shape New shape with the same element count; the tensor becomes dense.
     * @return False when the count differs or the tensor is strided.
     */
    bool reshape(std::vector<uint64_t> shape);

    /**
     * @brief Replace the shape and reallocate.
     *
     * Existing elements are kept where positions overlap in flat order and the
     * rest are zero. The tensor becomes dense with no byte offset.
     */
    bool resize(std::vector<uint64_t> shape);

    /** @brief Attach per-axis roles and names; the count must equal the rank. */
    bool set_axes(std::vector<DataDimension> axes);
    /** @brief Attach or clear quantization; it must be consistent with the shape. */
    bool set_quantization(std::optional<TensorQuantization> quantization);
    /** @brief Rename the tensor. */
    void set_name(std::string name) { m_tensor->m_spec.name = std::move(name); }

private:
    TensorData* m_tensor;
};

}
