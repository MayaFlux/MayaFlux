#include "WindowContainer.hpp"

#include "MayaFlux/Core/Backends/Windowing/Window.hpp"
#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Kakshya/NDData/DataAccess.hpp"
#include "MayaFlux/Kakshya/Processors/WindowAccessProcessor.hpp"
#include "MayaFlux/Kakshya/Region/RegionGroup.hpp"
#include "MayaFlux/Kakshya/Utils/CoordUtils.hpp"
#include "MayaFlux/Kakshya/Utils/RegionUtils.hpp"

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

    const auto& dims = m_structure.dimensions;
    const std::span<const uint8_t> src_span { src->data(), src->size() };

    std::vector<DataVariant> result;

    for (const auto& [name, group] : m_region_groups) {
        for (const auto& r : group.regions) {
            if (!regions_intersect(r, region))
                continue;
            try {
                result.emplace_back(extract_nd_region<uint8_t>(src_span, r, dims));
            } catch (const std::exception& e) {
                MF_WARN(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
                    "WindowContainer::get_region_data extraction failed — {}", e.what());
            }
        }
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

void WindowContainer::load_region(const Region& /*region*/)
{
    MF_WARN(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
        "WindowContainer::load_region — no-op. Register regions via add_region_group()");
}

void WindowContainer::unload_region(const Region& /*region*/)
{
    MF_WARN(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
        "WindowContainer::unload_region — no-op. Remove regions via remove_region_group()");
}

bool WindowContainer::is_region_loaded(const Region& /*region*/) const
{
    return true;
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
    auto readback = std::make_shared<WindowAccessProcessor>();
    readback->on_attach(shared_from_this());
    m_default_processor = readback;
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
