#include "ModalNetwork.hpp"

#include "MayaFlux/Nodes/Filters/Filter.hpp"
#include "MayaFlux/Nodes/Generators/Sine.hpp"

namespace MayaFlux::Nodes::Network {

//-----------------------------------------------------------------------------
// Construction
//-----------------------------------------------------------------------------

ModalNetwork::ModalNetwork(size_t num_modes, double fundamental,
    Spectrum spectrum, double base_decay)
    : m_spectrum(spectrum)
    , m_fundamental(fundamental)
{
    set_output_mode(OutputMode::AUDIO_SINK);
    set_topology(Topology::INDEPENDENT);

    auto ratios = generate_spectrum_ratios(spectrum, num_modes);
    initialize_modes(ratios, base_decay);
}

ModalNetwork::ModalNetwork(const std::vector<double>& frequency_ratios,
    double fundamental, double base_decay)
    : m_spectrum(Spectrum::CUSTOM)
    , m_fundamental(fundamental)
{
    set_output_mode(OutputMode::AUDIO_SINK);
    set_topology(Topology::INDEPENDENT);

    initialize_modes(frequency_ratios, base_decay);
}

//-----------------------------------------------------------------------------
// Spectrum Generation
//-----------------------------------------------------------------------------

std::vector<double> ModalNetwork::generate_spectrum_ratios(Spectrum spectrum,
    size_t count)
{
    std::vector<double> ratios;
    ratios.reserve(count);

    switch (spectrum) {
    case Spectrum::HARMONIC:
        // Perfect integer harmonics: 1, 2, 3, 4...
        for (size_t i = 0; i < count; ++i) {
            ratios.push_back(static_cast<double>(i + 1));
        }
        break;

    case Spectrum::INHARMONIC:
        // Bell-like spectrum (approximate mode ratios for circular plates)
        // Based on Bessel function zeros
        ratios = { 1.0, 2.756, 5.404, 8.933, 13.344, 18.64, 24.81, 31.86 };
        while (ratios.size() < count) {
            double last = ratios.back();
            ratios.push_back(last + 6.8);
        }
        ratios.resize(count);
        break;

    case Spectrum::STRETCHED:
        // f_n = n * f_0 * sqrt(1 + B * n^2)
        // Using small B = 0.0001 for moderate stretching
        {
            constexpr double B = 0.0001;
            for (size_t n = 1; n <= count; ++n) {
                double ratio = n * std::sqrt(1.0 + B * n * n);
                ratios.push_back(ratio);
            }
        }
        break;

    case Spectrum::CUSTOM:
        for (size_t i = 0; i < count; ++i) {
            ratios.push_back(static_cast<double>(i + 1));
        }
        break;
    }

    return ratios;
}

//-----------------------------------------------------------------------------
// Mode Initialization
//-----------------------------------------------------------------------------

void ModalNetwork::initialize_modes(const std::vector<double>& ratios,
    double base_decay)
{
    m_modes.clear();
    m_modes.reserve(ratios.size());

    for (size_t i = 0; i < ratios.size(); ++i) {
        ModalNode mode;
        mode.index = i;
        mode.frequency_ratio = ratios[i];
        mode.base_frequency = m_fundamental * ratios[i];
        mode.current_frequency = mode.base_frequency;

        mode.decay_time = base_decay / ratios[i];

        mode.initial_amplitude = 1.0 / double(i + 1);
        mode.amplitude = 0.0;

        mode.oscillator = std::make_shared<Generator::Sine>(static_cast<float>(mode.current_frequency));
        mode.oscillator->set_in_network(true);

        mode.decay_coefficient = std::exp(-1.0 / (base_decay * m_sample_rate));

        m_modes.push_back(std::move(mode));
    }
}

void ModalNetwork::reset()
{
    for (auto& mode : m_modes) {
        mode.amplitude = 0.0;
        mode.phase = 0.0;
        mode.current_frequency = mode.base_frequency;
    }
    m_last_output = 0.0;
}

//-----------------------------------------------------------------------------
// Exciter System
//-----------------------------------------------------------------------------

void ModalNetwork::set_exciter_duration(double seconds)
{
    m_exciter_duration = std::max(0.001, seconds);
}

void ModalNetwork::set_exciter_sample(const std::vector<double>& sample)
{
    m_exciter_sample = sample;
}

void ModalNetwork::initialize_exciter(double /*strength*/)
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
            m_exciter_duration * m_sample_rate);
        break;

    case ExciterType::SAMPLE:
        m_exciter_samples_remaining = m_exciter_sample.size();
        break;

    case ExciterType::CONTINUOUS:
        m_exciter_samples_remaining = std::numeric_limits<size_t>::max();
        break;
    }
}

