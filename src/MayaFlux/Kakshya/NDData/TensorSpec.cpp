#include "TensorSpec.hpp"

namespace MayaFlux::Kakshya {

namespace {

    constexpr uint64_t max_signed = static_cast<uint64_t>(std::numeric_limits<int64_t>::max());

    bool magnitude_times(uint64_t a, uint64_t b, uint64_t& out)
    {
        if (a != 0 && b > max_signed / a)
            return false;
        out = a * b;
        return true;
    }

}

TensorSpec TensorSpec::contiguous(TensorDType dtype, std::vector<uint64_t> shape, std::string name)
{
    TensorSpec spec;
    spec.name = std::move(name);
    spec.dtype = dtype;
    spec.shape = std::move(shape);
    return spec;
}

std::optional<uint64_t> TensorSpec::element_count() const noexcept
{
    uint64_t count = 1;
    for (const auto extent : shape) {
        if (!magnitude_times(count, extent, count))
            return std::nullopt;
    }
    return count;
}

bool TensorSpec::is_contiguous() const noexcept
{
    if (strides.empty())
        return true;
    if (strides.size() != shape.size())
        return false;

    const auto count = element_count();
    if (count && *count == 0)
        return true;

    int64_t expected = 1;
    for (size_t i = shape.size(); i-- > 0;) {
        if (shape[i] != 1 && strides[i] != expected)
            return false;
        if (shape[i] > max_signed / static_cast<uint64_t>(std::max<int64_t>(expected, 1)))
            return false;
        expected *= static_cast<int64_t>(shape[i]);
    }
    return true;
}

std::vector<int64_t> TensorSpec::resolved_strides() const
{
    if (!strides.empty())
        return strides;

    std::vector<int64_t> result(shape.size(), 1);
    int64_t running = 1;
    for (size_t i = shape.size(); i-- > 0;) {
        result[i] = running;
        running *= static_cast<int64_t>(std::max<uint64_t>(shape[i], 1));
    }
    return result;
}

std::optional<std::pair<uint64_t, uint64_t>> TensorSpec::byte_range() const noexcept
{
    const auto count = element_count();
    if (!count)
        return std::nullopt;
    if (*count == 0)
        return std::pair<uint64_t, uint64_t> { byte_offset, byte_offset };

    if (dtype.is_sub_byte()) {
        const auto bytes = dtype.storage_bytes(*count);
        if (!bytes || *bytes > std::numeric_limits<uint64_t>::max() - byte_offset)
            return std::nullopt;
        return std::pair<uint64_t, uint64_t> { byte_offset, byte_offset + *bytes };
    }

    const auto resolved = resolved_strides();
    uint64_t low = 0;
    uint64_t high = 0;
    for (size_t i = 0; i < shape.size(); ++i) {
        if (shape[i] <= 1)
            continue;
        const int64_t stride = resolved[i];
        const uint64_t magnitude = stride < 0 ? static_cast<uint64_t>(-(stride + 1)) + 1 : static_cast<uint64_t>(stride);
        uint64_t reach = 0;
        if (!magnitude_times(shape[i] - 1, magnitude, reach))
            return std::nullopt;
        uint64_t& side = stride < 0 ? low : high;
        if (reach > max_signed - side)
            return std::nullopt;
        side += reach;
    }

    const uint64_t element = dtype.element_bytes();
    uint64_t low_bytes = 0;
    uint64_t high_bytes = 0;
    if (!magnitude_times(low, element, low_bytes) || !magnitude_times(high, element, high_bytes))
        return std::nullopt;
    if (low_bytes > byte_offset)
        return std::nullopt;

    const uint64_t first = byte_offset - low_bytes;
    const uint64_t extent = high_bytes + low_bytes + element;
    if (extent < element || extent > std::numeric_limits<uint64_t>::max() - first)
        return std::nullopt;
    return std::pair<uint64_t, uint64_t> { first, first + extent };
}

std::string TensorSpec::diagnose() const
{
    const std::string label = name.empty() ? std::string("tensor") : "tensor '" + name + "'";

    if (dtype.bits == 0 || dtype.lanes == 0)
        return label + ": element type has zero width";
    if (static_cast<uint8_t>(dtype.kind) > static_cast<uint8_t>(TensorDTypeKind::BCOMPLEX))
        return label + ": unknown element type kind";
    if (!element_count())
        return label + ": element count overflows";
    if (!strides.empty() && strides.size() != shape.size())
        return label + ": strides rank " + std::to_string(strides.size())
            + " differs from shape rank " + std::to_string(shape.size());
    if (!axes.empty() && axes.size() != shape.size())
        return label + ": axes rank " + std::to_string(axes.size())
            + " differs from shape rank " + std::to_string(shape.size());
    if (dtype.is_sub_byte() && !is_contiguous())
        return label + ": " + dtype.name() + " is sub-byte and must be contiguous";
    if (!byte_range())
        return label + ": layout addresses memory outside its storage";

    if (quantization) {
        const auto& q = *quantization;
        if (q.scale.empty())
            return label + ": quantization has no scale";
        const size_t expected = q.axis ? (*q.axis < shape.size() ? shape[*q.axis] : 0) : 1;
        if (q.axis && *q.axis >= shape.size())
            return label + ": quantization axis " + std::to_string(*q.axis) + " is out of range";
        if (q.scale.size() != expected)
            return label + ": quantization has " + std::to_string(q.scale.size())
                + " scales, expected " + std::to_string(expected);
        if (!q.zero_point.empty() && q.zero_point.size() != q.scale.size())
            return label + ": quantization zero points differ in length from scales";
    }

    return {};
}

std::vector<DataDimension> TensorSpec::dimensions() const
{
    const auto resolved = resolved_strides();
    std::vector<DataDimension> result;
    result.reserve(shape.size());

    for (size_t i = 0; i < shape.size(); ++i) {
        const int64_t stride = resolved[i];
        const uint64_t magnitude = stride < 0 ? static_cast<uint64_t>(-(stride + 1)) + 1 : static_cast<uint64_t>(stride);
        if (i < axes.size()) {
            auto axis = axes[i];
            axis.size = shape[i];
            axis.stride = magnitude;
            result.push_back(std::move(axis));
        } else {
            result.emplace_back("axis" + std::to_string(i), shape[i], magnitude, DataDimension::Role::CUSTOM);
        }
    }
    return result;
}

}
