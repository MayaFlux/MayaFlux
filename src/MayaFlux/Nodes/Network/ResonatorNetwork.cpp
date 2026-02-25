#include "ResonatorNetwork.hpp"

#include "MayaFlux/Nodes/Filters/IIR.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Nodes::Network {

//-----------------------------------------------------------------------------
// Preset tables
//-----------------------------------------------------------------------------

namespace {

    struct FormantEntry {
        double frequency;
        double q;
    };

    /*
     * Source: Peterson & Barney (1952) / Hillenbrand et al. (1995) averaged values.
     * Five formants; lower formants have broader absolute bandwidths modelled by
     * lower Q. Q approximated as F / BW with BW ≈ 50–80 Hz for F1, scaling upward.
     */
    const FormantEntry k_vowel_a[] = {
        { .frequency = 800.0, .q = 16.0 },
        { .frequency = 1200.0, .q = 30.0 },
        { .frequency = 2500.0, .q = 55.0 },
        { .frequency = 3500.0, .q = 70.0 },
        { .frequency = 4500.0, .q = 90.0 },
    };

    const FormantEntry k_vowel_e[] = {
        { .frequency = 400.0, .q = 10.0 },
        { .frequency = 2000.0, .q = 45.0 },
        { .frequency = 2600.0, .q = 55.0 },
        { .frequency = 3500.0, .q = 70.0 },
        { .frequency = 4500.0, .q = 90.0 },
    };

    const FormantEntry k_vowel_i[] = {
        { .frequency = 270.0, .q = 7.0 },
        { .frequency = 2300.0, .q = 50.0 },
        { .frequency = 3000.0, .q = 60.0 },
        { .frequency = 3500.0, .q = 70.0 },
        { .frequency = 4500.0, .q = 90.0 },
    };

    const FormantEntry k_vowel_o[] = {
        { .frequency = 500.0, .q = 12.0 },
        { .frequency = 900.0, .q = 22.0 },
        { .frequency = 2500.0, .q = 55.0 },
        { .frequency = 3500.0, .q = 70.0 },
        { .frequency = 4500.0, .q = 90.0 },
    };

    const FormantEntry k_vowel_u[] = {
        { .frequency = 300.0, .q = 8.0 },
        { .frequency = 800.0, .q = 20.0 },
        { .frequency = 2300.0, .q = 50.0 },
        { .frequency = 3500.0, .q = 70.0 },
        { .frequency = 4500.0, .q = 90.0 },
    };

    constexpr size_t k_preset_formant_count = 5;

} // namespace

//-----------------------------------------------------------------------------
// Static helper
//-----------------------------------------------------------------------------

void ResonatorNetwork::preset_to_vectors(FormantPreset preset,
    size_t n,
    std::vector<double>& out_freqs,
    std::vector<double>& out_qs)
{
    const FormantEntry* table = nullptr;

    switch (preset) {
    case FormantPreset::VOWEL_A:
        table = k_vowel_a;
        break;
    case FormantPreset::VOWEL_E:
        table = k_vowel_e;
        break;
    case FormantPreset::VOWEL_I:
        table = k_vowel_i;
        break;
    case FormantPreset::VOWEL_O:
        table = k_vowel_o;
        break;
    case FormantPreset::VOWEL_U:
        table = k_vowel_u;
        break;
    default:
        break;
    }

    out_freqs.resize(n, 440.0);
    out_qs.resize(n, 10.0);

    if (!table) {
        return;
    }

    const size_t defined = std::min(n, k_preset_formant_count);
    for (size_t i = 0; i < defined; ++i) {
        out_freqs[i] = table[i].frequency;
        out_qs[i] = table[i].q;
    }
}

//-----------------------------------------------------------------------------
// Construction
//-----------------------------------------------------------------------------

ResonatorNetwork::ResonatorNetwork(size_t num_resonators,
    FormantPreset preset)
{
    std::vector<double> freqs, qs;
    preset_to_vectors(preset, num_resonators, freqs, qs);
    build_resonators(freqs, qs);
}

ResonatorNetwork::ResonatorNetwork(const std::vector<double>& frequencies,
    const std::vector<double>& q_values)
{
    if (frequencies.size() != q_values.size()) {
        error<std::invalid_argument>(Journal::Component::Nodes, Journal::Context::NodeProcessing, std::source_location::current(),
            "ResonatorNetwork: frequencies and q_values vectors must have equal length");
    }
    build_resonators(frequencies, q_values);
}

//-----------------------------------------------------------------------------
// Internal construction helpers
//-----------------------------------------------------------------------------

void ResonatorNetwork::build_resonators(const std::vector<double>& frequencies,
    const std::vector<double>& qs)
{
    m_resonators.clear();
    m_resonators.reserve(frequencies.size());

    for (size_t i = 0; i < frequencies.size(); ++i) {
        ResonatorNode r;
        r.frequency = std::clamp(frequencies[i], 1.0, m_sample_rate * 0.5 - 1.0);
        r.q = std::clamp(qs[i], 0.1, 1000.0);
        r.gain = 1.0;
        r.last_output = 0.0;
        r.index = i;
        r.filter = std::make_shared<Filters::IIR>(
            std::vector<double> { 1.0, 0.0, 0.0 },
            std::vector<double> { 0.0, 0.0, 0.0 });
        compute_biquad(r);
        m_resonators.push_back(std::move(r));
    }
}

