#include "CompositeContainer.hpp"

#include "MayaFlux/Kakshya/Region/RegionSegment.hpp"

namespace MayaFlux::Kakshya {

namespace {

    bool append_slice(CompositeInsertion& insertion, const CompositeSlice& slice)
    {
        for (size_t i = 0; i < slice.size(); ++i) {
            const auto element = slice.at(i);
            if (!element || !insertion.append(*element))
                return false;
        }
        return true;
    }

    std::optional<CompositeArray> copy_slice(const CompositeSlice& slice)
    {
        CompositeArray copy(slice.layout());
        CompositeInsertion insertion(copy);
        if (!append_slice(insertion, slice))
            return std::nullopt;
        return copy;
    }

}

CompositeContainer::CompositeContainer(CompositeLayout layout, size_t batch_size)
    : m_data(std::move(layout))
    , m_materialized_data(m_data.layout())
    , m_batch_size(batch_size == 0 ? 1 : batch_size)
{
}

CompositeContainer::CompositeContainer(CompositeArray data, size_t batch_size)
    : m_data(std::move(data))
    , m_materialized_data(m_data.layout())
    , m_batch_size(batch_size == 0 ? 1 : batch_size)
{
}

CompositeContainer::~CompositeContainer() = default;

bool CompositeContainer::materialize_next()
{
    if (at_end())
        return false;

    auto traversal = traverse(m_batch_size);
    traversal.seek(m_next_element);
    const auto window = traversal.current();
    if (!window || window->size() == 0)
        return false;

    auto next = copy_slice(*window);
    if (!next)
        return false;

    m_materialized_data = std::move(*next);
    m_next_element += window->size();
    return true;
}

std::vector<DataDimension> CompositeContainer::get_dimensions() const
{
    return { DataDimension("elements", m_data.size(), 1, DataDimension::Role::CUSTOM) };
}

std::vector<CompositeArray> CompositeContainer::get_region_data(const Region& region) const
{
    const auto selected = m_data.slice(region);
    if (!selected)
        return {};

    auto copy = copy_slice(*selected);
    if (!copy)
        return {};

    std::vector<CompositeArray> result;
    result.push_back(std::move(*copy));
    return result;
}

std::vector<CompositeArray> CompositeContainer::get_region_group_data(const RegionGroup& group) const
{
    std::vector<CompositeArray> result;
    result.reserve(group.regions.size());

    for (const auto& region : group.regions) {
        auto copied = get_region_data(region);
        if (!copied.empty())
            result.push_back(std::move(copied.front()));
    }
    return result;
}

std::vector<CompositeArray> CompositeContainer::get_segments_data(const std::vector<RegionSegment>& segments) const
{
    std::vector<CompositeArray> result;
    result.reserve(segments.size());

    for (const auto& segment : segments) {
        const auto& region = segment.source_region;

        if (region.start_coordinates.size() != 1 || region.end_coordinates.size() != 1
            || segment.offset_in_region.size() != 1 || segment.segment_size.size() != 1)
            continue;

        const uint64_t source_start = region.start_coordinates[0];
        const uint64_t source_end = region.end_coordinates[0];
        const uint64_t offset = segment.offset_in_region[0];
        const uint64_t count = segment.segment_size[0];

        if (source_start > source_end || offset > source_end - source_start || count == 0)
            continue;

        const uint64_t start = source_start + offset;
        if (count - 1 > source_end - start)
            continue;

        auto copied = get_region_data(Region(
            std::vector<uint64_t> { start }, std::vector<uint64_t> { start + count - 1 }));

        if (!copied.empty())
            result.push_back(std::move(copied.front()));
    }
    return result;
}

void CompositeContainer::set_region_data(const Region& region, const std::vector<CompositeArray>& data)
{
    const auto target = m_data.slice(region);
    if (!target)
        return;

    size_t supplied = 0;
    for (const auto& array : data) {
        if (array.size() > target->size() - supplied)
            return;
        supplied += array.size();
    }
    if (supplied != target->size())
        return;

    CompositeArray replacement(m_data.layout());
    CompositeInsertion insertion(replacement);

    const auto before = m_data.slice(0, target->start());
    const auto after_start = target->start() + target->size();
    const auto after = m_data.slice(after_start, m_data.size() - after_start);

    if (!before || !after || !append_slice(insertion, *before))
        return;

    for (const auto& array : data) {
        const auto selected = array.slice(0, array.size());
        if (!selected || !append_slice(insertion, *selected))
            return;
    }
    if (!append_slice(insertion, *after))
        return;

    m_data = std::move(replacement);
}

void CompositeContainer::add_region_group(const RegionGroup& group)
{
    m_region_groups[group.name] = group;
}

RegionGroup CompositeContainer::get_region_group(const std::string& name) const
{
    const auto it = m_region_groups.find(name);
    return it == m_region_groups.end() ? RegionGroup {} : it->second;
}

void CompositeContainer::remove_region_group(const std::string& name)
{
    m_region_groups.erase(name);
}

bool CompositeContainer::is_region_loaded(const Region& region) const
{
    return m_data.slice(region).has_value();
}

uint64_t CompositeContainer::coordinates_to_linear_index(const std::vector<uint64_t>& coordinates) const
{
    return coordinates.size() == 1 ? coordinates[0] : 0;
}

std::vector<uint64_t> CompositeContainer::linear_index_to_coordinates(uint64_t linear_index) const
{
    return { linear_index };
}

void CompositeContainer::clear()
{
    m_data = CompositeArray(m_data.layout());
    m_materialized_data = CompositeArray(m_data.layout());
    m_region_groups.clear();
    m_next_element = 0;
}

const void* CompositeContainer::get_raw_data() const
{
    const auto& bytes = std::get<std::vector<uint8_t>>(m_data.element_data());
    return bytes.empty() ? nullptr : bytes.data();
}

}
