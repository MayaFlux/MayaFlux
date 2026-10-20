#include "Phasor.hpp"

namespace MayaFlux::Nodes::Generator {

Phasor::Phasor(float frequency, double amplitude, float offset)
    : m_offset(offset)
    , m_frequency_modulator(nullptr)
    , m_amplitude_modulator(nullptr)
    , m_phase_wrapped(false)
    , m_threshold_crossed(false)
{
    m_amplitude = amplitude;
    m_frequency = frequency;
    update_phase_increment(frequency);
}

Phasor::Phasor(const std::shared_ptr<Node>& frequency_modulator, float frequency, double amplitude, float offset)
    : m_offset(offset)
    , m_frequency_modulator(frequency_modulator)
    , m_amplitude_modulator(nullptr)
    , m_phase_wrapped(false)
    , m_threshold_crossed(false)
{
    m_amplitude = amplitude;
    m_frequency = frequency;
    update_phase_increment(frequency);
}

Phasor::Phasor(float frequency, const std::shared_ptr<Node>& amplitude_modulator, double amplitude, float offset)
    : m_offset(offset)
    , m_frequency_modulator(nullptr)
    , m_amplitude_modulator(amplitude_modulator)
    , m_phase_wrapped(false)
    , m_threshold_crossed(false)
{
    m_amplitude = amplitude;
    update_phase_increment(frequency);
}

Phasor::Phasor(const std::shared_ptr<Node>& frequency_modulator, const std::shared_ptr<Node>& amplitude_modulator,
    float frequency, double amplitude, float offset)
    : m_offset(offset)
    , m_frequency_modulator(frequency_modulator)
    , m_amplitude_modulator(amplitude_modulator)
    , m_phase_wrapped(false)
    , m_threshold_crossed(false)
{
    m_amplitude = amplitude;
    m_frequency = frequency;
    update_phase_increment(frequency);
}

void Phasor::set_frequency(float frequency)
{
    m_frequency = frequency;
    update_phase_increment(frequency);
}

void Phasor::update_phase_increment(double frequency)
{
    m_phase_inc = frequency / (double)m_timing_rate;
}

void Phasor::set_frequency_modulator(const std::shared_ptr<Node>& modulator)
{
    m_frequency_modulator = modulator;
}

void Phasor::set_amplitude_modulator(const std::shared_ptr<Node>& modulator)
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
        atomic_inc_modulator_count(m_frequency_modulator->m_modulator_count, 1);
        uint32_t state = m_frequency_modulator->m_state.load();
        if (state & NodeState::PROCESSED) {
            effective_freq += m_frequency_modulator->get_last_output();
        } else {
            effective_freq = m_frequency_modulator->process_sample(0.F);
            atomic_add_flag(m_frequency_modulator->m_state, NodeState::PROCESSED);
        }

        update_phase_increment(effective_freq);
    }

    output = m_phase * m_amplitude;

    if (m_amplitude_modulator) {
        atomic_inc_modulator_count(m_amplitude_modulator->m_modulator_count, 1);
        uint32_t state = m_amplitude_modulator->m_state.load();
        if (state & NodeState::PROCESSED) {
            output *= m_amplitude_modulator->get_last_output();
        } else {
            output *= m_amplitude_modulator->process_sample(0.F);
            atomic_add_flag(m_amplitude_modulator->m_state, NodeState::PROCESSED);
        }
    }

    output += m_offset;

    m_phase += m_phase_inc;
    if (m_phase >= 1.0) {
        m_phase -= 1.0;
        m_phase_wrapped = true;
    }

    m_last_output = output;

    if ((!m_state_saved || (m_state_saved && m_fire_events_during_snapshot))
        && !m_networked_node) {
        notify_tick(output);
    }

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

void Phasor::on_phase_wrap(const TypedHook<GeneratorContext>& callback)
{
    m_phase_wrap_callbacks.add_unique(adapt_hook(callback), callback.target_type());
}

void Phasor::on_threshold(const TypedHook<GeneratorContext>& callback, double threshold, bool /*rising*/)
{
    m_threshold_callbacks.add_unique(adapt_hook(callback), callback.target_type(), 0, { threshold, 0.0 });
}

bool Phasor::remove_threshold_callback(const TypedHook<GeneratorContext>& callback)
{
    return m_threshold_callbacks.remove_first_by_key(callback.target_type());
}

bool Phasor::remove_hook(const TypedHook<GeneratorContext>& callback)
{
    return remove_threshold_callback(callback);
}

void Phasor::notify_tick(double value)
{
    update_context(value);

    auto& ctx = get_last_context();

    m_callbacks.fire(ctx);

    if (m_phase_wrapped) {
        m_phase_wrap_callbacks.fire(ctx);
    }

    m_conditional_callbacks.fire(ctx);

    for (const auto& entry : m_threshold_callbacks.entries()) {
        const double threshold = entry.params[0];
        if (value >= threshold && !m_threshold_crossed) {
            entry.hook(ctx);
            m_threshold_crossed = true;
        } else if (value < threshold) {
            m_threshold_crossed = false;
        }
    }
}

void Phasor::save_state()
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

void Phasor::restore_state()
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

std::vector<std::pair<ModulatorRole, std::shared_ptr<Node>>>
Phasor::get_modulators() const
{
    std::vector<std::pair<ModulatorRole, std::shared_ptr<Node>>> result;
    if (m_frequency_modulator)
        result.emplace_back(ModulatorRole::FrequencyMod, m_frequency_modulator);
    if (m_amplitude_modulator)
        result.emplace_back(ModulatorRole::AmplitudeMod, m_amplitude_modulator);
    return result;
}

} // namespace MayaFlux::Nodes::Generator
