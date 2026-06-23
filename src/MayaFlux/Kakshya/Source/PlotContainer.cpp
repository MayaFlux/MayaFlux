#include "PlotContainer.hpp"

#include "MayaFlux/Kakshya/NDData/DataAccess.hpp"
#include "MayaFlux/Kakshya/Processors/PlotProcessor.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Kakshya {

namespace {
    constexpr auto C = Journal::Component::Kakshya;
    constexpr auto X = Journal::Context::ContainerProcessing;
}

PlotContainer::PlotContainer()
{
    m_structure = ContainerDataStructure(
        DataModality::TENSOR_ND,
        OrganizationStrategy::PLANAR,
        MemoryLayout::ROW_MAJOR);
}

// =========================================================================
// ensure_processor
// =========================================================================

PlotProcessor& PlotContainer::ensure_processor()
{
    if (!m_processor)
        create_default_processor();
    return *std::static_pointer_cast<PlotProcessor>(m_processor);
}

// =========================================================================
// Series management
// =========================================================================

uint32_t PlotContainer::add_series(std::string name, uint64_t count,
    DataDimension::Role role, DataModality modality)
{
    m_data.emplace_back(std::vector<double>(count, 0.0));
    m_processed_data.emplace_back(std::vector<double>(count, 0.0));

    m_series_locks.resize(m_data.size());

    m_structure.dimensions.emplace_back(std::move(name), count, 1, role);
    m_structure.modality = DataModality::TENSOR_ND;

    const auto idx = static_cast<uint32_t>(m_structure.dimensions.size() - 1);
    MF_INFO(C, X, "PlotContainer: added series '{}' at index {} ({} samples, role={}, modality={})",
        m_structure.dimensions[idx].name, idx, count,
        static_cast<int>(role),
        modality_to_string(modality));

    return idx;
}

void PlotContainer::write_series(uint32_t index, std::span<const double> samples)
{
    if (index >= m_data.size()) {
        MF_ERROR(C, X, "PlotContainer::write_series: index {} out of range", index);
        return;
    }

    auto* vec = std::get_if<std::vector<double>>(&m_data[index]);
    if (!vec) {
        MF_ERROR(C, X, "PlotContainer::write_series: series {} has unexpected variant type", index);
        return;
    }

    const uint64_t n = std::min<uint64_t>(samples.size(), vec->size());
    Memory::SeqlockWriteGuard g(m_series_locks[index]);
    std::copy_n(samples.begin(), n, vec->begin());
}

void PlotContainer::write_sample(uint32_t index, uint64_t sample_index, double value)
{
    if (index >= m_data.size()) {
        MF_ERROR(C, X, "PlotContainer::write_sample: series index {} out of range", index);
        return;
    }

    auto* vec = std::get_if<std::vector<double>>(&m_data[index]);
    if (!vec || sample_index >= vec->size()) {
        MF_ERROR(C, X, "PlotContainer::write_sample: sample index {} out of range in series {}", sample_index, index);
        return;
    }

    Memory::SeqlockWriteGuard g(m_series_locks[index]);
    (*vec)[sample_index] = value;
}

void PlotContainer::resize_series(uint32_t index, uint64_t count)
{
    if (index >= m_data.size()) {
        MF_ERROR(C, X, "PlotContainer::resize_series: index {} out of range", index);
        return;
    }

    auto* vec = std::get_if<std::vector<double>>(&m_data[index]);
    if (!vec)
        return;

    vec->resize(count, 0.0);

    auto* pv = std::get_if<std::vector<double>>(&m_processed_data[index]);
    if (pv)
        pv->resize(count, 0.0);

    m_structure.dimensions[index].size = count;
}

uint32_t PlotContainer::series_count() const
{
    return static_cast<uint32_t>(m_structure.dimensions.size());
}

const std::string& PlotContainer::series_name(uint32_t index) const
{
    static const std::string empty;
    return index < m_structure.dimensions.size()
        ? m_structure.dimensions[index].name
        : empty;
}

uint64_t PlotContainer::series_size(uint32_t index) const
{
    if (index >= m_data.size())
        return 0;
    const auto* vec = std::get_if<std::vector<double>>(&m_data[index]);
    return vec ? static_cast<uint64_t>(vec->size()) : 0;
}

