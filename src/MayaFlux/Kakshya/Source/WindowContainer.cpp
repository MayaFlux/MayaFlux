#include "WindowContainer.hpp"

#include "MayaFlux/Core/Backends/Windowing/Window.hpp"
#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Kakshya/NDData/DataAccess.hpp"
#include "MayaFlux/Kakshya/Processors/WindowAccessProcessor.hpp"
#include "MayaFlux/Kakshya/Region/RegionGroup.hpp"
#include "MayaFlux/Kakshya/Utils/CoordUtils.hpp"

namespace MayaFlux::Kakshya {

WindowContainer::WindowContainer(std::shared_ptr<Core::Window> window)
    : m_window(std::move(window))
{
    if (!m_window) {
        error<std::invalid_argument>(
            Journal::Component::Kakshya,
            Journal::Context::ContainerProcessing,
            std::source_location::current(),
            "WindowContainer requires a valid window");
    }

    setup_dimensions();
    create_default_processor();

    MF_INFO(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
        "WindowContainer created for window '{}' ({}x{})",
        m_window->get_create_info().title,
        m_structure.get_width(), m_structure.get_height());
}

// =========================================================================
// Setup
// =========================================================================

void WindowContainer::setup_dimensions()
{
    const auto& fmt = m_window->get_create_info().container_format;
    const uint32_t w = m_window->get_create_info().width;
    const uint32_t h = m_window->get_create_info().height;
    const uint32_t c = fmt.color_channels;

    m_structure = ContainerDataStructure::image_interleaved();
    m_structure.dimensions = DataDimension::create_dimensions(
        DataModality::IMAGE_COLOR,
        { static_cast<uint64_t>(h), static_cast<uint64_t>(w), static_cast<uint64_t>(c) },
        MemoryLayout::ROW_MAJOR);

    const size_t sz = static_cast<size_t>(w) * h * c;
    m_processed_data.resize(1);
    m_processed_data[0] = std::vector<uint8_t>(sz, 0U);
}

// =========================================================================
// NDDimensionalContainer
// =========================================================================

std::vector<DataDimension> WindowContainer::get_dimensions() const
{
    return m_structure.dimensions;
}

uint64_t WindowContainer::get_total_elements() const
{
    return m_structure.get_total_elements();
}

MemoryLayout WindowContainer::get_memory_layout() const
{
    return m_structure.memory_layout;
}

void WindowContainer::set_memory_layout(MemoryLayout layout)
{
    m_structure.memory_layout = layout;
}

std::vector<DataVariant> WindowContainer::get_region_data(const Region& region) const
{
    std::shared_lock lock(m_data_mutex);

    if (m_processed_data.empty())
        return {};

    const auto* src = std::get_if<std::vector<uint8_t>>(&m_processed_data[0]);
    if (!src || src->empty())
        return {};

    std::vector<DataVariant> result;
    result.reserve(m_loaded_regions.size());

    for (const auto& r : m_loaded_regions) {
        if (!regions_intersect(r, region))
            continue;

        const auto x0 = static_cast<uint32_t>(r.start_coordinates[1]);
        const auto y0 = static_cast<uint32_t>(r.start_coordinates[0]);
        const auto x1 = static_cast<uint32_t>(std::min(r.end_coordinates[1],
            static_cast<uint64_t>(m_structure.get_width())));
        const auto y1 = static_cast<uint32_t>(std::min(r.end_coordinates[0],
            static_cast<uint64_t>(m_structure.get_height())));

        if (x1 <= x0 || y1 <= y0)
            continue;

        result.emplace_back(crop_region(*src, x0, y0, x1 - x0, y1 - y0));
    }

    return result;
}

void WindowContainer::set_region_data(const Region& /*region*/, const std::vector<DataVariant>& /*data*/)
{
    MF_WARN(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
        "WindowContainer::set_region_data — write path not yet implemented");
}

std::vector<DataVariant> WindowContainer::get_region_group_data(const RegionGroup& /*group*/) const
{
    std::shared_lock lock(m_data_mutex);
    return m_processed_data;
}

std::vector<DataVariant> WindowContainer::get_segments_data(const std::vector<RegionSegment>& /*segments*/) const
{
    std::shared_lock lock(m_data_mutex);
    return m_processed_data;
}

double WindowContainer::get_value_at(const std::vector<uint64_t>& coordinates) const
{
    if (coordinates.size() < 3 || m_processed_data.empty())
        return 0.0;

    const auto* pixels = std::get_if<std::vector<uint8_t>>(&m_processed_data[0]);
    if (!pixels)
        return 0.0;

    const uint64_t w = m_structure.get_width();
    const uint64_t c = m_structure.get_channel_count();
    const uint64_t idx = (coordinates[0] * w + coordinates[1]) * c + coordinates[2];

    if (idx >= pixels->size())
        return 0.0;

    return static_cast<double>((*pixels)[idx]) / 255.0;
}

void WindowContainer::set_value_at(const std::vector<uint64_t>& /*coords*/, double /*value*/) { }

uint64_t WindowContainer::coordinates_to_linear_index(const std::vector<uint64_t>& coordinates) const
{
    return coordinates_to_linear(coordinates, m_structure.dimensions);
}

std::vector<uint64_t> WindowContainer::linear_index_to_coordinates(uint64_t index) const
{
    return linear_to_coordinates(index, m_structure.dimensions);
}

void WindowContainer::clear()
{
    std::unique_lock lock(m_data_mutex);
    const size_t sz = m_structure.get_total_elements();
    m_processed_data.resize(1);
    m_processed_data[0] = std::vector<uint8_t>(sz, 0U);
    update_processing_state(ProcessingState::IDLE);
}

void WindowContainer::lock() { m_data_mutex.lock(); }
void WindowContainer::unlock() { m_data_mutex.unlock(); }
bool WindowContainer::try_lock() { return m_data_mutex.try_lock(); }

const void* WindowContainer::get_raw_data() const
{
    if (m_processed_data.empty())
        return nullptr;
    const auto* v = std::get_if<std::vector<uint8_t>>(&m_processed_data[0]);
    return (v && !v->empty()) ? v->data() : nullptr;
}

bool WindowContainer::has_data() const
{
    std::shared_lock lock(m_data_mutex);
    if (m_processed_data.empty())
        return false;
    return std::visit([](const auto& v) { return !v.empty(); }, m_processed_data[0]);
}

// =========================================================================
// Crop helper
// =========================================================================

std::vector<uint8_t> WindowContainer::crop_region(
    const std::vector<uint8_t>& src,
    uint32_t x0, uint32_t y0,
    uint32_t pw, uint32_t ph) const
{
    const auto full_w = static_cast<uint32_t>(m_structure.get_width());
    const auto c = static_cast<uint32_t>(m_structure.get_channel_count());
    const uint32_t row_bytes = pw * c;

    std::vector<uint8_t> out(static_cast<size_t>(pw) * ph * c);

    for (uint32_t row = 0; row < ph; ++row) {
        const size_t src_offset = static_cast<size_t>(((y0 + row) * full_w + x0)) * c;
        const size_t dst_offset = static_cast<size_t>(row) * row_bytes;
        std::memcpy(out.data() + dst_offset, src.data() + src_offset, row_bytes);
    }

    return out;
}

// =========================================================================
// Region management
// =========================================================================

void WindowContainer::load_region(const Region& region)
{
    load_region_tracked(region);
}

std::optional<uint32_t> WindowContainer::load_region_tracked(const Region& region)
{
    std::unique_lock lock(m_data_mutex);

    for (size_t i = 0; i < m_loaded_regions.size(); ++i) {
        const auto& r = m_loaded_regions[i];
        if (r.start_coordinates == region.start_coordinates
            && r.end_coordinates == region.end_coordinates)
            return static_cast<uint32_t>(i);
    }

    m_loaded_regions.push_back(region);
    const auto slot = static_cast<uint32_t>(m_loaded_regions.size() - 1);

    lock.unlock();

    MF_INFO(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
        "WindowContainer: loaded region [{},{} -> {},{}], {} active region(s)",
        region.start_coordinates.size() > 1 ? region.start_coordinates[1] : 0,
        region.start_coordinates.size() > 0 ? region.start_coordinates[0] : 0,
        region.end_coordinates.size() > 1 ? region.end_coordinates[1] : 0,
        region.end_coordinates.size() > 0 ? region.end_coordinates[0] : 0,
        m_loaded_regions.size());

    return slot;
}

void WindowContainer::unload_region(const Region& region)
{
    std::unique_lock lock(m_data_mutex);

    auto it = std::ranges::find_if(m_loaded_regions, [&](const Region& r) {
        return r.start_coordinates == region.start_coordinates
            && r.end_coordinates == region.end_coordinates;
    });

    if (it == m_loaded_regions.end())
        return;

    m_loaded_regions.erase(it);

    MF_INFO(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
        "WindowContainer: unloaded region, {} active region(s)",
        m_loaded_regions.size());
}

bool WindowContainer::is_region_loaded(const Region& region) const
{
    std::shared_lock lock(m_data_mutex);
    return std::ranges::any_of(m_loaded_regions, [&](const Region& r) {
        return regions_intersect(r, region);
    });
}

const std::vector<Region>& WindowContainer::get_loaded_regions() const
{
    return m_loaded_regions;
}

bool WindowContainer::regions_intersect(const Region& r1, const Region& r2) noexcept
{
    if (r1.start_coordinates.size() < 2 || r1.end_coordinates.size() < 2
        || r2.start_coordinates.size() < 2 || r2.end_coordinates.size() < 2)
        return false;

    const bool y_overlap = r1.start_coordinates[0] <= r2.end_coordinates[0]
        && r2.start_coordinates[0] <= r1.end_coordinates[0];
    const bool x_overlap = r1.start_coordinates[1] <= r2.end_coordinates[1]
        && r2.start_coordinates[1] <= r1.end_coordinates[1];

    return y_overlap && x_overlap;
}

// =========================================================================
// RegionGroup management
// =========================================================================

void WindowContainer::add_region_group(const RegionGroup& group)
{
    std::lock_guard lock(m_state_mutex);
    m_region_groups[group.name] = group;
}

const RegionGroup& WindowContainer::get_region_group(const std::string& name) const
{
    static const RegionGroup empty;
    std::shared_lock lock(m_data_mutex);
    auto it = m_region_groups.find(name);
    return it != m_region_groups.end() ? it->second : empty;
}

std::unordered_map<std::string, RegionGroup> WindowContainer::get_all_region_groups() const
{
    std::shared_lock lock(m_data_mutex);
    return m_region_groups;
}

void WindowContainer::remove_region_group(const std::string& name)
{
    std::lock_guard lock(m_state_mutex);
    m_region_groups.erase(name);
}

// =========================================================================
// SignalSourceContainer
// =========================================================================

ProcessingState WindowContainer::get_processing_state() const
{
    return m_processing_state.load();
}

void WindowContainer::update_processing_state(ProcessingState new_state)
{
    ProcessingState old = m_processing_state.exchange(new_state);
    if (old == new_state)
        return;

    std::lock_guard lock(m_state_mutex);
    if (m_state_callback)
        m_state_callback(shared_from_this(), new_state);
}

void WindowContainer::register_state_change_callback(
    std::function<void(const std::shared_ptr<SignalSourceContainer>&, ProcessingState)> callback)
{
    std::lock_guard lock(m_state_mutex);
    m_state_callback = std::move(callback);
}

void WindowContainer::unregister_state_change_callback()
{
    std::lock_guard lock(m_state_mutex);
    m_state_callback = nullptr;
}

bool WindowContainer::is_ready_for_processing() const
{
    return m_ready_for_processing.load(std::memory_order_acquire);
}

void WindowContainer::mark_ready_for_processing(bool ready)
{
    m_ready_for_processing.store(ready, std::memory_order_release);
}

void WindowContainer::create_default_processor()
{
    auto proc = std::make_shared<WindowAccessProcessor>();
    proc->on_attach(shared_from_this());
    m_default_processor = proc;
}

void WindowContainer::process_default()
{
    if (m_default_processor)
        m_default_processor->process(shared_from_this());
}

void WindowContainer::set_default_processor(const std::shared_ptr<DataProcessor>& proc)
{
    if (m_default_processor)
        m_default_processor->on_detach(shared_from_this());
    m_default_processor = proc;
    if (m_default_processor)
        m_default_processor->on_attach(shared_from_this());
}

std::shared_ptr<DataProcessor> WindowContainer::get_default_processor() const
{
    return m_default_processor;
}

std::shared_ptr<DataProcessingChain> WindowContainer::get_processing_chain()
{
    return m_processing_chain;
}

void WindowContainer::set_processing_chain(const std::shared_ptr<DataProcessingChain>& chain)
{
    m_processing_chain = chain;
}

// =========================================================================
// Consumer tracking
// =========================================================================

uint32_t WindowContainer::register_dimension_reader(uint32_t /*slot_index*/)
{
    ++m_registered_readers;
    return m_next_reader_id.fetch_add(1, std::memory_order_relaxed);
}

void WindowContainer::unregister_dimension_reader(uint32_t /*slot_index*/)
{
    if (m_registered_readers.load(std::memory_order_relaxed) > 0)
        --m_registered_readers;
}

bool WindowContainer::has_active_readers() const
{
    return m_registered_readers.load(std::memory_order_acquire) > 0;
}

void WindowContainer::mark_dimension_consumed(uint32_t /*slot_index*/, uint32_t /*reader_id*/)
{
    m_consumed_readers.fetch_add(1, std::memory_order_release);
}

bool WindowContainer::all_dimensions_consumed() const
{
    return m_consumed_readers.load(std::memory_order_acquire)
        >= m_registered_readers.load(std::memory_order_acquire);
}

// =========================================================================
// Data access
// =========================================================================

std::vector<DataVariant>& WindowContainer::get_processed_data()
{
    return m_processed_data;
}

const std::vector<DataVariant>& WindowContainer::get_processed_data() const
{
    return m_processed_data;
}

const std::vector<DataVariant>& WindowContainer::get_data()
{
    return m_data;
}

DataAccess WindowContainer::channel_data(size_t /*channel*/)
{
    MF_WARN(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
        "WindowContainer::channel_data — not meaningful for interleaved image data; returning full surface");
    return { m_processed_data[0], m_structure.dimensions, DataModality::IMAGE_COLOR };
}

std::vector<DataAccess> WindowContainer::all_channel_data()
{
    return { DataAccess(m_processed_data[0], m_structure.dimensions, DataModality::IMAGE_COLOR) };
}

} // namespace MayaFlux::Kakshya
