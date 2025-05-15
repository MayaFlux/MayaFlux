#include "Sine.hpp"
#include "MayaFlux/MayaFlux.hpp"
#include "MayaFlux/Nodes/NodeGraphManager.hpp"

namespace MayaFlux::Nodes::Generator {

Sine::Sine(float frequency, float amplitude, float offset, bool bAuto_register)
    : m_phase(0)
    , m_amplitude(amplitude)
    , m_frequency(frequency)
    , m_offset(offset)
    , m_frequency_modulator(nullptr)
    , m_amplitude_modulator(nullptr)
    , m_is_registered(false)
    , m_is_processed(false)
{
    Setup(bAuto_register);
}

Sine::Sine(std::shared_ptr<Node> frequency_modulator, float frequency, float amplitude, float offset, bool bAuto_register)
    : m_phase(0)
    , m_amplitude(amplitude)
    , m_frequency(frequency)
    , m_offset(offset)
    , m_frequency_modulator(frequency_modulator)
    , m_amplitude_modulator(nullptr)
    , m_is_registered(false)
    , m_is_processed(false)
{
    Setup(bAuto_register);
}

Sine::Sine(float frequency, std::shared_ptr<Node> amplitude_modulator, float amplitude, float offset, bool bAuto_register)
    : m_phase(0)
    , m_amplitude(amplitude)
    , m_frequency(frequency)
    , m_offset(offset)
    , m_frequency_modulator(nullptr)
    , m_amplitude_modulator(amplitude_modulator)
    , m_is_registered(false)
    , m_is_processed(false)
{
    Setup(bAuto_register);
}

Sine::Sine(std::shared_ptr<Node> frequency_modulator, std::shared_ptr<Node> amplitude_modulator, float frequency, float amplitude, float offset, bool bAuto_register)
    : m_phase(0)
    , m_amplitude(amplitude)
    , m_frequency(frequency)
    , m_offset(offset)
    , m_frequency_modulator(frequency_modulator)
    , m_amplitude_modulator(amplitude_modulator)
    , m_is_registered(false)
    , m_is_processed(false)
{
    Setup(bAuto_register);
}

void Sine::Setup(bool bAuto_register)
{
    m_phase_inc = (2 * M_PI * m_frequency) / MayaFlux::get_sample_rate();

    if (bAuto_register) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        register_to_defult();
    }
}

void Sine::set_frequency(float frequency)
{
    m_frequency = frequency;
    m_phase_inc = (2 * M_PI * m_frequency) / MayaFlux::get_sample_rate();
}

void Sine::update_phase_increment(float frequency)
{
    m_phase_inc = (2 * M_PI * frequency) / MayaFlux::get_sample_rate();
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
        double current_freq = m_frequency;
        if (m_frequency_modulator->is_processed()) {
            current_freq += m_frequency_modulator->get_last_output();
            m_frequency_modulator->mark_processed(false);
        } else {
            current_freq += m_frequency_modulator->process_sample(0.f);
            m_frequency_modulator->mark_processed(true);
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
        if (m_amplitude_modulator->is_processed()) {
            current_amplitude += m_amplitude_modulator->get_last_output();
            m_amplitude_modulator->mark_processed(false);
        } else {
            current_amplitude += m_amplitude_modulator->process_sample(0.f);
            m_amplitude_modulator->mark_processed(true);
        }
    }
    current_sample *= current_amplitude;

    if (input) {
        current_sample += input;
        current_sample *= 0.5f;
    }

    notify_tick(current_sample);

    return current_sample;
}

std::vector<double> Sine::processFull(unsigned int num_samples)
{
    std::vector<double> output(num_samples);
    for (unsigned int i = 0; i < num_samples; i++) {
        output[i] = process_sample(0.0);
    }
    return output;
}

void Sine::register_to_defult()
{
    try {
        auto self = shared_from_this();
        MayaFlux::add_node_to_root(self);
    } catch (const std::bad_weak_ptr& e) {
        std::cerr << "Error in register_to_defult: " << e.what() << std::endl;
        std::cerr << "The Sine object must be created with std::make_shared for shared_from_this() to work." << std::endl;

        MayaFlux::add_node_to_root(std::make_shared<Sine>(*this));
    }
}

void Sine::reset(float frequency, float amplitude, float offset)
{
    m_phase = 0;
    m_frequency = frequency;
    m_amplitude = amplitude;
    m_offset = offset;
    if (MayaFlux::is_engine_initialized()) {
        m_phase_inc = (2 * M_PI * m_frequency) / MayaFlux::get_sample_rate();
    } else {
        m_phase_inc = (2 * M_PI * m_frequency) / 48000.f;
    }
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

void Sine::on_tick(NodeHook callback)
{
    safe_add_callback(m_callbacks, callback);
}

void Sine::on_tick_if(NodeHook callback, NodeCondition condition)
{
    safe_add_conditional_callback(m_conditional_callbacks, callback, condition);
}

bool Sine::remove_hook(const NodeHook& callback)
{
    return safe_remove_callback(m_callbacks, callback);
}

bool Sine::remove_conditional_hook(const NodeCondition& callback)
{
    return safe_remove_conditional_callback(m_conditional_callbacks, callback);
}

void Sine::reset_processed_state()
{
    mark_processed(false);
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
