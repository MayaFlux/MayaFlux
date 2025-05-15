#include "Impulse.hpp"
#include "MayaFlux/MayaFlux.hpp"

namespace MayaFlux::Nodes::Generator {

Impulse::Impulse(float frequency, float amplitude, float offset, bool bAuto_register)
    : m_phase(0.0)
    , m_amplitude(amplitude)
    , m_frequency(frequency)
    , m_offset(offset)
    , m_frequency_modulator(nullptr)
    , m_amplitude_modulator(nullptr)
    , m_is_registered(false)
    , m_is_processed(false)
    , m_last_output(0.0)
    , m_impulse_occurred(false)
{
    update_phase_increment(frequency);
}

Impulse::Impulse(std::shared_ptr<Node> frequency_modulator, float frequency, float amplitude, float offset, bool bAuto_register)
    : m_phase(0.0)
    , m_amplitude(amplitude)
    , m_frequency(frequency)
    , m_offset(offset)
    , m_frequency_modulator(frequency_modulator)
    , m_amplitude_modulator(nullptr)
    , m_is_registered(false)
    , m_is_processed(false)
    , m_last_output(0.0)
    , m_impulse_occurred(false)
{
    update_phase_increment(frequency);
}

Impulse::Impulse(float frequency, std::shared_ptr<Node> amplitude_modulator, float amplitude, float offset, bool bAuto_register)
    : m_phase(0.0)
    , m_amplitude(amplitude)
    , m_frequency(frequency)
    , m_offset(offset)
    , m_frequency_modulator(nullptr)
    , m_amplitude_modulator(amplitude_modulator)
    , m_is_registered(false)
    , m_is_processed(false)
    , m_last_output(0.0)
    , m_impulse_occurred(false)
{
    update_phase_increment(frequency);
}

Impulse::Impulse(std::shared_ptr<Node> frequency_modulator, std::shared_ptr<Node> amplitude_modulator,
    float frequency, float amplitude, float offset, bool bAuto_register)
    : m_phase(0.0)
    , m_amplitude(amplitude)
    , m_frequency(frequency)
    , m_offset(offset)
    , m_frequency_modulator(frequency_modulator)
    , m_amplitude_modulator(amplitude_modulator)
    , m_is_registered(false)
    , m_is_processed(false)
    , m_last_output(0.0)
    , m_impulse_occurred(false)
{
    update_phase_increment(frequency);
}

void Impulse::set_frequency(float frequency)
{
    m_frequency = frequency;
    update_phase_increment(frequency);
}

void Impulse::update_phase_increment(float frequency)
{
    m_phase_inc = frequency / MayaFlux::get_sample_rate();
}

void Impulse::set_frequency_modulator(std::shared_ptr<Node> modulator)
{
    m_frequency_modulator = modulator;
}

void Impulse::set_amplitude_modulator(std::shared_ptr<Node> modulator)
{
    m_amplitude_modulator = modulator;
}

void Impulse::clear_modulators()
{
    m_frequency_modulator = nullptr;
    m_amplitude_modulator = nullptr;
}

double Impulse::process_sample(double input)
{
    double output = 0.0;
    m_impulse_occurred = false;

    double effective_freq = m_frequency;
    if (m_frequency_modulator) {
        effective_freq += m_frequency_modulator->process_sample(input);
        if (effective_freq <= 0) {
            effective_freq = 0.001;
        }
        update_phase_increment(effective_freq);
    }

    if (m_phase < m_phase_inc) {
        output = m_amplitude;
        m_impulse_occurred = true;
    } else {
        output = 0.0;
    }

    if (m_amplitude_modulator) {
        output *= m_amplitude_modulator->process_sample(input);
    }

    output += m_offset;

    m_phase += m_phase_inc;
    if (m_phase >= 1.0) {
        m_phase -= 1.0;
    }

    m_last_output = output;

    notify_tick(output);

    return output;
}

std::vector<double> Impulse::processFull(unsigned int num_samples)
{
    std::vector<double> output(num_samples, 0.0);

    for (unsigned int i = 0; i < num_samples; ++i) {
        output[i] = process_sample(0.0); // Use 0.0 as default input
    }

    return output;
}

void Impulse::reset(float frequency, float amplitude, float offset)
{
    m_phase = 0.0;
    m_amplitude = amplitude;
    m_offset = offset;
    set_frequency(frequency);
    m_last_output = 0.0;
}

void Impulse::on_tick(NodeHook callback)
{
    safe_add_callback(m_callbacks, callback);
}

void Impulse::on_impulse(NodeHook callback)
{
    safe_add_callback(m_impulse_callbacks, callback);
}

void Impulse::on_tick_if(NodeHook callback, NodeCondition condition)
{
    safe_add_conditional_callback(m_conditional_callbacks, callback, condition);
}

bool Impulse::remove_hook(const NodeHook& callback)
{
    bool removed_from_tick = safe_remove_callback(m_callbacks, callback);
    bool removed_from_impulse = safe_remove_callback(m_impulse_callbacks, callback);
    return removed_from_tick || removed_from_impulse;
}

bool Impulse::remove_conditional_hook(const NodeCondition& condition)
{
    return safe_remove_conditional_callback(m_conditional_callbacks, condition);
}

void Impulse::reset_processed_state()
{
    m_is_processed = false;

    if (m_frequency_modulator) {
        m_frequency_modulator->reset_processed_state();
    }

    if (m_amplitude_modulator) {
        m_amplitude_modulator->reset_processed_state();
    }
}

std::unique_ptr<NodeContext> Impulse::create_context(double value)
{
    return std::make_unique<GeneratorContext>(value, m_frequency, m_amplitude, m_phase);
}

void Impulse::notify_tick(double value)
{
    auto context = create_context(value);

    for (auto& callback : m_callbacks) {
        callback(*context);
    }
    for (auto& [callback, condition] : m_conditional_callbacks) {
        if (condition(*context)) {
            callback(*context);
        }
    }
    if (m_impulse_occurred) {
        for (auto& callback : m_impulse_callbacks) {
            callback(*context);
        }
    }
}

} // namespace MayaFlux::Nodes::Generator
