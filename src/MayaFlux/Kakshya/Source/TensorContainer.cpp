#include "TensorContainer.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Kakshya/Region/RegionSegment.hpp"

namespace MayaFlux::Kakshya {

namespace {

    std::vector<int64_t> column_major_strides(const std::vector<uint64_t>& shape)
    {
        std::vector<int64_t> result(shape.size(), 1);
        int64_t running = 1;
        for (size_t axis = 0; axis < shape.size(); ++axis) {
            result[axis] = running;
            running *= static_cast<int64_t>(std::max<uint64_t>(shape[axis], 1));
        }
        return result;
    }

}

TensorContainer::TensorContainer(TensorData data)
    : m_data(std::move(data))
{
    if (!m_data.is_valid() || !m_data.spec().is_valid()) {
        error<std::invalid_argument>(
            Journal::Component::Kakshya, Journal::Context::Configuration,
            std::source_location::current(),
            "TensorContainer: the tensor holds no valid spec");
    }
}

TensorContainer::TensorContainer(TensorSpec spec)
{
    auto allocated = TensorData::allocate(std::move(spec));
    if (!allocated) {
        error<std::invalid_argument>(
            Journal::Component::Kakshya, Journal::Context::Configuration,
            std::source_location::current(),
            "TensorContainer: the spec is inconsistent or not host memory");
    }
    m_data = std::move(*allocated);
}

TensorContainer::~TensorContainer() = default;

std::optional<TensorAccess> TensorContainer::view(const Region& region) const
{
    const auto window = m_data.spec().window(region);
    if (!window)
        return std::nullopt;

    return as_tensor_access(m_data.storage(), *window, false);
}

std::vector<DataDimension> TensorContainer::get_dimensions() const
{
    return m_data.spec().dimensions();
}

uint64_t TensorContainer::get_total_elements() const
{
    return m_data.spec().element_count().value_or(0);
}

MemoryLayout TensorContainer::get_memory_layout() const
{
    const auto& spec = m_data.spec();
    if (spec.rank() > 1 && !spec.strides.empty() && spec.strides == column_major_strides(spec.shape))
        return MemoryLayout::COLUMN_MAJOR;
    return MemoryLayout::ROW_MAJOR;
}

uint64_t TensorContainer::get_frame_size() const
{
    const auto& shape = m_data.spec().shape;
    if (shape.empty())
        return 0;
    return std::accumulate(shape.begin() + 1, shape.end(), uint64_t { 1 }, std::multiplies<> {});
}

uint64_t TensorContainer::get_num_frames() const
{
    const auto& shape = m_data.spec().shape;
    return shape.empty() ? 0 : shape.front();
}

std::vector<TensorData> TensorContainer::get_region_data(const Region& region) const
{
    const auto selected = view(region);
    if (!selected)
        return {};

    auto copy = TensorData::copy_of(*selected);
    if (!copy)
        return {};

    std::vector<TensorData> result;
    result.push_back(std::move(*copy));
    return result;
}

std::vector<TensorData> TensorContainer::get_region_group_data(const RegionGroup& group) const
{
    std::vector<TensorData> result;
    result.reserve(group.regions.size());

    for (const auto& region : group.regions) {
        auto copied = get_region_data(region);
        if (!copied.empty())
            result.push_back(std::move(copied.front()));
    }
    return result;
}

std::vector<TensorData> TensorContainer::get_segments_data(const std::vector<RegionSegment>& segments) const
{
    std::vector<TensorData> result;
    result.reserve(segments.size());

    for (const auto& segment : segments) {
        if (segment.is_cached) {
            if (const TensorData* held = segment.cache.get(tensor_cache())) {
                result.push_back(*held);
                continue;
            }
        }

        const auto& region = segment.source_region;
        const size_t rank = region.start_coordinates.size();
        if (rank == 0 || region.end_coordinates.size() != rank
            || segment.offset_in_region.size() != rank || segment.segment_size.size() != rank)
            continue;

        Region box;
        box.start_coordinates.resize(rank);
        box.end_coordinates.resize(rank);
        bool valid = true;
        for (size_t axis = 0; axis < rank && valid; ++axis) {
            const uint64_t source_start = region.start_coordinates[axis];
            const uint64_t source_end = region.end_coordinates[axis];
            const uint64_t offset = segment.offset_in_region[axis];
            const uint64_t count = segment.segment_size[axis];

            valid = source_start <= source_end && offset <= source_end - source_start && count != 0;
            if (!valid)
                break;
            const uint64_t start = source_start + offset;
            valid = count - 1 <= source_end - start;
            box.start_coordinates[axis] = start;
            box.end_coordinates[axis] = start + count - 1;
        }
        if (!valid)
            continue;

        auto copied = get_region_data(box);
        if (!copied.empty())
            result.push_back(std::move(copied.front()));
    }
    return result;
}

void TensorContainer::set_region_data(const Region& region, const std::vector<TensorData>& data)
{
    if (data.size() != 1)
        return;

    const auto source = data.front().access();
    if (!source)
        return;

    TensorInsertion(m_data).assign_window(region, *source);
}

void TensorContainer::add_region_group(const RegionGroup& group)
{
    m_region_groups[group.name] = group;
}

RegionGroup TensorContainer::get_region_group(const std::string& name) const
{
    const auto it = m_region_groups.find(name);
    return it == m_region_groups.end() ? RegionGroup {} : it->second;
}

void TensorContainer::remove_region_group(const std::string& name)
{
    m_region_groups.erase(name);
}

bool TensorContainer::is_region_loaded(const Region& region) const
{
    return view(region).has_value();
}

uint64_t TensorContainer::coordinates_to_linear_index(const std::vector<uint64_t>& coordinates) const
{
    const auto& shape = m_data.spec().shape;
    if (coordinates.size() != shape.size())
        return 0;

    uint64_t index = 0;
    for (size_t axis = 0; axis < shape.size(); ++axis) {
        if (coordinates[axis] >= shape[axis])
            return 0;
        index = index * shape[axis] + coordinates[axis];
    }
    return index;
}

std::vector<uint64_t> TensorContainer::linear_index_to_coordinates(uint64_t linear_index) const
{
    const auto& shape = m_data.spec().shape;
    if (linear_index >= get_total_elements())
        return {};

    std::vector<uint64_t> coordinates(shape.size());
    for (size_t axis = shape.size(); axis-- > 0;) {
        coordinates[axis] = linear_index % shape[axis];
        linear_index /= shape[axis];
    }
    return coordinates;
}

void TensorContainer::clear()
{
    auto emptied = TensorData::allocate(
        TensorSpec::contiguous(m_data.spec().dtype, { 0 }, m_data.spec().name));
    if (emptied)
        m_data = std::move(*emptied);
    m_region_groups.clear();
}

const void* TensorContainer::get_raw_data() const
{
    const auto access = m_data.access();
    return access ? access->element_data() : nullptr;
}

}