void ResonatorNetwork::compute_biquad(ResonatorNode& r)
{
    /*
     * RBJ Audio EQ Cookbook — BPF (constant 0 dB peak gain):
     *
     *   w0    = 2π f0 / Fs
     *   alpha = sin(w0) / (2 Q)
     *   b0    =  alpha
     *   b1    =  0
     *   b2    = -alpha
     *   a0    =  1 + alpha
     *   a1    = -2 cos(w0)
     *   a2    =  1 - alpha
     *
     * Normalised (divide through by a0):
     *   b_coefs = { b0/a0, 0, b2/a0 }
     *   a_coefs = { 1,  a1/a0, a2/a0 }
     */
    const double w0 = 2.0 * std::numbers::pi * r.frequency / m_sample_rate;
    const double sinw0 = std::sin(w0);
    const double cosw0 = std::cos(w0);
    const double alpha = sinw0 / (2.0 * r.q);
    const double a0 = 1.0 + alpha;

    const std::vector<double> a = {
        1.0,
        (-2.0 * cosw0) / a0,
        (1.0 - alpha) / a0,
    };
    const std::vector<double> b = {
        alpha / a0,
        0.0,
        -alpha / a0,
    };

    r.filter->setACoefficients(a);
    r.filter->setBCoefficients(b);
    r.filter->reset();
}

//-----------------------------------------------------------------------------
// NodeNetwork interface
//-----------------------------------------------------------------------------

void ResonatorNetwork::process_batch(unsigned int num_samples)
{
    if (m_resonators.empty()) {
        m_last_audio_buffer.assign(num_samples, 0.0);
        return;
    }

    update_mapped_parameters();

    m_last_audio_buffer.assign(num_samples, 0.0);
    const double norm = 1.0 / static_cast<double>(m_resonators.size());

    for (unsigned int s = 0; s < num_samples; ++s) {
        for (auto& r : m_resonators) {
            double excitation = 0.0;

            if (r.exciter) {
                excitation = r.exciter->process_sample(0.0);
            } else if (m_exciter) {
                excitation = m_exciter->process_sample(0.0);
            }

            const double out = r.filter->process_sample(excitation) * r.gain;
            r.last_output = out;
            m_last_audio_buffer[s] += out * norm;
        }
    }
}

std::optional<std::vector<double>> ResonatorNetwork::get_audio_buffer() const
{
    if (m_last_audio_buffer.empty()) {
        return std::nullopt;
    }
    return m_last_audio_buffer;
}

std::optional<double> ResonatorNetwork::get_node_output(size_t index) const
{
    if (index >= m_resonators.size()) {
        return std::nullopt;
    }
    return m_resonators[index].last_output;
}

//-----------------------------------------------------------------------------
// Parameter mappings
//-----------------------------------------------------------------------------

void ResonatorNetwork::map_parameter(const std::string& param_name,
    const std::shared_ptr<Node>& source,
    MappingMode mode)
{
    unmap_parameter(param_name);

    ParameterMapping m;
    m.param_name = param_name;
    m.mode = mode;
    m.broadcast_source = source;
    m_parameter_mappings.push_back(std::move(m));
}

void ResonatorNetwork::map_parameter(const std::string& param_name,
    const std::shared_ptr<NodeNetwork>& source_network)
{
    unmap_parameter(param_name);

    ParameterMapping m;
    m.param_name = param_name;
    m.mode = MappingMode::ONE_TO_ONE;
    m.network_source = source_network;
    m_parameter_mappings.push_back(std::move(m));
}

void ResonatorNetwork::unmap_parameter(const std::string& param_name)
{
    std::erase_if(m_parameter_mappings,
        [&](const auto& m) { return m.param_name == param_name; });
}

void ResonatorNetwork::update_mapped_parameters()
{
    for (const auto& mapping : m_parameter_mappings) {
        if (mapping.mode == MappingMode::BROADCAST && mapping.broadcast_source) {
            apply_broadcast_parameter(
                mapping.param_name,
                mapping.broadcast_source->get_last_output());
        } else if (mapping.mode == MappingMode::ONE_TO_ONE && mapping.network_source) {
            apply_one_to_one_parameter(mapping.param_name, mapping.network_source);
        }
    }
}

void ResonatorNetwork::apply_broadcast_parameter(const std::string& param, double value)
{
    if (param == "frequency") {
        set_all_frequencies(value);
    } else if (param == "q") {
        set_all_q(value);
    } else if (param == "gain") {
        for (auto& r : m_resonators) {
            r.gain = value;
        }
    }
}