DataDimension::Role PlotContainer::series_role(uint32_t index) const
{
    return index < m_structure.dimensions.size()
        ? m_structure.dimensions[index].role
        : DataDimension::Role::CUSTOM;
}

// =========================================================================
// Source binding
// =========================================================================

void PlotContainer::bind(uint32_t series_index, std::shared_ptr<Nodes::Node> node)
{
    auto& p = ensure_processor();
    p.bind_node(series_index, std::move(node));
    p.set_series_semantics(series_index, m_structure.dimensions[series_index].role,
        m_structure.modality);
    mark_ready_for_processing(true);
}

void PlotContainer::bind(uint32_t series_index,
    std::shared_ptr<Buffers::AudioBuffer> buffer)
{
    auto& p = ensure_processor();
    p.bind_audio_buffer(series_index, std::move(buffer));
    p.set_series_semantics(series_index, m_structure.dimensions[series_index].role,
        m_structure.modality);
    mark_ready_for_processing(true);
}

void PlotContainer::bind(uint32_t series_index,
    std::shared_ptr<Nodes::Network::NodeNetwork> network)
{
    auto& p = ensure_processor();
    p.bind_network(series_index, std::move(network));
    p.set_series_semantics(series_index, m_structure.dimensions[series_index].role,
        m_structure.modality);
    mark_ready_for_processing(true);
}

void PlotContainer::bind(uint32_t series_index,
    std::function<void(std::vector<double>&)> fn)
{
    auto& p = ensure_processor();
    p.bind_callable(series_index, std::move(fn));
    p.set_series_semantics(series_index, m_structure.dimensions[series_index].role,
        m_structure.modality);
    mark_ready_for_processing(true);
}

void PlotContainer::set_raw(uint32_t series_index, std::vector<double> data)
{
    auto& p = ensure_processor();
    p.set_raw(series_index, std::move(data));
    p.set_series_semantics(series_index, m_structure.dimensions[series_index].role,
        m_structure.modality);
    mark_ready_for_processing(true);
}

void PlotContainer::unbind(uint32_t series_index)
{
    if (m_processor)
        std::static_pointer_cast<PlotProcessor>(m_processor)->unbind(series_index);
}

// =========================================================================
// NDDataContainer
// =========================================================================

std::vector<DataDimension> PlotContainer::get_dimensions() const
{
    return m_structure.dimensions;
}

uint64_t PlotContainer::get_total_elements() const
{
    uint64_t total = 0;
    for (const auto& v : m_data) {
        const auto* vec = std::get_if<std::vector<double>>(&v);
        if (vec)
            total += vec->size();
    }
    return total;
}

uint64_t PlotContainer::get_num_frames() const
{
    return static_cast<uint64_t>(m_data.size());
}

std::vector<DataVariant> PlotContainer::get_region_data(const Region& region) const
{
    if (region.start_coordinates.empty() || region.end_coordinates.empty())
        return {};

    const auto series_idx = static_cast<size_t>(region.start_coordinates[0]);
    if (series_idx >= m_data.size())
        return {};

    std::optional<std::vector<DataVariant>> result;
    seqlock_read_void(m_series_locks[series_idx], 8, [&] {
        const auto* src = std::get_if<std::vector<double>>(&m_data[series_idx]);
        if (!src || src->empty())
            return;
        const uint64_t s = (region.start_coordinates.size() > 1) ? region.start_coordinates[1] : 0;
        const uint64_t e = (region.end_coordinates.size() > 1)
            ? std::min(region.end_coordinates[1], static_cast<uint64_t>(src->size() - 1))
            : static_cast<uint64_t>(src->size() - 1);
        if (s > e)
            return;
        std::vector<double> slice(src->begin() + static_cast<ptrdiff_t>(s),
            src->begin() + static_cast<ptrdiff_t>(e + 1));
        result = { DataVariant(std::move(slice)) };
    });

    return result.value_or(std::vector<DataVariant> {});
}

