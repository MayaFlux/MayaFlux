#include "Phasor.hpp"
#include "MayaFlux/MayaFlux.hpp"

namespace MayaFlux::Nodes::Generator {

Phasor::Phasor(float frequency, float amplitude, float offset, bool bAuto_register)
    : m_phase(0.0)
    , m_amplitude(amplitude)
    , m_frequency(frequency)
    , m_offset(offset)
    , m_frequency_modulator(nullptr)
    , m_amplitude_modulator(nullptr)
    , m_last_output(0.0)
    , m_phase_wrapped(false)
    , m_threshold_crossed((m_state = { Utils::NodeState::INACTIVE }, false))
{
    update_phase_increment(frequency);
}

Phasor::Phasor(std::shared_ptr<Node> frequency_modulator, float frequency, float amplitude, float offset, bool bAuto_register)
    : m_phase(0.0)
    , m_amplitude(amplitude)
    , m_frequency(frequency)
    , m_offset(offset)
    , m_frequency_modulator(frequency_modulator)
    , m_amplitude_modulator(nullptr)
    , m_last_output(0.0)
    , m_phase_wrapped(false)
    , m_threshold_crossed((m_state = { Utils::NodeState::INACTIVE }, false))
{
    update_phase_increment(frequency);
}

Phasor::Phasor(float frequency, std::shared_ptr<Node> amplitude_modulator, float amplitude, float offset, bool bAuto_register)
    : m_phase(0.0)
    , m_amplitude(amplitude)
    , m_frequency(frequency)
    , m_offset(offset)
    , m_frequency_modulator(nullptr)
    , m_amplitude_modulator(amplitude_modulator)
    , m_last_output(0.0)
    , m_phase_wrapped(false)
    , m_threshold_crossed((m_state = { Utils::NodeState::INACTIVE }, false))
{
    update_phase_increment(frequency);
}

Phasor::Phasor(std::shared_ptr<Node> frequency_modulator, std::shared_ptr<Node> amplitude_modulator,
    float frequency, float amplitude, float offset, bool bAuto_register)
    : m_phase(0.0)
    , m_amplitude(amplitude)
    , m_frequency(frequency)
    , m_offset(offset)
    , m_frequency_modulator(frequency_modulator)
    , m_amplitude_modulator(amplitude_modulator)
    , m_last_output(0.0)
    , m_phase_wrapped(false)
    , m_threshold_crossed((m_state = { Utils::NodeState::INACTIVE }, false))
{
    update_phase_increment(frequency);
}

void Phasor::set_frequency(float frequency)
{
    m_frequency = frequency;
    update_phase_increment(frequency);
}

void Phasor::update_phase_increment(float frequency)
{
    m_phase_inc = frequency / MayaFlux::get_sample_rate();
}

void Phasor::set_frequency_modulator(std::shared_ptr<Node> modulator)
{
    m_frequency_modulator = modulator;
}

void Phasor::set_amplitude_modulator(std::shared_ptr<Node> modulator)
{
    m_amplitude_modulator = modulator;
}

void Phasor::clear_modulators()
{
    m_frequency_modulator = nullptr;
    m_amplitude_modulator = nullptr;
}

double Phasor::process_sample(double input)
{
    double output = 0.0;

    double effective_freq = m_frequency;

    if (m_frequency_modulator) {
        u_int32_t state = m_frequency_modulator->m_state.load();
        if (state & Utils::NodeState::PROCESSED) {
            effective_freq += m_frequency_modulator->get_last_output();
        } else {
            effective_freq = m_frequency_modulator->process_sample(0.f);
            atomic_add_flag(m_frequency_modulator->m_state, Utils::NodeState::PROCESSED);
        }

        update_phase_increment(effective_freq);
    }

    output = m_phase * m_amplitude;

    if (m_amplitude_modulator) {
        u_int32_t state = m_amplitude_modulator->m_state.load();
        if (state & Utils::NodeState::PROCESSED) {
            output *= m_amplitude_modulator->get_last_output();
        } else {
            output *= m_amplitude_modulator->process_sample(0.f);
            atomic_add_flag(m_amplitude_modulator->m_state, Utils::NodeState::PROCESSED);
        }
    }

    output += m_offset;

    m_phase += m_phase_inc;
    if (m_phase >= 1.0) {
        m_phase -= 1.0;
        m_phase_wrapped = true;
    }

    m_last_output = output;

    notify_tick(output);

    return output;
}

std::vector<double> Phasor::process_batch(unsigned int num_samples)
{
    std::vector<double> output(num_samples, 0.0);

    for (unsigned int i = 0; i < num_samples; ++i) {
        output[i] = process_sample(0.0); // Use 0.0 as default input
    }

    return output;
}

void Phasor::reset(float frequency, float amplitude, float offset, double phase)
{
    m_frequency = frequency;
    m_amplitude = amplitude;
    m_offset = offset;
    m_phase = phase;

    while (m_phase >= 1.0)
        m_phase -= 1.0;
    while (m_phase < 0.0)
        m_phase += 1.0;

    update_phase_increment(frequency);
    m_last_output = 0.0;
}

void Phasor::on_tick(NodeHook callback)
{
    safe_add_callback(m_callbacks, callback);
}

void Phasor::on_tick_if(NodeHook callback, NodeCondition condition)
{
    safe_add_conditional_callback(m_conditional_callbacks, callback, condition);
}

void Phasor::on_phase_wrap(NodeHook callback)
{
    safe_add_callback(m_phase_wrap_callbacks, callback);
}

void Phasor::on_threshold(NodeHook callback, double threshold, bool rising)
{
    std::pair<NodeHook, double> entry = std::make_pair(callback, threshold);
    for (auto& pair : m_threshold_callbacks) {
        if (pair.first.target_type() == callback.target_type() && pair.second == threshold) {
            return;
        }
    }
    m_threshold_callbacks.push_back(entry);
}

bool Phasor::remove_threshold_callback(const NodeHook& callback)
{
    for (auto it = m_threshold_callbacks.begin(); it != m_threshold_callbacks.end(); ++it) {
        if (it->first.target_type() == callback.target_type()) {
            m_threshold_callbacks.erase(it);
            return true;
        }
    }
    return false;
}

bool Phasor::remove_hook(const NodeHook& callback)
{
    bool removed_from_tick = safe_remove_callback(m_callbacks, callback);
    bool removed_from_phase_wrap = safe_remove_callback(m_phase_wrap_callbacks, callback);
    bool removed_from_threshold = remove_threshold_callback(callback);
    return removed_from_tick || removed_from_phase_wrap || removed_from_threshold;
}

bool Phasor::remove_conditional_hook(const NodeCondition& callback)
{
    return safe_remove_conditional_callback(m_conditional_callbacks, callback);
}

void Phasor::reset_processed_state()
{
    atomic_remove_flag(m_state, Utils::NodeState::PROCESSED);

    if (m_frequency_modulator) {
        m_frequency_modulator->reset_processed_state();
    }

    if (m_amplitude_modulator) {
        m_amplitude_modulator->reset_processed_state();
    }
}

std::unique_ptr<NodeContext> Phasor::create_context(double value)
{
    return std::make_unique<GeneratorContext>(value, m_frequency, m_amplitude, m_phase);
}

void Phasor::notify_tick(double value)
{
    auto context = create_context(value);

    for (auto& callback : m_callbacks) {
        callback(*context);
    }

    if (m_phase_wrapped) {
        for (auto& callback : m_phase_wrap_callbacks) {
            callback(*context);
        }
    }

    for (auto& [callback, condition] : m_conditional_callbacks) {
        if (condition(*context)) {
            callback(*context);
        }
    }

    for (auto& [callback, threshold] : m_threshold_callbacks) {
        if (value >= threshold && !m_threshold_crossed) {
            callback(*context);
            m_threshold_crossed = true;
        } else if (value < threshold) {
            m_threshold_crossed = false;
        }
    }
}

} // namespace MayaFlux::Nodes::Generator
