#pragma once

#include "Tensor.hpp"

struct DLManagedTensor;
struct DLManagedTensorVersioned;

namespace MayaFlux::Kakshya {

/**
 * @brief Hand a tensor to another library through DLPack without copying.
 * @param tensor Tensor to share; the export keeps it alive until the consumer
 * calls the returned tensor's deleter.
 * @return Versioned DLPack tensor flagged read-only, or nullptr when the
 * tensor is null or invalid. The consumer owns the result and must call its
 * deleter exactly once.
 */
[[nodiscard]] MAYAFLUX_API DLManagedTensorVersioned* export_dlpack(
    std::shared_ptr<const TensorData> tensor);

/**
 * @class TensorImport
 * @brief Owns a DLPack tensor received from another library.
 *
 * Takes the producer's tensor, validates it, and exposes it as a TensorAccess
 * without copying. The producer's deleter runs when this object is destroyed
 * or when the tensor is rejected.
 *
 * Usage:
 * @code
 * auto imported = TensorImport::take(managed);
 * auto view = imported->access();
 * auto owned = imported->copy();
 * @endcode
 *
 * @note The data is borrowed from the producer. Its extent is derived from the
 * shape, strides and offset, which cannot be checked against the producer's
 * allocation.
 */
class MAYAFLUX_API TensorImport {
public:
    TensorImport(TensorImport&& other) noexcept;
    TensorImport& operator=(TensorImport&& other) noexcept;
    TensorImport(const TensorImport&) = delete;
    TensorImport& operator=(const TensorImport&) = delete;
    ~TensorImport();

    /**
     * @brief Take ownership of a versioned DLPack tensor.
     * @param managed Tensor from a producer. Always consumed: its deleter is
     * called on rejection as the protocol requires.
     * @return Import, or std::nullopt for a null pointer, a different major
     * version, a device the host cannot read or an unusable layout.
     */
    [[nodiscard]] static std::optional<TensorImport> take(DLManagedTensorVersioned* managed);

    /** @brief Take ownership of a legacy, unversioned DLPack tensor. */
    [[nodiscard]] static std::optional<TensorImport> take(DLManagedTensor* managed);

    /** @brief Layout described by the producer. */
    [[nodiscard]] const TensorSpec& spec() const noexcept { return m_spec; }
    /** @brief Whether the producer marked the tensor read-only. */
    [[nodiscard]] bool read_only() const noexcept { return m_read_only; }
    /** @brief Borrow the producer's memory. */
    [[nodiscard]] std::optional<TensorAccess> access() const;
    /** @brief Dense owning copy, independent of the producer. */
    [[nodiscard]] std::optional<TensorData> copy() const;

private:
    TensorImport(TensorSpec spec, const std::byte* data, size_t size, bool read_only,
        std::function<void()> release) noexcept;

    TensorSpec m_spec;
    const std::byte* m_data {};
    size_t m_size {};
    bool m_read_only {};
    std::function<void()> m_release;
};

}