double ModalNetwork::generate_exciter_sample()
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
        if (m_exciter_filter) {
            sample = m_exciter_filter->process_sample(noise);
        } else {
            sample = noise;
        }
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
// Spatial Excitation
//-----------------------------------------------------------------------------

void ModalNetwork::excite_at_position(double position, double strength)
{
    position = std::clamp(position, 0.0, 1.0);

    if (m_spatial_distribution.empty()) {
        compute_spatial_distribution();
    }

    initialize_exciter(strength);

    for (size_t i = 0; i < m_modes.size(); ++i) {
        double spatial_amp = std::sin((i + 1) * M_PI * position);
        spatial_amp = std::abs(spatial_amp);

        m_modes[i].amplitude = m_modes[i].initial_amplitude * strength * spatial_amp;
    }
}

void ModalNetwork::set_spatial_distribution(const std::vector<double>& distribution)
{
    if (distribution.size() != m_modes.size()) {
        return;
    }
    m_spatial_distribution = distribution;
}

void ModalNetwork::compute_spatial_distribution()
{
    m_spatial_distribution.clear();
    m_spatial_distribution.reserve(m_modes.size());

    for (size_t i = 0; i < m_modes.size(); ++i) {
        m_spatial_distribution.push_back(1.0);
    }
}

//-----------------------------------------------------------------------------
// Modal Coupling
//-----------------------------------------------------------------------------

void ModalNetwork::set_mode_coupling(size_t mode_a, size_t mode_b, double strength)
{
    if (mode_a >= m_modes.size() || mode_b >= m_modes.size() || mode_a == mode_b) {
        return;
    }

    strength = std::clamp(strength, 0.0, 1.0);

    remove_mode_coupling(mode_a, mode_b);

    m_couplings.push_back({ mode_a, mode_b, strength });
}

void ModalNetwork::remove_mode_coupling(size_t mode_a, size_t mode_b)
{
    std::erase_if(m_couplings,
        [mode_a, mode_b](const ModeCoupling& c) {
            return (c.mode_a == mode_a && c.mode_b == mode_b)
                || (c.mode_a == mode_b && c.mode_b == mode_a);
        });
}

void ModalNetwork::compute_mode_coupling()
{
    for (const auto& coupling : m_couplings) {
        auto& mode_a = m_modes[coupling.mode_a];
        auto& mode_b = m_modes[coupling.mode_b];

        double energy_diff = (mode_a.amplitude - mode_b.amplitude) * coupling.strength;

        mode_a.amplitude -= energy_diff * 0.5;
        mode_b.amplitude += energy_diff * 0.5;
    }
}

//-----------------------------------------------------------------------------
// Processing
//-----------------------------------------------------------------------------

void ModalNetwork::process_batch(unsigned int num_samples)
{
    ensure_initialized();

    m_last_audio_buffer.clear();

    if (!is_enabled()) {
        m_last_audio_buffer.assign(num_samples, 0.0);
        m_last_output = 0.0;
        return;
    }

    update_mapped_parameters();
    m_last_audio_buffer.reserve(num_samples);

    m_node_buffers.assign(m_modes.size(), {});
    for (auto& nb : m_node_buffers)
        nb.reserve(num_samples);

    for (size_t i = 0; i < num_samples; ++i) {
        double exciter_signal = generate_exciter_sample();

        if (m_coupling_enabled && !m_couplings.empty())
            compute_mode_coupling();

        double sum = 0.0;
        for (size_t m = 0; m < m_modes.size(); ++m) {
            auto& mode = m_modes[m];

            if (mode.amplitude > 0.0001) {
                mode.amplitude *= mode.decay_coefficient;
            } else {
                mode.amplitude = 0.0;
            }

            double sample = mode.oscillator->process_sample(0.0) * mode.amplitude;
            m_node_buffers[m].push_back(sample);
            sum += sample;
        }
        m_last_audio_buffer.push_back(sum);
    }
    m_last_output = m_last_audio_buffer.back();
}

//-----------------------------------------------------------------------------
// Parameter Mapping
//-----------------------------------------------------------------------------

void ModalNetwork::update_mapped_parameters()
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

void ModalNetwork::apply_broadcast_parameter(const std::string& param,
    double value)
{
    if (param == "frequency") {
        set_fundamental(value);
    } else if (param == "decay") {
        m_decay_multiplier = std::max(0.01, value);
    } else if (param == "amplitude") {
        for (auto& mode : m_modes) {
            mode.amplitude *= value;
        }
    }
}

