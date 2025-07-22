#include "Sine.hpp"
#include "MayaFlux/API/Core.hpp"

namespace MayaFlux::Nodes::Generator {

Sine::Sine(float frequency, double amplitude, float offset)
    : m_phase(0)
    , m_frequency(frequency)
    , m_offset(offset)
    , m_frequency_modulator(nullptr)
    , m_amplitude_modulator(nullptr)
{

    m_amplitude = amplitude;
    update_phase_increment(frequency);
}

Sine::Sine(std::shared_ptr<Node> frequency_modulator, float frequency, double amplitude, float offset)
    : m_phase(0)
    , m_frequency(frequency)
    , m_offset(offset)
    , m_frequency_modulator(frequency_modulator)
    , m_amplitude_modulator(nullptr)
{
    m_amplitude = amplitude;
    update_phase_increment(frequency);
}

Sine::Sine(float frequency, std::shared_ptr<Node> amplitude_modulator, double amplitude, float offset)
    : m_phase(0)
    , m_frequency(frequency)
    , m_offset(offset)
    , m_frequency_modulator(nullptr)
    , m_amplitude_modulator(amplitude_modulator)
{
    m_amplitude = amplitude;
    update_phase_increment(frequency);
}

Sine::Sine(std::shared_ptr<Node> frequency_modulator, std::shared_ptr<Node> amplitude_modulator, float frequency, double amplitude, float offset)
    : m_phase(0)
    , m_frequency(frequency)
    , m_offset(offset)
    , m_frequency_modulator(frequency_modulator)
    , m_amplitude_modulator(amplitude_modulator)
{
    m_amplitude = amplitude;
    update_phase_increment(frequency);
}

void Sine::set_frequency(float frequency)
{
    m_frequency = frequency;
    update_phase_increment(frequency);
}

void Sine::update_phase_increment(float frequency)
{

    u_int64_t s_rate = 48000u;
    if (MayaFlux::is_engine_initialized()) {
        s_rate = MayaFlux::get_sample_rate();
    }
    m_phase_inc = (2 * M_PI * frequency) / s_rate;
}

void Sine::set_frequency_modulator(std::shared_ptr<Node> modulator)
{
    m_frequency_modulator = modulator;
}

void Sine::set_amplitude_modulator(std::shared_ptr<Node> modulator)
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
        u_int32_t state = m_frequency_modulator->m_state.load();
        double current_freq = m_frequency;

        if (state & Utils::NodeState::PROCESSED) {
            current_freq += m_frequency_modulator->get_last_output();
        } else {
            current_freq += m_frequency_modulator->process_sample(0.f);
            atomic_add_flag(m_frequency_modulator->m_state, Utils::NodeState::PROCESSED);
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

    float current_amplitude = m_amplitude;
    if (m_amplitude_modulator) {
        atomic_inc_modulator_count(m_amplitude_modulator->m_modulator_count, 1);
        u_int32_t state = m_amplitude_modulator->m_state.load();

        if (state & Utils::NodeState::PROCESSED) {
            current_amplitude += m_amplitude_modulator->get_last_output();
            if (!(state & Utils::NodeState::ACTIVE)) {
                atomic_remove_flag(m_amplitude_modulator->m_state, Utils::NodeState::PROCESSED);
            }
        } else {
            current_amplitude += m_amplitude_modulator->process_sample(0.f);
            atomic_add_flag(m_amplitude_modulator->m_state, Utils::NodeState::PROCESSED);
        }
    }
    current_sample *= current_amplitude;

    if (input) {
        current_sample += input;
        current_sample *= 0.5f;
    }

    notify_tick(current_sample);

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

void Sine::reset(float frequency, float amplitude, float offset)
{
    m_phase = 0;
    m_frequency = frequency;
    m_amplitude = amplitude;
    m_offset = offset;
    update_phase_increment(frequency);
}

std::unique_ptr<NodeContext> Sine::create_context(double value)
{
    return std::make_unique<GeneratorContext>(value, m_frequency, m_amplitude, m_phase);
}

void Sine::notify_tick(double value)
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
}

void Sine::reset_processed_state()
{
    atomic_remove_flag(m_state, Utils::NodeState::PROCESSED);
    if (m_frequency_modulator) {
        m_frequency_modulator->reset_processed_state();
    }
    if (m_amplitude_modulator) {
        m_amplitude_modulator->reset_processed_state();
    }
}

void Sine::printGraph()
{
}

void Sine::printCurrent()
{
}

}
