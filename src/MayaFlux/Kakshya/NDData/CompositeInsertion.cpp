#include "CompositeInsertion.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Kakshya {

std::optional<size_t> CompositeInsertion::append(const Composite& source)
{
    const auto& source_layout = source.layout();
    const auto& target_layout = m_array->m_layout;

    if (source_layout.stride_bytes() != target_layout.stride_bytes()
        || source_layout.fields().size() != target_layout.fields().size()) {
        MF_WARN(Journal::Component::Kakshya, Journal::Context::Runtime,
            "CompositeInsertion: incompatible layout");
        return std::nullopt;
    }

    for (size_t i = 0; i < target_layout.fields().size(); ++i) {
        const auto& src = source_layout.fields()[i];
        const auto& dst = target_layout.fields()[i];
        if (src.name != dst.name || src.type != dst.type
            || src.offset_bytes != dst.offset_bytes || src.size_bytes != dst.size_bytes) {
            MF_WARN(Journal::Component::Kakshya, Journal::Context::Runtime,
                "CompositeInsertion: incompatible field '{}'", dst.name);
            return std::nullopt;
        }
    }

    const auto& source_elements = std::get<std::vector<uint8_t>>(*source.m_rows);
    const size_t stride = target_layout.stride_bytes();
    const auto element_begin = source_elements.begin() + static_cast<ptrdiff_t>(source.m_index * stride);

    std::vector<uint8_t> element(element_begin, element_begin + static_cast<ptrdiff_t>(stride));
    std::vector<std::pair<size_t, std::string>> texts;
    size_t additional_text = 0;

    for (size_t i = 0; i < source_layout.fields().size(); ++i) {
        const auto& field = source_layout.fields()[i];
        if (field.type != typeid(std::string) || !source.has(field.name))
            continue;
        const auto value = source.text(field.name);
        if (!value || value->size() > std::numeric_limits<size_t>::max() - additional_text)
            return std::nullopt;
        additional_text += value->size();
        texts.emplace_back(i, std::string(*value));
    }

    auto& target_elements = std::get<std::vector<uint8_t>>(m_array->m_rows);
    auto& target_text = std::get<std::vector<uint8_t>>(m_array->m_text);

    if (stride > target_elements.max_size() - target_elements.size()
        || additional_text > target_text.max_size() - target_text.size())
        return std::nullopt;

    target_elements.reserve(target_elements.size() + stride);
    target_text.reserve(target_text.size() + additional_text);

    const size_t index = m_array->append();
    std::memcpy(target_elements.data() + index * stride, element.data(), stride);

    for (const auto& [field_index, value] : texts)
        m_array->set_text(index, target_layout.fields()[field_index].name, value);

    return index;
}

}