void PlotContainer::set_region_data(const Region& region, const std::vector<DataVariant>& data)
{
    if (data.empty() || region.start_coordinates.empty())
        return;

    const auto series_idx = static_cast<size_t>(region.start_coordinates[0]);
    if (series_idx >= m_data.size())
        return;

    const auto* src = std::get_if<std::vector<double>>(&data[0]);
    if (!src || src->empty())
        return;

    Memory::SeqlockWriteGuard g(m_series_locks[series_idx]);
    auto* dst = std::get_if<std::vector<double>>(&m_data[series_idx]);
    if (!dst)
        return;

    const uint64_t s = (region.start_coordinates.size() > 1) ? region.start_coordinates[1] : 0;
    const uint64_t e = (region.end_coordinates.size() > 1)
        ? std::min(region.end_coordinates[1], static_cast<uint64_t>(dst->size() - 1))
        : static_cast<uint64_t>(dst->size() - 1);
    if (s > e)
        return;

    const uint64_t n = std::min<uint64_t>(src->size(), e - s + 1);
    std::copy_n(src->begin(), n, dst->begin() + static_cast<ptrdiff_t>(s));
}

std::vector<DataVariant> PlotContainer::get_region_group_data(const RegionGroup& group) const
{
    std::vector<DataVariant> result;
    for (const auto& r : group.regions) {
        auto slice = get_region_data(r);
        for (auto& v : slice)
            result.push_back(std::move(v));
    }
    return result;
}

std::vector<DataVariant> PlotContainer::get_segments_data(const std::vector<RegionSegment>&) const
{
    return m_processed_data;
}

uint64_t PlotContainer::coordinates_to_linear_index(const std::vector<uint64_t>& coordinates) const
{
    if (coordinates.size() < 2)
        return 0;
    return coordinates[1];
}

std::vector<uint64_t> PlotContainer::linear_index_to_coordinates(uint64_t linear) const
{
    return { 0, linear };
}

void PlotContainer::clear()
{
    m_data.clear();
    m_processed_data.clear();
    m_structure.dimensions.clear();
    update_processing_state(ProcessingState::IDLE);
}

const void* PlotContainer::get_raw_data() const
{
    if (m_data.empty())
        return nullptr;
    const auto* vec = std::get_if<std::vector<double>>(&m_data[0]);
    return (vec && !vec->empty()) ? vec->data() : nullptr;
}

bool PlotContainer::has_data() const
{
    return !m_data.empty();
}

DataAccess PlotContainer::channel_data(size_t index)
{
    if (index >= m_data.size()) {
        static DataVariant empty = std::vector<double> {};
        static std::vector<DataDimension> empty_dims;
        return { empty, empty_dims, DataModality::TENSOR_ND };
    }
    return { m_data[index], { m_structure.dimensions[index] }, DataModality::TENSOR_ND };
}

std::vector<DataAccess> PlotContainer::all_channel_data()
{
    std::vector<DataAccess> result;
    result.reserve(m_data.size());
    for (size_t i = 0; i < m_data.size(); ++i) {
        result.emplace_back(m_data[i],
            std::vector<DataDimension> { m_structure.dimensions[i] },
            DataModality::TENSOR_ND);
    }

    return result;
}

void PlotContainer::add_region_group(const RegionGroup& group)
{
    Memory::SeqlockWriteGuard g(m_region_lock);
    m_region_groups[group.name] = group;
}

RegionGroup PlotContainer::get_region_group(const std::string& name) const
{
    static const RegionGroup empty;
    std::optional<RegionGroup> result;
    seqlock_read_void(m_region_lock, 8, [&] {
        auto it = m_region_groups.find(name);
        result = (it != m_region_groups.end()) ? it->second : empty;
    });
    return result.value_or(empty);
}

std::unordered_map<std::string, RegionGroup> PlotContainer::get_all_region_groups() const
{
    std::optional<std::unordered_map<std::string, RegionGroup>> result;
    seqlock_read_void(m_region_lock, 8, [&] {
        result = m_region_groups;
    });
    return result.value_or(std::unordered_map<std::string, RegionGroup> {});
}

void PlotContainer::remove_region_group(const std::string& name)
{
    Memory::SeqlockWriteGuard g(m_region_lock);
    m_region_groups.erase(name);
}

// =========================================================================
// SignalSourceContainer
// =========================================================================

ProcessingState PlotContainer::get_processing_state() const
{
    return m_processing_state.load(std::memory_order_acquire);
}

void PlotContainer::update_processing_state(ProcessingState state)
{
    ProcessingState prev = m_processing_state.exchange(state);
    if (prev == state)
        return;

    seqlock_read_void(m_cb_lock, 8, [&] {
        if (m_state_cb)
            m_state_cb(shared_from_this(), state);
    });
}

