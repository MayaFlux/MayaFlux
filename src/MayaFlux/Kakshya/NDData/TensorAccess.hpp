#pragma once

#include "TensorSpec.hpp"

namespace MayaFlux::Kakshya {

class TensorAccess;

/**
 * @struct TensorGpuView
 * @brief Pointer, byte count and element format for uploading a tensor.
 */
struct TensorGpuView {
    const void* data {};
    size_t bytes {};
    std::optional<GpuDataFormat> format;
};

/**
 * @brief Validate and borrow tensor memory.
 * @param storage Memory the spec addresses, from its first byte.
 * @param spec Layout to interpret the memory with.
 * @param read_only Whether writers must treat the memory as immutable.
 * @return Access view, or std::nullopt (with the reason logged) when the spec
 * is inconsistent, addresses memory outside @p storage, or lives on a device
 * the host cannot read.
 */
[[nodiscard]] MAYAFLUX_API std::optional<TensorAccess> as_tensor_access(
    std::span<const std::byte> storage, const TensorSpec& spec, bool read_only = true);

/**
 * @class TensorAccess
 * @brief Validated, non-owning view of tensor memory and its layout.
 *
 * Works over any host memory: a TensorData, a DataVariant store, a mapped
 * file or an imported DLPack tensor. Nothing is converted or copied.
 *
 * Usage:
 * @code
 * auto access = tensor.access();
 * auto values = access->as<float>();
 * auto upload = access->gpu_view();
 * @endcode
 *
 * @note The borrowed memory must outlive the view.
 */
class MAYAFLUX_API TensorAccess {
public:
    /** @brief Layout of the borrowed memory. */
    [[nodiscard]] const TensorSpec& spec() const noexcept { return m_spec; }
    /** @brief Whether writers must treat the memory as immutable. */
    [[nodiscard]] bool read_only() const noexcept { return m_read_only; }
    /** @brief The whole borrowed region. */
    [[nodiscard]] std::span<const std::byte> storage() const noexcept { return m_storage; }
    /** @brief Pointer to element zero, or nullptr for an empty tensor. */
    [[nodiscard]] const std::byte* element_data() const noexcept;
    /** @brief Number of elements. */
    [[nodiscard]] uint64_t element_count() const noexcept;

    /**
     * @brief Bytes of the elements when they are dense in C order.
     * @return The packed bytes, or an empty span for a strided layout.
     */
    [[nodiscard]] std::span<const std::byte> bytes() const noexcept;

    /**
     * @brief Elements as a typed span.
     * @tparam T Element type that equals the tensor dtype exactly.
     * @return Span over the elements, or empty on a dtype mismatch, a strided
     * layout or a misaligned address.
     */
    template <TensorElement T>
    [[nodiscard]] std::span<const T> as() const noexcept
    {
        if (!m_spec.dtype.is<T>())
            return {};
        const auto raw = bytes();
        if (raw.empty() || reinterpret_cast<uintptr_t>(raw.data()) % alignof(T) != 0)
            return {};
        return { reinterpret_cast<const T*>(raw.data()), raw.size() / sizeof(T) };
    }

    /**
     * @brief Upload description for a dense tensor.
     * @return Pointer, bytes and format; data is null for a strided layout.
     */
    [[nodiscard]] TensorGpuView gpu_view() const noexcept;

    /** @brief Dimension descriptors with sizes and strides from the spec. */
    [[nodiscard]] std::vector<DataDimension> dimensions() const { return m_spec.dimensions(); }

private:
    friend MAYAFLUX_API std::optional<TensorAccess> as_tensor_access(
        std::span<const std::byte>, const TensorSpec&, bool);

    TensorAccess(std::span<const std::byte> storage, TensorSpec spec, bool read_only)
        : m_storage(storage)
        , m_spec(std::move(spec))
        , m_read_only(read_only)
    {
    }

    std::span<const std::byte> m_storage;
    TensorSpec m_spec;
    bool m_read_only;
};

}
