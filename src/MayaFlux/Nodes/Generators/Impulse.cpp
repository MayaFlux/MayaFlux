#include "Impulse.hpp"
#include "MayaFlux/API/Config.hpp"

namespace MayaFlux::Nodes::Generator {

Impulse::Impulse(float frequency, double amplitude, float offset)
    : m_offset(offset)
    , m_frequency_modulator(nullptr)
    , m_amplitude_modulator(nullptr)
    , m_impulse_occurred(false)
{
    m_amplitude = amplitude;
    update_phase_increment(frequency);
}

Impulse::Impulse(const std::shared_ptr<Node>& frequency_modulator, float frequency, double amplitude, float offset)
    : m_offset(offset)
    , m_frequency_modulator(frequency_modulator)
    , m_amplitude_modulator(nullptr)
    , m_impulse_occurred(false)
{
    m_amplitude = amplitude;
    m_frequency = frequency;
    update_phase_increment(frequency);
}

Impulse::Impulse(float frequency, const std::shared_ptr<Node>& amplitude_modulator, double amplitude, float offset)
    : m_offset(offset)
    , m_frequency_modulator(nullptr)
    , m_amplitude_modulator(amplitude_modulator)
    , m_impulse_occurred(false)
{
    m_amplitude = amplitude;
    m_frequency = frequency;
    update_phase_increment(frequency);
}

Impulse::Impulse(const std::shared_ptr<Node>& frequency_modulator, const std::shared_ptr<Node>& amplitude_modulator,
    float frequency, double amplitude, float offset)
    : m_offset(offset)
    , m_frequency_modulator(frequency_modulator)
    , m_amplitude_modulator(amplitude_modulator)
    , m_impulse_occurred(false)
{
    m_amplitude = amplitude;
    m_frequency = frequency;
    update_phase_increment(frequency);
}

void Impulse::set_frequency(float frequency)
{
    m_frequency = frequency;
    update_phase_increment(frequency);
}

void Impulse::update_phase_increment(double frequency)
{
    uint64_t s_rate = 48000U;
    if (MayaFlux::is_engine_initialized()) {
        s_rate = Config::get_sample_rate();
    }
    m_phase_inc = frequency / (double)s_rate;
}

void Impulse::set_frequency_modulator(const std::shared_ptr<Node>& modulator)
{
    m_frequency_modulator = modulator;
}

void Impulse::set_amplitude_modulator(const std::shared_ptr<Node>& modulator)
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
        atomic_inc_modulator_count(m_frequency_modulator->m_modulator_count, 1);
        uint32_t state = m_frequency_modulator->m_state.load();
        if (state & Utils::NodeState::PROCESSED) {
            effective_freq += m_frequency_modulator->get_last_output();
        } else {
            effective_freq = m_frequency_modulator->process_sample(0.F);
            atomic_add_flag(m_frequency_modulator->m_state, Utils::NodeState::PROCESSED);
        }

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

    double current_amplitude = m_amplitude;
    if (m_amplitude_modulator) {
        atomic_inc_modulator_count(m_amplitude_modulator->m_modulator_count, 1);
        uint32_t state = m_amplitude_modulator->m_state.load();

        if (state & Utils::NodeState::PROCESSED) {
            current_amplitude += m_amplitude_modulator->get_last_output();
        } else {
            current_amplitude += m_amplitude_modulator->process_sample(0.F);
            atomic_add_flag(m_amplitude_modulator->m_state, Utils::NodeState::PROCESSED);
        }
    }

    output *= current_amplitude;
    m_amplitude = current_amplitude;
    output += m_offset;

    m_phase += m_phase_inc;
    if (m_phase >= 1.0) {
        m_phase -= 1.0;
    }

    m_last_output = output;

    if (!m_state_saved || (m_state_saved && m_fire_events_during_snapshot))
        notify_tick(output);

    if (m_frequency_modulator) {
        atomic_dec_modulator_count(m_frequency_modulator->m_modulator_count, 1);
        try_reset_processed_state(m_frequency_modulator);
    }
    if (m_amplitude_modulator) {
        atomic_dec_modulator_count(m_amplitude_modulator->m_modulator_count, 1);
        try_reset_processed_state(m_amplitude_modulator);
    }

    return output;
}

std::vector<double> Impulse::process_batch(unsigned int num_samples)
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

void Impulse::on_impulse(const NodeHook& callback)
{
    safe_add_callback(m_impulse_callbacks, callback);
}

bool Impulse::remove_hook(const NodeHook& callback)
{
    bool removed_from_tick = safe_remove_callback(m_callbacks, callback);
    bool removed_from_impulse = safe_remove_callback(m_impulse_callbacks, callback);
    return removed_from_tick || removed_from_impulse;
}

void Impulse::notify_tick(double value)
{
    m_last_context = create_context(value);

    for (auto& callback : m_callbacks) {
        callback(*m_last_context);
    }
    for (auto& [callback, condition] : m_conditional_callbacks) {
        if (condition(*m_last_context)) {
            callback(*m_last_context);
        }
    }
    if (m_impulse_occurred) {
        for (auto& callback : m_impulse_callbacks) {
            callback(*m_last_context);
        }
    }
}

void Impulse::save_state()
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

void Impulse::restore_state()
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

} // namespace MayaFlux::Nodes::Generator
