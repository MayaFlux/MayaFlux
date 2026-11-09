#pragma once

#include "TensorAccess.hpp"

namespace MayaFlux::Kakshya {

namespace detail {

    struct AlignedDelete {
        void operator()(std::byte* pointer) const noexcept
        {
            ::operator delete(pointer, std::align_val_t { 256 });
        }
    };

    using AlignedBytes = std::unique_ptr<std::byte, AlignedDelete>;

}

/**
 * @class TensorData
 * @brief Owning host tensor: a TensorSpec and the bytes it describes.
 *
 * Storage is one 256-byte aligned byte block, copied deeply with the object.
 * Any element type the spec can name is stored the same way, so half, int8,
 * int64 and the rest need no DataVariant alternative. A DataVariant is an
 * optional bridge: from_variant() and to_variant() / to_vector() copy to and
 * from it where the element types agree.
 *
 * Usage:
 * @code
 * std::vector<float> pixels(3 * 224 * 224);
 * auto input = TensorData::from_values<float>(pixels, { 1, 3, 224, 224 }, "input");
 * auto view = input->access();
 * auto upload = view->gpu_view();
 * auto variant = input->to_variant();
 * @endcode
 *
 * @note Zero-copy exchange with other libraries goes through TensorAccess and
 * the DLPack functions in TensorDLPack.hpp.
 */
class MAYAFLUX_API TensorData {
public:
    TensorData() = default;
    TensorData(const TensorData& other);
    TensorData& operator=(const TensorData& other);
    TensorData(TensorData&&) noexcept = default;
    TensorData& operator=(TensorData&&) noexcept = default;
    ~TensorData() = default;

    /**
     * @brief Zero-filled tensor for a spec.
     * @return Tensor, or std::nullopt when the spec is inconsistent or not
     * host memory.
     */
    [[nodiscard]] static std::optional<TensorData> allocate(TensorSpec spec);

    /**
     * @brief Tensor holding a copy of @p bytes.
     * @param spec Layout of the bytes.
     * @param bytes Exactly the storage the spec addresses.
     * @return Tensor, or std::nullopt for an inconsistent spec or a size mismatch.
     */
    [[nodiscard]] static std::optional<TensorData> from_bytes(
        TensorSpec spec, std::span<const std::byte> bytes);

    /**
     * @brief Contiguous tensor holding a copy of typed values.
     * @param values Elements in C order.
     * @param shape Dimension sizes; empty means one dimension of values.size().
     * @param name Optional tensor name.
     * @return Tensor, or std::nullopt when the shape does not match the count.
     */
    template <TensorElement T>
    [[nodiscard]] static std::optional<TensorData> from_values(
        std::span<const T> values, std::vector<uint64_t> shape = {}, std::string name = {})
    {
        if (shape.empty())
            shape.push_back(values.size());
        return from_bytes(
            TensorSpec::contiguous(TensorDType::of<T>(), std::move(shape), std::move(name)),
            std::as_bytes(values));
    }

    /**
     * @brief Tensor holding a copy of a DataVariant.
     * @param variant Source data. glm vectors become [N, components] float
     * tensors and glm::mat4 becomes [N, 4, 4].
     * @param shape Dimension sizes; empty keeps the shape implied above.
     * @param name Optional tensor name.
     * @return Tensor, or std::nullopt when the shape does not match the data.
     */
    [[nodiscard]] static std::optional<TensorData> from_variant(
        const DataVariant& variant, std::vector<uint64_t> shape = {}, std::string name = {});

    /**
     * @brief Dense C-ordered copy of any tensor view.
     *
     * Gathers through strides, so transposed, Fortran-ordered, offset and
     * broadcast views all become ordinary contiguous tensors.
     */
    [[nodiscard]] static std::optional<TensorData> copy_of(const TensorAccess& view);

    /** @brief Layout of the stored bytes. */
    [[nodiscard]] const TensorSpec& spec() const noexcept { return m_spec; }
    /** @brief Whether the tensor holds a consistent spec and its storage. */
    [[nodiscard]] bool is_valid() const noexcept { return m_size > 0 || m_spec.element_count().value_or(1) == 0; }
    /** @brief Storage size in bytes. */
    [[nodiscard]] size_t byte_size() const noexcept { return m_size; }
    /** @brief The whole stored block. */
    [[nodiscard]] std::span<const std::byte> storage() const noexcept { return { m_data.get(), m_size }; }
    /** @brief The whole stored block, writable. */
    [[nodiscard]] std::span<std::byte> storage() noexcept { return { m_data.get(), m_size }; }

    /** @brief Borrow the tensor, or std::nullopt when it holds nothing valid. */
    [[nodiscard]] std::optional<TensorAccess> access() const;

    /**
     * @brief Copy into a flat DataVariant when the dtype has an alternative.
     * @return Variant for f64, f32, u8, u16, u32, complex f32 and complex f64
     * dense tensors, otherwise std::nullopt.
     */
    [[nodiscard]] std::optional<DataVariant> to_variant() const;

    /**
     * @brief Copy into a vector of T.
     *
     * Scalar and complex T need a dense tensor of that dtype. glm vector and
     * matrix T need a dense f32 tensor whose bytes divide into whole elements.
     */
    template <DataVariantElement T>
    [[nodiscard]] std::optional<std::vector<T>> to_vector() const
    {
        const auto view = access();
        if (!view)
            return std::nullopt;
        const auto raw = view->bytes();
        if constexpr (TensorElement<T>) {
            if (!m_spec.dtype.is<T>())
                return std::nullopt;
        } else {
            if (!m_spec.dtype.is<float>() || raw.size() % sizeof(T) != 0)
                return std::nullopt;
        }
        if (raw.empty())
            return view->element_count() == 0 ? std::optional<std::vector<T>>(std::vector<T> {}) : std::nullopt;
        std::vector<T> result(raw.size() / sizeof(T));
        std::memcpy(result.data(), raw.data(), result.size() * sizeof(T));
        return result;
    }

private:
    friend class TensorInsertion;

    TensorSpec m_spec;
    detail::AlignedBytes m_data;
    size_t m_size {};
};

/**
 * @struct TensorSet
 * @brief Named tensors that belong together, such as a model's weights or I/O.
 *
 * Mirrors a safetensors or ONNX initializer set: ordered tensors with unique
 * names, plus free-form string metadata.
 */
struct MAYAFLUX_API TensorSet {
    std::vector<TensorData> tensors;
    std::map<std::string, std::string> metadata;

    /** @brief Tensor with this exact name, or nullptr. */
    [[nodiscard]] const TensorData* find(std::string_view name) const;
    /** @brief Tensor with this exact name, or nullptr. */
    [[nodiscard]] TensorData* find(std::string_view name);
    /** @brief Whether every tensor is valid and named, with no duplicate names. */
    [[nodiscard]] bool is_consistent() const;
};

}