void ModalNetwork::apply_one_to_one_parameter(
    const std::string& param, const std::shared_ptr<NodeNetwork>& source)
{
    if (source->get_node_count() != m_modes.size()) {
        return;
    }

    if (param == "amplitude") {
        for (size_t i = 0; i < m_modes.size(); ++i) {
            auto val = source->get_node_output(i);
            if (val) {
                m_modes[i].amplitude *= *val;
            }
        }
    } else if (param == "detune") {
        for (size_t i = 0; i < m_modes.size(); ++i) {
            auto val = source->get_node_output(i);
            if (val) {
                double detune_cents = *val * 100.0; // ±100 cents
                double ratio = std::pow(2.0, detune_cents / 1200.0);
                m_modes[i].current_frequency = m_modes[i].base_frequency * ratio;
                m_modes[i].oscillator->set_frequency(m_modes[i].current_frequency);
            }
        }
    }
}

void ModalNetwork::map_parameter(const std::string& param_name,
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

void ModalNetwork::map_parameter(
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

void ModalNetwork::unmap_parameter(const std::string& param_name)
{
    std::erase_if(m_parameter_mappings,
        [&](const auto& m) { return m.param_name == param_name; });
}

//-----------------------------------------------------------------------------
// Modal Control
//-----------------------------------------------------------------------------

void ModalNetwork::excite(double strength)
{
    initialize_exciter(strength);

    for (auto& mode : m_modes) {
        mode.amplitude = mode.initial_amplitude * strength;
    }
}

void ModalNetwork::excite_mode(size_t mode_index, double strength)
{
    if (mode_index < m_modes.size()) {
        m_modes[mode_index].amplitude = m_modes[mode_index].initial_amplitude * strength;
    }
}

void ModalNetwork::damp(double damping_factor)
{
    for (auto& mode : m_modes) {
        mode.amplitude *= damping_factor;
    }
}

void ModalNetwork::set_fundamental(double frequency)
{
    m_fundamental = frequency;

    for (auto& mode : m_modes) {
        mode.base_frequency = m_fundamental * mode.frequency_ratio;
        mode.current_frequency = mode.base_frequency;

        mode.oscillator->set_frequency(mode.current_frequency);
    }
}

//-----------------------------------------------------------------------------
// Metadata
//-----------------------------------------------------------------------------

std::unordered_map<std::string, std::string>
ModalNetwork::get_metadata() const
{
    auto metadata = NodeNetwork::get_metadata();

    metadata["fundamental"] = std::to_string(m_fundamental) + " Hz";
    metadata["spectrum"] = [this]() {
        switch (m_spectrum) {
        case Spectrum::HARMONIC:
            return "HARMONIC";
        case Spectrum::INHARMONIC:
            return "INHARMONIC";
        case Spectrum::STRETCHED:
            return "STRETCHED";
        case Spectrum::CUSTOM:
            return "CUSTOM";
        default:
            return "UNKNOWN";
        }
    }();
    metadata["decay_multiplier"] = std::to_string(m_decay_multiplier);

    double avg_amplitude = 0.0;
    for (const auto& mode : m_modes) {
        avg_amplitude += mode.amplitude;
    }
    avg_amplitude /= (double)m_modes.size();
    metadata["avg_amplitude"] = std::to_string(avg_amplitude);

    metadata["exciter_type"] = [this]() {
        switch (m_exciter_type) {
        case ExciterType::IMPULSE:
            return "IMPULSE";
        case ExciterType::NOISE_BURST:
            return "NOISE_BURST";
        case ExciterType::FILTERED_NOISE:
            return "FILTERED_NOISE";
        case ExciterType::SAMPLE:
            return "SAMPLE";
        case ExciterType::CONTINUOUS:
            return "CONTINUOUS";
        default:
            return "UNKNOWN";
        }
    }();

    metadata["coupling_enabled"] = m_coupling_enabled ? "true" : "false";
    metadata["coupling_count"] = std::to_string(m_couplings.size());

    return metadata;
}

[[nodiscard]] std::optional<double> ModalNetwork::get_node_output(size_t index) const
{
    if (m_modes.size() > index) {
        return m_modes[index].oscillator->get_last_output();
    }
    return std::nullopt;
}

std::optional<std::span<const double>> ModalNetwork::get_node_audio_buffer(size_t index) const
{
    if (index >= m_node_buffers.size() || m_node_buffers[index].empty())
        return std::nullopt;
    return std::span<const double>(m_node_buffers[index]);
}

} // namespace MayaFlux::Nodes::Network