void ResonatorNetwork::apply_one_to_one_parameter(const std::string& param,
    const std::shared_ptr<NodeNetwork>& source)
{
    const size_t count = std::min(m_resonators.size(), source->get_node_count());

    for (size_t i = 0; i < count; ++i) {
        const auto val = source->get_node_output(i);
        if (!val.has_value()) {
            continue;
        }
        if (param == "frequency") {
            set_frequency(i, *val);
        } else if (param == "q") {
            set_q(i, *val);
        } else if (param == "gain") {
            m_resonators[i].gain = *val;
        }
    }
}

//-----------------------------------------------------------------------------
// Excitation control
//-----------------------------------------------------------------------------

void ResonatorNetwork::set_exciter(const std::shared_ptr<Node>& exciter)
{
    m_exciter = exciter;
}

void ResonatorNetwork::clear_exciter()
{
    m_exciter = nullptr;
}

void ResonatorNetwork::set_resonator_exciter(size_t index, const std::shared_ptr<Node>& exciter)
{
    if (index >= m_resonators.size()) {
        error<std::out_of_range>(Journal::Component::Nodes, Journal::Context::NodeProcessing, std::source_location::current(),
            "ResonatorNetwork::set_resonator_exciter: index out of range (index={}, resonator_count={})", index, m_resonators.size());
    }
    m_resonators[index].exciter = exciter;
}

void ResonatorNetwork::clear_resonator_exciter(size_t index)
{
    if (index >= m_resonators.size()) {
        error<std::out_of_range>(Journal::Component::Nodes, Journal::Context::NodeProcessing, std::source_location::current(),
            "ResonatorNetwork::clear_resonator_exciter: index out of range (index={}, resonator_count={})", index, m_resonators.size());
    }
    m_resonators[index].exciter = nullptr;
}

//-----------------------------------------------------------------------------
// Per-resonator parameter control
//-----------------------------------------------------------------------------

void ResonatorNetwork::set_frequency(size_t index, double frequency)
{
    if (index >= m_resonators.size()) {
        error<std::out_of_range>(Journal::Component::Nodes, Journal::Context::NodeProcessing, std::source_location::current(),
            "ResonatorNetwork::set_frequency: index out of range (index={}, resonator_count={})", index, m_resonators.size());
    }
    auto& r = m_resonators[index];
    r.frequency = std::clamp(frequency, 1.0, m_sample_rate * 0.5 - 1.0);
    compute_biquad(r);
}

void ResonatorNetwork::set_q(size_t index, double q)
{
    if (index >= m_resonators.size()) {
        error<std::out_of_range>(Journal::Component::Nodes, Journal::Context::NodeProcessing, std::source_location::current(),
            "ResonatorNetwork::set_q: index out of range (index={}, resonator_count={})", index, m_resonators.size());
    }
    auto& r = m_resonators[index];
    r.q = std::clamp(q, 0.1, 1000.0);
    compute_biquad(r);
}

void ResonatorNetwork::set_resonator_gain(size_t index, double gain)
{
    if (index >= m_resonators.size()) {
        error<std::out_of_range>(Journal::Component::Nodes, Journal::Context::NodeProcessing, std::source_location::current(),
            "ResonatorNetwork::set_resonator_gain: index out of range (index={}, resonator_count={})", index, m_resonators.size());
    }
    m_resonators[index].gain = gain;
}

//-----------------------------------------------------------------------------
// Network-wide control
//-----------------------------------------------------------------------------

void ResonatorNetwork::set_all_frequencies(double frequency)
{
    for (size_t i = 0; i < m_resonators.size(); ++i) {
        set_frequency(i, frequency);
    }
}

void ResonatorNetwork::set_all_q(double q)
{
    for (size_t i = 0; i < m_resonators.size(); ++i) {
        set_q(i, q);
    }
}

void ResonatorNetwork::apply_preset(FormantPreset preset)
{
    std::vector<double> freqs, qs;
    preset_to_vectors(preset, m_resonators.size(), freqs, qs);

    for (size_t i = 0; i < m_resonators.size(); ++i) {
        m_resonators[i].frequency = freqs[i];
        m_resonators[i].q = qs[i];
        compute_biquad(m_resonators[i]);
    }
}

//-----------------------------------------------------------------------------
// Metadata
//-----------------------------------------------------------------------------

std::unordered_map<std::string, std::string> ResonatorNetwork::get_metadata() const
{
    auto meta = NodeNetwork::get_metadata();

    meta["num_resonators"] = std::to_string(m_resonators.size());
    meta["sample_rate"] = std::to_string(m_sample_rate) + " Hz";

    for (const auto& r : m_resonators) {
        const std::string prefix = "resonator_" + std::to_string(r.index) + "_";
        meta[prefix + "freq"] = std::to_string(r.frequency) + " Hz";
        meta[prefix + "q"] = std::to_string(r.q);
        meta[prefix + "gain"] = std::to_string(r.gain);
    }

    return meta;
}

} // namespace MayaFlux::Nodes::Network
