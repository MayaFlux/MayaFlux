#include "TensorAccess.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Kakshya {

std::optional<TensorAccess> as_tensor_access(
    std::span<const std::byte> storage, const TensorSpec& spec, bool read_only)
{
    if (const auto reason = spec.diagnose(); !reason.empty()) {
        MF_ERROR(Journal::Component::Kakshya, Journal::Context::Runtime,
            "as_tensor_access: {}", reason);
        return std::nullopt;
    }
    if (!spec.device.is_host_accessible()) {
        MF_ERROR(Journal::Component::Kakshya, Journal::Context::Runtime,
            "as_tensor_access: tensor '{}' lives on a device the host cannot read", spec.name);
        return std::nullopt;
    }

    const auto range = spec.byte_range();
    if (!range || range->second > storage.size()) {
        MF_ERROR(Journal::Component::Kakshya, Journal::Context::Runtime,
            "as_tensor_access: tensor '{}' needs {} bytes but {} are available",
            spec.name, range ? range->second : 0, storage.size());
        return std::nullopt;
    }

    return TensorAccess(storage, spec, read_only);
}

uint64_t TensorAccess::element_count() const noexcept
{
    return m_spec.element_count().value_or(0);
}

const std::byte* TensorAccess::element_data() const noexcept
{
    if (element_count() == 0 || m_storage.empty())
        return nullptr;
    return m_storage.data() + m_spec.byte_offset;
}

std::span<const std::byte> TensorAccess::bytes() const noexcept
{
    const auto* first = element_data();
    if (!first || !m_spec.is_contiguous())
        return {};
    const auto size = m_spec.dtype.storage_bytes(element_count());
    return size ? std::span<const std::byte>(first, *size) : std::span<const std::byte> {};
}

TensorGpuView TensorAccess::gpu_view() const noexcept
{
    const auto raw = bytes();
    return { .data = raw.data(), .bytes = raw.size(), .format = m_spec.dtype.gpu_format() };
}

}
