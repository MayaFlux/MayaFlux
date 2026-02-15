#include "Sine.hpp"

namespace MayaFlux::Nodes::Generator {

Sine::Sine(float frequency, double amplitude, float offset)
    : m_offset(offset)
    , m_frequency_modulator(nullptr)
    , m_amplitude_modulator(nullptr)
{
    m_amplitude = amplitude;
    m_frequency = frequency;
    update_phase_increment(frequency);
}

Sine::Sine(const std::shared_ptr<Node>& frequency_modulator, float frequency, double amplitude, float offset)
    : m_offset(offset)
    , m_frequency_modulator(frequency_modulator)
    , m_amplitude_modulator(nullptr)
{
    m_amplitude = amplitude;
    m_frequency = frequency;
    update_phase_increment(frequency);
}

Sine::Sine(float frequency, const std::shared_ptr<Node>& amplitude_modulator, double amplitude, float offset)
    : m_offset(offset)
    , m_frequency_modulator(nullptr)
    , m_amplitude_modulator(amplitude_modulator)
{
    m_amplitude = amplitude;
    m_frequency = frequency;
    update_phase_increment(frequency);
}

Sine::Sine(const std::shared_ptr<Node>& frequency_modulator, const std::shared_ptr<Node>& amplitude_modulator, float frequency, double amplitude, float offset)
    : m_offset(offset)
    , m_frequency_modulator(frequency_modulator)
    , m_amplitude_modulator(amplitude_modulator)
{
    m_amplitude = amplitude;
    m_frequency = frequency;
    update_phase_increment(frequency);
}

void Sine::set_frequency(float frequency)
{
    m_frequency = frequency;
    update_phase_increment(frequency);
}

void Sine::update_phase_increment(double frequency)
{
    m_phase_inc = (2 * M_PI * frequency) / (double)m_sample_rate;
}

void Sine::set_frequency_modulator(const std::shared_ptr<Node>& modulator)
{
    m_frequency_modulator = modulator;
}

void Sine::set_amplitude_modulator(const std::shared_ptr<Node>& modulator)
{
    m_amplitude_modulator = modulator;
}

void Sine::clear_modulators()
{
    m_frequency_modulator = nullptr;
    m_amplitude_modulator = nullptr;
    reset(m_frequency, m_amplitude, m_offset);
}

double Sine::process_sample(double input)
{
    if (m_frequency_modulator) {
        atomic_inc_modulator_count(m_frequency_modulator->m_modulator_count, 1);
        uint32_t state = m_frequency_modulator->m_state.load();
        double current_freq = m_frequency;

        if (state & NodeState::PROCESSED) {
            current_freq += m_frequency_modulator->get_last_output();
        } else {
            current_freq += m_frequency_modulator->process_sample(0.F);
            atomic_add_flag(m_frequency_modulator->m_state, NodeState::PROCESSED);
        }
        update_phase_increment(current_freq);
    }

    double current_sample = std::sin(m_phase + m_offset);
    m_phase += m_phase_inc;

    if (m_phase > 2 * M_PI) {
        m_phase -= 2 * M_PI;
    } else if (m_phase < -2 * M_PI) {
        m_phase += 2 * M_PI;
    }

    double current_amplitude = m_amplitude;
    if (m_amplitude_modulator) {
        atomic_inc_modulator_count(m_amplitude_modulator->m_modulator_count, 1);
        uint32_t state = m_amplitude_modulator->m_state.load();

        if (state & NodeState::PROCESSED) {
            current_amplitude += m_amplitude_modulator->get_last_output();
            if (!(state & NodeState::ACTIVE)) {
                atomic_remove_flag(m_amplitude_modulator->m_state, NodeState::PROCESSED);
            }
        } else {
            current_amplitude += m_amplitude_modulator->process_sample(0.F);
            atomic_add_flag(m_amplitude_modulator->m_state, NodeState::PROCESSED);
        }
    }
    current_sample *= current_amplitude;

    if (input != 0.0) {
        current_sample += input;
        current_sample *= 0.5F;
    }

    m_last_output = current_sample;

    if ((!m_state_saved || (m_state_saved && m_fire_events_during_snapshot))
        && !m_networked_node) {
        notify_tick(current_sample);
    }

    if (m_frequency_modulator) {
        atomic_dec_modulator_count(m_frequency_modulator->m_modulator_count, 1);
        try_reset_processed_state(m_frequency_modulator);
    }
    if (m_amplitude_modulator) {
        atomic_dec_modulator_count(m_amplitude_modulator->m_modulator_count, 1);
        try_reset_processed_state(m_amplitude_modulator);
    }
    return current_sample;
}

std::vector<double> Sine::process_batch(unsigned int num_samples)
{
    std::vector<double> output(num_samples);
    for (unsigned int i = 0; i < num_samples; i++) {
        output[i] = process_sample(0.0);
    }
    return output;
}

void Sine::reset(float frequency, double amplitude, float offset)
{
    m_phase = 0;
    m_frequency = frequency;
    m_amplitude = amplitude;
    m_offset = offset;
    update_phase_increment(frequency);
}

void Sine::notify_tick(double value)
{
    update_context(value);
    auto& ctx = get_last_context();

    for (auto& callback : m_callbacks) {
        callback(ctx);
    }
    for (auto& [callback, condition] : m_conditional_callbacks) {
        if (condition(ctx)) {
            callback(ctx);
        }
    }
}

void Sine::save_state()
{
    m_saved_phase = m_phase;
    m_saved_frequency = m_frequency;
    m_saved_offset = m_offset;
    m_saved_phase_inc = m_phase_inc;
    m_saved_last_output = m_last_output;

    if (m_frequency_modulator)
        m_frequency_modulator->save_state();
    if (m_amplitude_modulator)
        m_amplitude_modulator->save_state();

    m_state_saved = true;
}

void Sine::restore_state()
{
    m_phase = m_saved_phase;
    m_frequency = m_saved_frequency;
    m_offset = m_saved_offset;
    m_phase_inc = m_saved_phase_inc;
    m_last_output = m_saved_last_output;

    if (m_frequency_modulator)
        m_frequency_modulator->restore_state();
    if (m_amplitude_modulator)
        m_amplitude_modulator->restore_state();

    m_state_saved = false;
}

void Sine::printGraph()
{
}

void Sine::printCurrent()
{
}

}