void PlotContainer::register_state_change_callback(
    std::function<void(const std::shared_ptr<SignalSourceContainer>&, ProcessingState)> cb)
{
    Memory::SeqlockWriteGuard g(m_cb_lock);
    m_state_cb = std::move(cb);
}

void PlotContainer::unregister_state_change_callback()
{
    Memory::SeqlockWriteGuard g(m_cb_lock);
    m_state_cb = nullptr;
}

bool PlotContainer::is_ready_for_processing() const
{
    const auto s = get_processing_state();
    return has_data() && (s == ProcessingState::READY || s == ProcessingState::PROCESSED);
}

void PlotContainer::mark_ready_for_processing(bool ready)
{
    update_processing_state(ready ? ProcessingState::READY : ProcessingState::IDLE);
}

void PlotContainer::create_default_processor()
{
    set_default_processor(std::make_shared<PlotProcessor>());
}

void PlotContainer::process_default()
{
    if (!m_processor || !is_ready_for_processing())
        return;
    update_processing_state(ProcessingState::PROCESSING);
    m_processor->process(shared_from_this());
    update_processing_state(ProcessingState::PROCESSED);
}

void PlotContainer::set_default_processor(const std::shared_ptr<DataProcessor>& processor)
{
    if (m_processor)
        m_processor->on_detach(shared_from_this());
    m_processor = processor;
    if (m_processor)
        m_processor->on_attach(shared_from_this());
}

std::shared_ptr<DataProcessor> PlotContainer::get_default_processor() const
{
    return m_processor;
}

auto PlotContainer::get_frame_typed(uint64_t frame_index) const -> std::span<const double>
{
    if (frame_index >= m_data.size())
        return {};

    std::span<const double> result;
    seqlock_read_void(m_series_locks[frame_index], 8, [&] {
        const auto* vec = std::get_if<std::vector<double>>(&m_data[frame_index]);
        if (!vec || vec->empty())
            return;
        result = { vec->data(), vec->size() };
    });
    return result;
}

void PlotContainer::get_frames_typed(std::span<double> output, uint64_t start_frame, uint64_t num_frames) const
{
    size_t out = 0;
    for (uint64_t i = start_frame; i < start_frame + num_frames && i < m_data.size(); ++i) {
        seqlock_read_void(m_series_locks[i], 8, [&] {
            const auto* vec = std::get_if<std::vector<double>>(&m_data[i]);
            if (!vec)
                return;
            for (double v : *vec) {
                if (out >= output.size())
                    return;
                output[out++] = v;
            }
        });
    }
}

auto PlotContainer::get_frame_span_impl(uint64_t frame_index) const -> DataSpanVariant
{
    return { get_frame_typed(frame_index) };
}

void PlotContainer::get_frames_impl(
    void* output,
    size_t count,
    uint64_t start_frame,
    uint64_t num_frames,
    const std::type_info& type) const
{
    if (type != typeid(double)) {
        error<std::runtime_error>(
            Journal::Component::Kakshya,
            Journal::Context::Runtime,
            std::source_location::current(),
            "PlotContainer only supports double");
    }

    get_frames_typed(std::span<double>(static_cast<double*>(output), count), start_frame, num_frames);
}

void PlotContainer::get_value_impl(
    const std::vector<uint64_t>& coords,
    void* out,
    const std::type_info& type) const
{
    if (type != typeid(double) || coords.size() < 2 || coords[0] >= m_data.size())
        return;

    const auto idx = static_cast<size_t>(coords[0]);

    seqlock_read_void(m_series_locks[idx], 8, [&] {
        const auto* vec = std::get_if<std::vector<double>>(&m_data[idx]);
        if (!vec || coords[1] >= vec->size())
            return;
        *static_cast<double*>(out) = (*vec)[coords[1]];
    });
}

void PlotContainer::set_value_impl(
    const std::vector<uint64_t>& coords,
    const void* in,
    const std::type_info& type)
{
    if (type != typeid(double) || coords.size() < 2 || coords[0] >= m_data.size())
        return;

    const auto idx = static_cast<size_t>(coords[0]);

    Memory::SeqlockWriteGuard g(m_series_locks[idx]);
    auto* vec = std::get_if<std::vector<double>>(&m_data[idx]);
    if (!vec || coords[1] >= vec->size())
        return;

    (*vec)[coords[1]] = *static_cast<const double*>(in);
}

} // namespace MayaFlux::Kakshya
