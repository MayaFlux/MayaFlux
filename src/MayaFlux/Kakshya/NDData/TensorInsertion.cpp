#include "TensorInsertion.hpp"

namespace MayaFlux::Kakshya {

namespace {

    void scatter(std::byte* base, const TensorSpec& target, const std::byte* from)
    {
        const std::vector<int64_t> strides = target.resolved_strides();
        const size_t element = target.dtype.element_bytes();
        std::vector<uint64_t> index(target.shape.size(), 0);
        const uint64_t count = target.element_count().value_or(0);

        for (uint64_t n = 0; n < count; ++n) {
            int64_t offset = 0;
            for (size_t axis = 0; axis < index.size(); ++axis)
                offset += strides[axis] * static_cast<int64_t>(index[axis]);
            std::memcpy(base + offset * static_cast<int64_t>(element), from + n * element, element);
            for (size_t axis = index.size(); axis-- > 0;) {
                if (++index[axis] < target.shape[axis])
                    break;
                index[axis] = 0;
            }
        }
    }

}

bool TensorInsertion::write_bytes(std::span<const std::byte> bytes, uint64_t byte_offset)
{
    const auto& spec = m_tensor->m_spec;
    const auto count = spec.element_count();
    if (!count || !spec.is_contiguous() || !m_tensor->is_valid())
        return false;

    const auto total = spec.dtype.storage_bytes(*count);
    if (!total || byte_offset > *total || bytes.size() > *total - byte_offset)
        return false;

    if (!bytes.empty())
        std::memcpy(m_tensor->m_data.get() + spec.byte_offset + byte_offset, bytes.data(), bytes.size());
    return true;
}

bool TensorInsertion::assign(const TensorAccess& view)
{
    const auto& spec = m_tensor->m_spec;
    if (view.spec().dtype != spec.dtype || view.spec().shape != spec.shape || !m_tensor->is_valid())
        return false;

    const auto copy = TensorData::copy_of(view);
    if (!copy)
        return false;

    const auto source = copy->access();
    if (!source)
        return false;

    if (spec.is_contiguous())
        return write_bytes(source->bytes());

    scatter(m_tensor->m_data.get() + spec.byte_offset, spec, source->element_data());
    return true;
}

bool TensorInsertion::assign_window(const Region& region, const TensorAccess& view)
{
    const auto& spec = m_tensor->m_spec;
    if (view.spec().dtype != spec.dtype || !m_tensor->is_valid())
        return false;

    const auto target = spec.window(region);
    if (!target || target->shape != view.spec().shape)
        return false;
    if (spec.dtype.is_sub_byte())
        return assign(view);

    const auto range = target->byte_range();
    if (!range || range->second > m_tensor->m_size)
        return false;

    const auto copy = TensorData::copy_of(view);
    if (!copy)
        return false;
    const auto source = copy->access();
    if (!source)
        return false;

    scatter(m_tensor->m_data.get() + target->byte_offset, *target, source->element_data());
    return true;
}

bool TensorInsertion::reshape(std::vector<uint64_t> shape)
{
    auto& spec = m_tensor->m_spec;
    if (!spec.is_contiguous())
        return false;

    TensorSpec candidate = spec;
    candidate.shape = std::move(shape);
    candidate.strides.clear();
    candidate.axes.clear();
    candidate.quantization.reset();

    if (candidate.element_count() != spec.element_count() || !candidate.is_valid())
        return false;

    spec = std::move(candidate);
    return true;
}

bool TensorInsertion::resize(std::vector<uint64_t> shape)
{
    TensorSpec candidate = m_tensor->m_spec;
    candidate.shape = std::move(shape);
    candidate.strides.clear();
    candidate.axes.clear();
    candidate.quantization.reset();
    candidate.byte_offset = 0;

    auto replacement = TensorData::allocate(std::move(candidate));
    if (!replacement)
        return false;

    const auto kept = m_tensor->access();
    if (kept && kept->element_data() && replacement->m_size > 0) {
        const auto source = TensorData::copy_of(*kept);
        if (source) {
            const size_t count = std::min(source->m_size, replacement->m_size);
            std::memcpy(replacement->m_data.get(), source->m_data.get(), count);
        }
    }

    *m_tensor = std::move(*replacement);
    return true;
}

bool TensorInsertion::set_axes(std::vector<DataDimension> axes)
{
    if (axes.size() != m_tensor->m_spec.shape.size())
        return false;
    m_tensor->m_spec.axes = std::move(axes);
    return true;
}

bool TensorInsertion::set_quantization(std::optional<TensorQuantization> quantization)
{
    TensorSpec candidate = m_tensor->m_spec;
    candidate.quantization = std::move(quantization);
    if (!candidate.is_valid())
        return false;
    m_tensor->m_spec = std::move(candidate);
    return true;
}

}
