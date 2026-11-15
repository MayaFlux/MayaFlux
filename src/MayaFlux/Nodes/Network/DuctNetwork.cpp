#include "DuctNetwork.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Transitive/Reflect/EnumReflect.hpp"

namespace MayaFlux::Nodes::Network {

namespace Acoustics = Kinesis::Acoustics;

namespace {

    constexpr uint8_t k_empty = 0;
    constexpr uint8_t k_ready = 1;
    constexpr uint8_t k_busy = 2;
    constexpr uint8_t k_retired = 3;

}

struct DuctNetwork::Prepared {
    Acoustics::DuctGrid grid;
    Acoustics::DuctSpec spec;
    Acoustics::DuctLoss loss;
    double friction {};
    double viscosity {};
    std::vector<double> pressure;
    std::vector<double> velocity;
    bool keep_state { false };
    std::shared_ptr<TerminationOperator> mouth;
    std::shared_ptr<TerminationOperator> bell;
};

//-----------------------------------------------------------------------------
// Construction
//-----------------------------------------------------------------------------

DuctNetwork::DuctNetwork(double length, double radius, double sample_rate)
    : DuctNetwork(length, std::vector<double> { radius, radius }, sample_rate)
{
}

DuctNetwork::DuctNetwork(double length, const std::vector<double>& radii, double sample_rate)
{
    setup(length, Profile { .radii = radii, .function = {} }, sample_rate);
}

DuctNetwork::DuctNetwork(double length, std::function<double(double)> radius, double sample_rate)
{
    setup(length, Profile { .radii = {}, .function = std::move(radius) }, sample_rate);
}

DuctNetwork::~DuctNetwork() = default;

void DuctNetwork::setup(double length, Profile profile, double sample_rate)
{
    m_sample_rate = static_cast<uint32_t>(sample_rate);
    set_output_mode(OutputMode::AUDIO_SINK);
    set_topology(Topology::CHAIN);

    std::scoped_lock lock(m_producer_mutex);
    m_length = length;
    m_rate = sample_rate;
    m_profile = std::move(profile);
    m_mouth = std::make_shared<ReedOperator>();
    m_bell = std::make_shared<RadiationOperator>();

    m_operator_chain->add(m_mouth);
    m_operator_chain->add(m_bell);

    if (auto prepared = build(m_length, m_profile, m_losses, m_mouth_volume, m_mouth, m_bell)) {
        m_applied_friction = prepared->friction;
        m_applied_viscosity = prepared->viscosity;
        m_intervals = prepared->grid.intervals;
        m_courant = prepared->grid.courant;
        m_bandwidth = prepared->grid.bandwidth;
        m_field_points.store(prepared->grid.intervals + 1, std::memory_order_relaxed);
        m_field.reserve(prepared->grid.intervals + 1);
        m_active = std::move(prepared);
    }
}

//-----------------------------------------------------------------------------
// Building and installing grids
//-----------------------------------------------------------------------------

std::shared_ptr<DuctNetwork::Prepared> DuctNetwork::build(
    double length,
    const Profile& profile,
    const Losses& losses,
    double mouth_volume,
    const std::shared_ptr<TerminationOperator>& mouth,
    const std::shared_ptr<TerminationOperator>& bell) const
{
    const Acoustics::DuctSpec spec { .length = length, .sample_rate = m_rate, .mouth_volume = mouth_volume };

    std::expected<Acoustics::DuctGrid, Acoustics::DuctError> grid = std::unexpected(Acoustics::DuctError::INVALID_PROFILE);
    if (!profile.radii.empty()) {
        grid = Acoustics::make_duct_grid(spec, profile.radii);
    } else if (profile.function) {
        grid = Acoustics::make_duct_grid(spec, profile.function);
    }

    if (!grid) {
        MF_WARN(Journal::Component::Nodes, Journal::Context::Configuration,
            "DuctNetwork could not build a grid: {}", Reflect::enum_to_string(grid.error()));
        return nullptr;
    }

    if (!mouth || !bell
        || !mouth->accepts(*grid, spec, DuctEnd::MOUTH)
        || !bell->accepts(*grid, spec, DuctEnd::BELL)) {
        MF_WARN(Journal::Component::Nodes, Journal::Context::Configuration,
            "DuctNetwork terminations cannot terminate the {} interval duct", grid->intervals);
        return nullptr;
    }

    auto prepared = std::make_shared<Prepared>();
    prepared->spec = spec;
    prepared->pressure.assign(grid->intervals + 1, 0.0);
    prepared->velocity.assign(grid->intervals, 0.0);
    prepared->grid = std::move(*grid);
    prepared->mouth = mouth;
    prepared->bell = bell;

    prepared->friction = losses.friction;
    prepared->viscosity = losses.viscosity;
    if (losses.automatic) {
        const auto& areas = prepared->grid.area;
        const double mean_area = std::accumulate(areas.begin(), areas.end(), 0.0) / static_cast<double>(areas.size());
        std::tie(prepared->friction, prepared->viscosity) = Acoustics::viscothermal_rates(
            std::sqrt(mean_area / std::numbers::pi), spec.sound_speed, spec.air_density);
    }

    if (!prepared->loss.configure(prepared->grid, prepared->friction, prepared->viscosity)) {
        MF_WARN(Journal::Component::Nodes, Journal::Context::Configuration,
            "DuctNetwork wall losses of {} and {} cannot be applied", prepared->friction, prepared->viscosity);
        return nullptr;
    }

    return prepared;
}

void DuctNetwork::publish(std::shared_ptr<Prepared> next)
{
    m_intervals = next->grid.intervals;
    m_courant = next->grid.courant;
    m_bandwidth = next->grid.bandwidth;
    m_applied_friction = next->friction;
    m_applied_viscosity = next->viscosity;

    while (m_audio_buffer_lock.test_and_set(std::memory_order_acquire)) {
        std::this_thread::yield();
    }
    m_field.reserve(next->grid.intervals + 1);
    m_audio_buffer_lock.clear(std::memory_order_release);

    for (;;) {
        uint8_t state = m_handoff.load(std::memory_order_acquire);

        if (state == k_retired) {
            m_slot.reset();
            m_handoff.store(k_empty, std::memory_order_release);
        } else if (state == k_ready) {
            if (m_handoff.compare_exchange_strong(state, k_busy, std::memory_order_acq_rel)) {
                next->keep_state = next->keep_state && m_slot->keep_state;
                m_slot = std::move(next);
                m_handoff.store(k_ready, std::memory_order_release);
                return;
            }
        } else if (state == k_busy) {
            std::this_thread::yield();
        } else {
            m_slot = std::move(next);
            m_handoff.store(k_ready, std::memory_order_release);
            return;
        }
    }
}

void DuctNetwork::install_pending()
{
    uint8_t expected = k_ready;
    if (!m_handoff.compare_exchange_strong(expected, k_busy, std::memory_order_acq_rel)) {
        return;
    }

    std::swap(m_active, m_slot);
    m_handoff.store(k_retired, std::memory_order_release);

    const bool carry = m_active->keep_state && m_slot && !m_slot->pressure.empty();
    if (carry) {
        Acoustics::resample_line(m_slot->pressure, m_active->pressure, 0.0);
        Acoustics::resample_line(m_slot->velocity, m_active->velocity, 0.5);
    }
    bind(*m_active, carry);
}

void DuctNetwork::bind(Prepared& prepared, bool keep)
{
    if (keep) {
        prepared.mouth->retarget(prepared.grid, prepared.spec, DuctEnd::MOUTH);
        prepared.bell->retarget(prepared.grid, prepared.spec, DuctEnd::BELL);
    } else {
        prepared.mouth->prepare(prepared.grid, prepared.spec, DuctEnd::MOUTH);
        prepared.bell->prepare(prepared.grid, prepared.spec, DuctEnd::BELL);
        m_previous_outflow = 0.0;
    }
    m_field_points.store(prepared.grid.intervals + 1, std::memory_order_relaxed);
}

void DuctNetwork::initialize()
{
    if (m_active) {
        bind(*m_active, false);
    }
}

void DuctNetwork::reset()
{
    std::scoped_lock lock(m_producer_mutex);
    if (auto next = build(m_length, m_profile, m_losses, m_mouth_volume, m_mouth, m_bell)) {
        publish(std::move(next));
    }
}

//-----------------------------------------------------------------------------
// Shape and terminations
//-----------------------------------------------------------------------------

bool DuctNetwork::set_length(double length)
{
    std::scoped_lock lock(m_producer_mutex);
    auto next = build(length, m_profile, m_losses, m_mouth_volume, m_mouth, m_bell);
    if (!next) {
        return false;
    }

    next->keep_state = true;

    m_length = length;
    publish(std::move(next));
    return true;
}

bool DuctNetwork::set_radius(double radius)
{
    return set_profile(std::vector<double> { radius, radius });
}

bool DuctNetwork::set_profile(const std::vector<double>& radii)
{
    std::scoped_lock lock(m_producer_mutex);
    Profile profile { .radii = radii, .function = {} };
    auto next = build(m_length, profile, m_losses, m_mouth_volume, m_mouth, m_bell);
    if (!next) {
        return false;
    }

    next->keep_state = true;

    m_profile = std::move(profile);
    publish(std::move(next));
    return true;
}

bool DuctNetwork::set_profile(std::function<double(double)> radius)
{
    std::scoped_lock lock(m_producer_mutex);
    Profile profile { .radii = {}, .function = std::move(radius) };
    auto next = build(m_length, profile, m_losses, m_mouth_volume, m_mouth, m_bell);
    if (!next) {
        return false;
    }

    next->keep_state = true;

    m_profile = std::move(profile);
    publish(std::move(next));
    return true;
}

bool DuctNetwork::set_mouth_volume(double volume)
{
    std::scoped_lock lock(m_producer_mutex);
    auto next = build(m_length, m_profile, m_losses, volume, m_mouth, m_bell);
    if (!next) {
        return false;
    }

    next->keep_state = true;

    m_mouth_volume = volume;
    publish(std::move(next));
    return true;
}

bool DuctNetwork::set_wall_losses(double friction, double viscosity)
{
    std::scoped_lock lock(m_producer_mutex);
    const Losses losses { .automatic = false, .friction = friction, .viscosity = viscosity };
    auto next = build(m_length, m_profile, losses, m_mouth_volume, m_mouth, m_bell);
    if (!next) {
        return false;
    }

    next->keep_state = true;

    m_losses = losses;
    publish(std::move(next));
    return true;
}

bool DuctNetwork::use_viscothermal_losses()
{
    std::scoped_lock lock(m_producer_mutex);
    const Losses losses { .automatic = true };
    auto next = build(m_length, m_profile, losses, m_mouth_volume, m_mouth, m_bell);
    if (!next) {
        return false;
    }

    next->keep_state = true;

    m_losses = losses;
    publish(std::move(next));
    return true;
}

bool DuctNetwork::set_mouth(std::shared_ptr<TerminationOperator> termination)
{
    std::scoped_lock lock(m_producer_mutex);
    auto next = build(m_length, m_profile, m_losses, m_mouth_volume, termination, m_bell);
    if (!next) {
        return false;
    }

    next->keep_state = true;

    m_operator_chain->remove(m_mouth);
    m_mouth = std::move(termination);
    m_operator_chain->add(m_mouth);
    publish(std::move(next));
    return true;
}

bool DuctNetwork::set_bell(std::shared_ptr<TerminationOperator> termination)
{
    std::scoped_lock lock(m_producer_mutex);
    auto next = build(m_length, m_profile, m_losses, m_mouth_volume, m_mouth, termination);
    if (!next) {
        return false;
    }

    next->keep_state = true;

    m_operator_chain->remove(m_bell);
    m_bell = std::move(termination);
    m_operator_chain->add(m_bell);
    publish(std::move(next));
    return true;
}

std::shared_ptr<TerminationOperator> DuctNetwork::get_mouth() const
{
    std::scoped_lock lock(m_producer_mutex);
    return m_mouth;
}

std::shared_ptr<TerminationOperator> DuctNetwork::get_bell() const
{
    std::scoped_lock lock(m_producer_mutex);
    return m_bell;
}

void DuctNetwork::set_pickup_position(double position)
{
    m_pickup.store(std::clamp(position, 0.0, 1.0), std::memory_order_relaxed);
}

//-----------------------------------------------------------------------------
// Processing
//-----------------------------------------------------------------------------

void DuctNetwork::process_batch(unsigned int num_samples)
{
    ensure_initialized();
    install_pending();

    if (!is_enabled() || !m_active || num_samples == 0) {
        m_scratch.assign(num_samples, 0.0);
        publish_block(m_scratch);
        return;
    }

    update_mapped_parameters();

    Prepared& state = *m_active;
    state.mouth->process(static_cast<float>(num_samples));
    state.bell->process(static_cast<float>(num_samples));

    const Output output = m_output.load(std::memory_order_relaxed);
    const double pickup = m_pickup.load(std::memory_order_relaxed);
    const Acoustics::DuctGrid& grid = state.grid;

    m_scratch.assign(num_samples, 0.0);
    for (unsigned int i = 0; i < num_samples; ++i) {
        Acoustics::advance_velocity(state.velocity, state.pressure, grid.velocity_gain);
        state.loss.apply(state.velocity);

        const auto [mouth_pressure, mouth_inflow] = state.mouth->couple(state.pressure.front(), state.velocity.front());
        Acoustics::advance_pressure(state.pressure, state.velocity, grid.pressure_gain);
        state.pressure.front() = mouth_pressure;

        const auto [bell_pressure, bell_inflow] = state.bell->couple(state.pressure.back(), -state.velocity.back());
        state.pressure.back() = bell_pressure;

        const double outflow = -bell_inflow;
        m_scratch[i] = output == Output::RADIATED
            ? Acoustics::radiated_pressure(outflow, m_previous_outflow, grid.time_step, state.spec.air_density)
            : Acoustics::sample_pressure(state.pressure, pickup);
        m_previous_outflow = outflow;
    }

    publish_block(m_scratch);
}

void DuctNetwork::publish_block(std::span<const double> block)
{
    while (m_audio_buffer_lock.test_and_set(std::memory_order_acquire)) {
        std::this_thread::yield();
    }

    m_last_audio_buffer.assign(block.begin(), block.end());
    apply_output_scale();
    m_last_output = m_last_audio_buffer.empty() ? 0.0 : m_last_audio_buffer.back();

    if (m_active) {
        m_field.assign(m_active->pressure.begin(), m_active->pressure.end());
    }

    m_audio_buffer_lock.clear(std::memory_order_release);
}

//-----------------------------------------------------------------------------
// Parameter mapping
//-----------------------------------------------------------------------------

void DuctNetwork::update_mapped_parameters()
{
    for (const auto& mapping : m_parameter_mappings) {
        if (mapping.mode != MappingMode::BROADCAST || !mapping.broadcast_source) {
            continue;
        }

        const ParameterKey key { .name = mapping.param_name, .hash = mapping.param_hash };
        const double value = mapping.broadcast_source->get_last_output();

        if (key.matches("pickup")) {
            set_pickup_position(value);
        } else if (key.matches("scale")) {
            m_output_scale = std::max(0.0, value);
        } else {
            m_active->mouth->set_mapped_parameter(key, value);
            m_active->bell->set_mapped_parameter(key, value);
        }
    }
}

void DuctNetwork::map_parameter(
    const std::string& param_name,
    const std::shared_ptr<Node>& source,
    MappingMode mode)
{
    unmap_parameter(param_name);

    ParameterMapping mapping;
    mapping.param_name = param_name;
    mapping.mode = mode;
    mapping.param_hash = parameter_hash(param_name);
    mapping.broadcast_source = source;
    mapping.network_source = nullptr;

    m_parameter_mappings.push_back(std::move(mapping));
}

void DuctNetwork::map_parameter(
    const std::string& param_name,
    const std::shared_ptr<NodeNetwork>& source)
{
    unmap_parameter(param_name);

    ParameterMapping mapping;
    mapping.param_name = param_name;
    mapping.mode = MappingMode::ONE_TO_ONE;
    mapping.param_hash = parameter_hash(param_name);
    mapping.broadcast_source = nullptr;
    mapping.network_source = source;

    m_parameter_mappings.push_back(std::move(mapping));
}

void DuctNetwork::unmap_parameter(const std::string& param_name)
{
    std::erase_if(m_parameter_mappings,
        [&](const auto& mapping) { return mapping.param_name == param_name; });
}

//-----------------------------------------------------------------------------
// Observation
//-----------------------------------------------------------------------------

std::optional<double> DuctNetwork::get_node_output(size_t index) const
{
    while (m_audio_buffer_lock.test_and_set(std::memory_order_acquire)) {
        std::this_thread::yield();
    }
    const std::optional<double> value = index < m_field.size()
        ? std::optional<double> { m_field[index] }
        : std::nullopt;
    m_audio_buffer_lock.clear(std::memory_order_release);
    return value;
}

std::optional<std::span<const double>> DuctNetwork::get_node_audio_buffer(size_t index) const
{
    if (index != 0 || m_last_audio_buffer.empty()) {
        return std::nullopt;
    }
    return std::span<const double>(m_last_audio_buffer);
}

std::vector<double> DuctNetwork::pressure_field() const
{
    while (m_audio_buffer_lock.test_and_set(std::memory_order_acquire)) {
        std::this_thread::yield();
    }
    std::vector<double> copy = m_field;
    m_audio_buffer_lock.clear(std::memory_order_release);
    return copy;
}

std::unordered_map<std::string, std::string> DuctNetwork::get_metadata() const
{
    auto metadata = NodeNetwork::get_metadata();

    std::scoped_lock lock(m_producer_mutex);
    metadata["length"] = std::to_string(m_length) + " m";
    metadata["intervals"] = std::to_string(m_intervals);
    metadata["courant"] = std::to_string(m_courant);
    metadata["bandwidth"] = std::to_string(m_bandwidth) + " Hz";
    metadata["friction"] = std::to_string(m_applied_friction) + " 1/s";
    metadata["viscosity"] = std::to_string(m_applied_viscosity) + " m^2/s";
    metadata["mouth"] = std::string(m_mouth->get_type_name());
    metadata["bell"] = std::string(m_bell->get_type_name());
    metadata["output"] = std::string(Reflect::enum_to_string(m_output.load(std::memory_order_relaxed)));
    return metadata;
}

} // namespace MayaFlux::Nodes::Network
