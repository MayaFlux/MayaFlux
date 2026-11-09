#include "Tensor.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Kakshya {

namespace {

    detail::AlignedBytes allocate_block(size_t size)
    {
        if (size == 0)
            return nullptr;
        auto* block = static_cast<std::byte*>(::operator new(size, std::align_val_t { 256 }));
        std::memset(block, 0, size);
        return detail::AlignedBytes(block);
    }

    void gather(const std::byte* source, std::byte*& destination, size_t element,
        const std::vector<uint64_t>& shape, const std::vector<int64_t>& strides, size_t axis)
    {
        if (axis == shape.size()) {
            std::memcpy(destination, source, element);
            destination += element;
            return;
        }
        for (uint64_t i = 0; i < shape[axis]; ++i) {
            const auto step = strides[axis] * static_cast<int64_t>(element) * static_cast<int64_t>(i);
            gather(source + step, destination, element, shape, strides, axis + 1);
        }
    }

    template <typename T>
    std::optional<TensorData> from_vector_shaped(
        const std::vector<T>& values, size_t components, bool matrix,
        std::vector<uint64_t> shape, std::string name)
    {
        const std::span<const std::byte> bytes = std::as_bytes(std::span<const T>(values));
        const size_t count = bytes.size() / sizeof(float) / std::max<size_t>(components, 1);
        if (shape.empty()) {
            shape.push_back(count);
            if (matrix) {
                shape.push_back(4);
                shape.push_back(4);
            } else {
                shape.push_back(components);
            }
        }
        return TensorData::from_bytes(
            TensorSpec::contiguous(TensorDType::of<float>(), std::move(shape), std::move(name)), bytes);
    }

}

TensorData::TensorData(const TensorData& other)
    : m_spec(other.m_spec)
    , m_data(allocate_block(other.m_size))
    , m_size(other.m_size)
{
    if (m_size > 0)
        std::memcpy(m_data.get(), other.m_data.get(), m_size);
}

TensorData& TensorData::operator=(const TensorData& other)
{
    if (this != &other) {
        TensorData copy(other);
        *this = std::move(copy);
    }
    return *this;
}

std::optional<TensorData> TensorData::allocate(TensorSpec spec)
{
    if (const auto reason = spec.diagnose(); !reason.empty()) {
        MF_ERROR(Journal::Component::Kakshya, Journal::Context::Runtime,
            "TensorData::allocate: {}", reason);
        return std::nullopt;
    }
    if (!spec.device.is_host_accessible()) {
        MF_ERROR(Journal::Component::Kakshya, Journal::Context::Runtime,
            "TensorData::allocate: tensor '{}' is not host memory", spec.name);
        return std::nullopt;
    }

    const auto range = spec.byte_range();
    TensorData result;
    result.m_size = static_cast<size_t>(range->second);
    result.m_data = allocate_block(result.m_size);
    result.m_spec = std::move(spec);
    return result;
}

std::optional<TensorData> TensorData::from_bytes(TensorSpec spec, std::span<const std::byte> bytes)
{
    auto result = allocate(std::move(spec));
    if (!result)
        return std::nullopt;
    if (result->m_size != bytes.size()) {
        MF_ERROR(Journal::Component::Kakshya, Journal::Context::Runtime,
            "TensorData::from_bytes: tensor '{}' needs {} bytes, received {}",
            result->m_spec.name, result->m_size, bytes.size());
        return std::nullopt;
    }
    if (!bytes.empty())
        std::memcpy(result->m_data.get(), bytes.data(), bytes.size());
    return result;
}

std::optional<TensorData> TensorData::from_variant(
    const DataVariant& variant, std::vector<uint64_t> shape, std::string name)
{
    return std::visit(
        [&](const auto& values) -> std::optional<TensorData> {
            using V = typename std::decay_t<decltype(values)>::value_type;
            if constexpr (TensorElement<V>) {
                return from_values<V>(std::span<const V>(values), std::move(shape), std::move(name));
            } else if constexpr (std::same_as<V, glm::mat4>) {
                return from_vector_shaped(values, 16, true, std::move(shape), std::move(name));
            } else {
                return from_vector_shaped(values, sizeof(V) / sizeof(float), false, std::move(shape), std::move(name));
            }
        },
        variant);
}

std::optional<TensorData> TensorData::copy_of(const TensorAccess& view)
{
    TensorSpec spec = view.spec();
    spec.strides.clear();
    spec.byte_offset = 0;
    spec.device = {};

    auto result = allocate(spec);
    if (!result)
        return std::nullopt;
    if (result->m_size == 0)
        return result;

    if (view.spec().dtype.is_sub_byte() || view.spec().is_contiguous()) {
        std::memcpy(result->m_data.get(), view.element_data(), result->m_size);
        return result;
    }

    std::byte* cursor = result->m_data.get();
    gather(view.element_data(), cursor, view.spec().dtype.element_bytes(),
        view.spec().shape, view.spec().resolved_strides(), 0);
    return result;
}

std::optional<TensorAccess> TensorData::access() const
{
    if (!is_valid())
        return std::nullopt;
    return as_tensor_access(storage(), m_spec, false);
}

std::optional<DataVariant> TensorData::to_variant() const
{
    if (const auto v = to_vector<double>())
        return DataVariant { *v };
    if (const auto v = to_vector<float>())
        return DataVariant { *v };
    if (const auto v = to_vector<uint8_t>())
        return DataVariant { *v };
    if (const auto v = to_vector<uint16_t>())
        return DataVariant { *v };
    if (const auto v = to_vector<uint32_t>())
        return DataVariant { *v };
    if (const auto v = to_vector<std::complex<float>>())
        return DataVariant { *v };
    if (const auto v = to_vector<std::complex<double>>())
        return DataVariant { *v };
    return std::nullopt;
}

const TensorData* TensorSet::find(std::string_view name) const
{
    for (const auto& tensor : tensors) {
        if (tensor.spec().name == name)
            return &tensor;
    }
    return nullptr;
}

TensorData* TensorSet::find(std::string_view name)
{
    for (auto& tensor : tensors) {
        if (tensor.spec().name == name)
            return &tensor;
    }
    return nullptr;
}

bool TensorSet::is_consistent() const
{
    for (size_t i = 0; i < tensors.size(); ++i) {
        const auto& name = tensors[i].spec().name;
        if (name.empty() || !tensors[i].is_valid() || !tensors[i].spec().is_valid())
            return false;
        for (size_t j = i + 1; j < tensors.size(); ++j) {
            if (tensors[j].spec().name == name)
                return false;
        }
    }
    return true;
}

}
