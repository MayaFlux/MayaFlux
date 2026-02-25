#include "WaveguideNetwork.hpp"

#include "MayaFlux/Nodes/Filters/FIR.hpp"
#include "MayaFlux/Nodes/Filters/Filter.hpp"
#include "MayaFlux/Transitive/Reflect/EnumReflect.hpp"

namespace MayaFlux::Nodes::Network {

//-----------------------------------------------------------------------------
// Construction
//-----------------------------------------------------------------------------

WaveguideNetwork::WaveguideNetwork(
    WaveguideType type,
    double fundamental_freq,
    double sample_rate)
    : m_type(type)
    , m_fundamental(fundamental_freq)
{
    m_sample_rate = static_cast<uint32_t>(sample_rate);
    set_output_mode(OutputMode::AUDIO_SINK);
    set_topology(Topology::RING);

    compute_delay_length();

    const auto prop_mode = (m_type == WaveguideType::TUBE)
        ? WaveguideSegment::PropagationMode::BIDIRECTIONAL
        : WaveguideSegment::PropagationMode::UNIDIRECTIONAL;

    m_segments.emplace_back(m_delay_length_integer + 2, prop_mode);

    create_default_loop_filter();
}

//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------

void WaveguideNetwork::initialize()
{
    compute_delay_length();
}

void WaveguideNetwork::reset()
{
    for (auto& seg : m_segments) {
        const auto len = seg.p_plus.capacity();
        seg.p_plus = Memory::HistoryBuffer<double>(len);
        seg.p_minus = Memory::HistoryBuffer<double>(len);
    }

    m_exciter_active = false;
    m_exciter_samples_remaining = 0;
    m_exciter_sample_position = 0;
    m_last_output = 0.0;
    m_last_audio_buffer.clear();
}

//-----------------------------------------------------------------------------
// Delay Length Computation
//-----------------------------------------------------------------------------

void WaveguideNetwork::compute_delay_length()
{
    double total_delay = static_cast<double>(m_sample_rate) / m_fundamental;

    total_delay -= 0.5;

    m_delay_length_integer = static_cast<size_t>(total_delay);
    m_delay_length_fraction = total_delay - static_cast<double>(m_delay_length_integer);

    m_pickup_sample = m_delay_length_integer / 2;
}

//-----------------------------------------------------------------------------
// Default Loop Filter
//-----------------------------------------------------------------------------

void WaveguideNetwork::create_default_loop_filter()
{
    auto filter = std::make_shared<Filters::FIR>(
        std::vector<double> { 0.5, 0.5 });

    if (!m_segments.empty()) {
        m_segments[0].loop_filter = filter;
    }
}

//-----------------------------------------------------------------------------
// Fractional Delay Interpolation
//-----------------------------------------------------------------------------

double WaveguideNetwork::read_with_interpolation(
    const Memory::HistoryBuffer<double>& delay,
    size_t integer_part,
    double fraction) const
{
    double s0 = delay[integer_part];
    double s1 = delay[integer_part + 1];
    return s0 + fraction * (s1 - s0);
}

//-----------------------------------------------------------------------------
// Processing
//-----------------------------------------------------------------------------

double WaveguideNetwork::observe_sample(const WaveguideSegment& seg) const
{
    if (seg.mode == WaveguideSegment::PropagationMode::UNIDIRECTIONAL) {
        return seg.p_plus[m_pickup_sample];
    }

    const double plus = seg.p_plus[m_pickup_sample];
    const double minus = seg.p_minus[m_pickup_sample];

    if (m_measurement_mode == MeasurementMode::PRESSURE)
        return plus + minus;

    return plus - minus;
}

void WaveguideNetwork::process_batch(unsigned int num_samples)
{
    ensure_initialized();
    m_last_audio_buffer.clear();

    if (!is_enabled() || m_segments.empty()) {
        m_last_audio_buffer.assign(num_samples, 0.0);
        m_last_output = 0.0;
        return;
    }

    update_mapped_parameters();
    m_last_audio_buffer.reserve(num_samples);

    auto& seg = m_segments[0];

    if (seg.mode == WaveguideSegment::PropagationMode::UNIDIRECTIONAL) {
        process_unidirectional(seg, num_samples);
    } else {
        process_bidirectional(seg, num_samples);
    }

    apply_output_scale();
    m_last_output = m_last_audio_buffer.back();
}

void WaveguideNetwork::process_unidirectional(WaveguideSegment& seg,
    unsigned int num_samples)
{
    for (unsigned int i = 0; i < num_samples; ++i) {
        const double exciter = generate_exciter_sample();

        const double delayed = read_with_interpolation(
            seg.p_plus, m_delay_length_integer, m_delay_length_fraction);

        const double filtered = seg.loop_filter ? seg.loop_filter->process_sample(delayed)
                                                : delayed;

        seg.p_plus.push(
            exciter + filtered * seg.loss_factor * seg.reflection_closed);

        m_last_audio_buffer.push_back(observe_sample(seg));
    }
}

void WaveguideNetwork::process_bidirectional(WaveguideSegment& seg,
    unsigned int num_samples)
{
    for (unsigned int i = 0; i < num_samples; ++i) {
        const double exciter = generate_exciter_sample();

        const double plus_end = read_with_interpolation(
            seg.p_plus, m_delay_length_integer, m_delay_length_fraction);

        const double minus_end = read_with_interpolation(
            seg.p_minus, m_delay_length_integer, m_delay_length_fraction);

        auto* filt_open = seg.loop_filter_open ? seg.loop_filter_open.get()
                                               : seg.loop_filter.get();
        auto* filt_closed = seg.loop_filter_closed ? seg.loop_filter_closed.get()
                                                   : seg.loop_filter.get();

        const double filtered_plus = filt_open ? filt_open->process_sample(plus_end)
                                               : plus_end;
        const double filtered_minus = filt_closed ? filt_closed->process_sample(minus_end)
                                                  : minus_end;

        seg.p_minus.push(filtered_plus * seg.loss_factor * seg.reflection_open);
        seg.p_plus.push(exciter + filtered_minus * seg.loss_factor * seg.reflection_closed);

        m_last_audio_buffer.push_back(observe_sample(seg));
    }
}

//-----------------------------------------------------------------------------
// Excitation
//-----------------------------------------------------------------------------

void WaveguideNetwork::pluck(double position, double strength)
{
    position = std::clamp(position, 0.01, 0.99);

    if (m_segments.empty()) {
        return;
    }

    auto& seg = m_segments[0];
    const size_t len = m_delay_length_integer;
    const auto pluck_sample = static_cast<size_t>(position * static_cast<double>(len));

    seg.p_plus = Memory::HistoryBuffer<double>(seg.p_plus.capacity());
    seg.p_minus = Memory::HistoryBuffer<double>(seg.p_minus.capacity());

    for (size_t s = 0; s < len; ++s) {
        double value = 0.0;
        if (s <= pluck_sample) {
            value = strength * static_cast<double>(s)
                / static_cast<double>(pluck_sample);
        } else {
            value = strength * static_cast<double>(len - s)
                / static_cast<double>(len - pluck_sample);
        }
        seg.p_plus.push(value);
    }

    m_exciter_active = false;
    m_exciter_samples_remaining = 0;
}

void WaveguideNetwork::strike(double position, double strength)
{
    position = std::clamp(position, 0.01, 0.99);

    m_exciter_type = ExciterType::NOISE_BURST;
    m_exciter_duration = 0.002;

    initialize_exciter();

    if (m_segments.empty()) {
        return;
    }

    auto& seg = m_segments[0];
    const size_t len = m_delay_length_integer;
    const auto strike_center = static_cast<size_t>(position * static_cast<double>(len));
    const size_t burst_width = std::max<size_t>(len / 10, 4);

    seg.p_plus = Memory::HistoryBuffer<double>(seg.p_plus.capacity());
    seg.p_minus = Memory::HistoryBuffer<double>(seg.p_minus.capacity());

    for (size_t s = 0; s < len; ++s) {
        const double dist = std::abs(static_cast<double>(s)
            - static_cast<double>(strike_center));
        const double window = std::exp(-(dist * dist)
            / (2.0 * static_cast<double>(burst_width * burst_width)));
        seg.p_plus.push(strength * m_random_generator(-1.0, 1.0) * window);
    }

    m_exciter_active = false;
    m_exciter_samples_remaining = 0;
}

void WaveguideNetwork::set_exciter_duration(double seconds)
{
    m_exciter_duration = std::max(0.001, seconds);
}

void WaveguideNetwork::set_exciter_sample(const std::vector<double>& sample)
{
    m_exciter_sample = sample;
}

void WaveguideNetwork::initialize_exciter()
{
    m_exciter_active = true;
    m_exciter_sample_position = 0;

    switch (m_exciter_type) {
    case ExciterType::IMPULSE:
        m_exciter_samples_remaining = 1;
        break;

    case ExciterType::NOISE_BURST:
    case ExciterType::FILTERED_NOISE:
        m_exciter_samples_remaining = static_cast<size_t>(
            m_exciter_duration * static_cast<double>(m_sample_rate));
        break;

    case ExciterType::SAMPLE:
        m_exciter_samples_remaining = m_exciter_sample.size();
        break;

    case ExciterType::CONTINUOUS:
        m_exciter_samples_remaining = std::numeric_limits<size_t>::max();
        break;
    }
}

double WaveguideNetwork::generate_exciter_sample()
{
    if (!m_exciter_active || m_exciter_samples_remaining == 0) {
        m_exciter_active = false;
        return 0.0;
    }

    --m_exciter_samples_remaining;
    double sample = 0.0;

    switch (m_exciter_type) {
    case ExciterType::IMPULSE:
        sample = 1.0;
        break;

    case ExciterType::NOISE_BURST:
        sample = m_random_generator(-1.0, 1.0);
        break;

    case ExciterType::FILTERED_NOISE: {
        double noise = m_random_generator(-1.0, 1.0);
        sample = m_exciter_filter ? m_exciter_filter->process_sample(noise) : noise;
        break;
    }

    case ExciterType::SAMPLE:
        if (m_exciter_sample_position < m_exciter_sample.size()) {
            sample = m_exciter_sample[m_exciter_sample_position++];
        }
        break;

    case ExciterType::CONTINUOUS:
        if (m_exciter_node) {
            sample = m_exciter_node->process_sample(0.0);
        }
        break;
    }

    return sample;
}

//-----------------------------------------------------------------------------
// Waveguide Control
//-----------------------------------------------------------------------------

void WaveguideNetwork::set_fundamental(double freq)
{
    m_fundamental = std::max(20.0, freq);
    compute_delay_length();

    if (!m_segments.empty()) {
        const size_t required = m_delay_length_integer + 2;
        auto& seg = m_segments[0];
        if (seg.p_plus.capacity() < required) {
            seg.p_plus = Memory::HistoryBuffer<double>(required);
            seg.p_minus = Memory::HistoryBuffer<double>(required);
        }
    }
}

void WaveguideNetwork::set_loss_factor(double loss)
{
    loss = std::clamp(loss, 0.0, 1.0);
    for (auto& seg : m_segments) {
        seg.loss_factor = loss;
    }
}

double WaveguideNetwork::get_loss_factor() const
{
    return m_segments.empty() ? 0.996 : m_segments[0].loss_factor;
}

void WaveguideNetwork::set_loop_filter(const std::shared_ptr<Filters::Filter>& filter)
{
    if (!m_segments.empty()) {
        m_segments[0].loop_filter = filter;
    }
}

void WaveguideNetwork::set_pickup_position(double position)
{
    position = std::clamp(position, 0.0, 1.0);
    m_pickup_sample = static_cast<size_t>(position * static_cast<double>(m_delay_length_integer));
    m_pickup_sample = std::clamp(m_pickup_sample, size_t { 0 }, m_delay_length_integer);
}

double WaveguideNetwork::get_pickup_position() const
{
    if (m_delay_length_integer == 0) {
        return 0.5;
    }
    return static_cast<double>(m_pickup_sample) / static_cast<double>(m_delay_length_integer);
}

void WaveguideNetwork::set_loop_filter_open(const std::shared_ptr<Filters::Filter>& filter)
{
    if (!m_segments.empty()) {
        m_segments[0].loop_filter_open = filter;
    }
}

void WaveguideNetwork::set_loop_filter_closed(const std::shared_ptr<Filters::Filter>& filter)
{
    if (!m_segments.empty()) {
        m_segments[0].loop_filter_closed = filter;
    }
}

//-----------------------------------------------------------------------------
// Parameter Mapping
//-----------------------------------------------------------------------------

void WaveguideNetwork::update_mapped_parameters()
{
    for (const auto& mapping : m_parameter_mappings) {
        if (mapping.mode == MappingMode::BROADCAST && mapping.broadcast_source) {
            double value = mapping.broadcast_source->get_last_output();
            apply_broadcast_parameter(mapping.param_name, value);
        } else if (mapping.mode == MappingMode::ONE_TO_ONE && mapping.network_source) {
            apply_one_to_one_parameter(mapping.param_name, mapping.network_source);
        }
    }
}

void WaveguideNetwork::apply_broadcast_parameter(const std::string& param, double value)
{
    if (param == "frequency") {
        set_fundamental(value);
    } else if (param == "damping" || param == "loss") {
        set_loss_factor(value);
    } else if (param == "position") {
        set_pickup_position(value);
    } else if (param == "scale") {
        m_output_scale = std::max(0.0, value);
    }
}

void WaveguideNetwork::apply_one_to_one_parameter(
    const std::string& /*param*/,
    const std::shared_ptr<NodeNetwork>& /*source*/)
{
}

void WaveguideNetwork::map_parameter(
    const std::string& param_name,
    const std::shared_ptr<Node>& source,
    MappingMode mode)
{
    unmap_parameter(param_name);

    ParameterMapping mapping;
    mapping.param_name = param_name;
    mapping.mode = mode;
    mapping.broadcast_source = source;
    mapping.network_source = nullptr;

    m_parameter_mappings.push_back(std::move(mapping));
}

void WaveguideNetwork::map_parameter(
    const std::string& param_name,
    const std::shared_ptr<NodeNetwork>& source_network)
{
    unmap_parameter(param_name);

    ParameterMapping mapping;
    mapping.param_name = param_name;
    mapping.mode = MappingMode::ONE_TO_ONE;
    mapping.broadcast_source = nullptr;
    mapping.network_source = source_network;

    m_parameter_mappings.push_back(std::move(mapping));
}

void WaveguideNetwork::unmap_parameter(const std::string& param_name)
{
    std::erase_if(m_parameter_mappings,
        [&](const auto& m) { return m.param_name == param_name; });
}

//-----------------------------------------------------------------------------
// Metadata
//-----------------------------------------------------------------------------

std::unordered_map<std::string, std::string>
WaveguideNetwork::get_metadata() const
{
    auto metadata = NodeNetwork::get_metadata();

    metadata["type"] = std::string(Reflect::enum_to_string(m_type));
    metadata["fundamental"] = std::to_string(m_fundamental) + " Hz";
    metadata["delay_length"] = std::to_string(m_delay_length_integer)
        + " + " + std::to_string(m_delay_length_fraction) + " samples";
    metadata["loss_factor"] = std::to_string(get_loss_factor());
    metadata["pickup_position"] = std::to_string(get_pickup_position());

    metadata["exciter_type"] = std::string(Reflect::enum_to_string(m_exciter_type));

    return metadata;
}

std::optional<double> WaveguideNetwork::get_node_output(size_t index) const
{
    if (index < m_segments.size() && !m_last_audio_buffer.empty()) {
        return m_last_output;
    }
    return std::nullopt;
}

std::optional<std::span<const double>> WaveguideNetwork::get_node_audio_buffer(size_t index) const
{
    if (index != 0 || m_last_audio_buffer.empty())
        return std::nullopt;

    return std::span<const double>(m_last_audio_buffer);
}

} // namespace MayaFlux::Nodes::Network
