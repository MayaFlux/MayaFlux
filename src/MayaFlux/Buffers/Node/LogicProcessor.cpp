#include "LogicProcessor.hpp"

#include "MayaFlux/Buffers/AudioBuffer.hpp"

namespace MayaFlux::Buffers {

LogicProcessor::LogicProcessor(
    const std::shared_ptr<Nodes::Generator::Logic>& logic,
    bool reset_between_buffers)
    : m_logic(logic)
    , m_reset_between_buffers(reset_between_buffers)
    , m_use_internal(false)
    , m_modulation_type(ModulationType::REPLACE)
    , m_has_generated_data(false)
    , m_high_value(1.0)
    , m_low_value(0.0)
    , m_last_held_value(0.0)
    , m_last_logic_value(0.0)
{
}

bool LogicProcessor::generate(size_t num_samples, const std::vector<double>& input_data)
{
    if (!m_logic || input_data.empty()) {
        return false;
    }

    if (m_pending_logic) {
        m_logic = m_pending_logic;
        m_pending_logic.reset();
        m_use_internal = true;
    }

    m_logic_data.resize(num_samples, 0);

    std::vector<double> data = input_data;
    if (data.size() < num_samples) {
        data.resize(num_samples, 0.0);
    }

    if (m_reset_between_buffers) {
        m_logic->reset();
    }

    const auto& state = m_logic->m_state.load();

    if (state == Utils::NodeState::INACTIVE) {
        for (size_t i = 0; i < data.size(); ++i) {
            m_logic_data[i] = m_logic->process_sample(data[i]);
        }
    } else {
        m_logic->save_state();
        for (size_t i = 0; i < data.size(); ++i) {
            m_logic_data[i] = m_logic->process_sample(data[i]);
        }
        m_logic->restore_state();
    }

    m_has_generated_data = true;
    return true;
}

bool LogicProcessor::apply(const std::shared_ptr<Buffer>& buffer, ModulationFunction modulation_func)
{
    if (!buffer || !m_has_generated_data) {
        return false;
    }

    auto& buffer_data = std::dynamic_pointer_cast<AudioBuffer>(buffer)->get_data();
    size_t min_size = std::min(m_logic_data.size(), buffer_data.size());

    if (!modulation_func) {
        switch (m_modulation_type) {
        case ModulationType::REPLACE:
            modulation_func = [](double logic_val, double /*buffer_val*/) {
                return logic_val;
            };
            break;

        case ModulationType::MULTIPLY:
            modulation_func = [](double logic_val, double buffer_val) {
                return logic_val * buffer_val;
            };
            break;

        case ModulationType::ADD:
            modulation_func = [](double logic_val, double buffer_val) {
                return logic_val + buffer_val;
            };
            break;

        case ModulationType::INVERT_ON_TRUE:
            modulation_func = [](double logic_val, double buffer_val) {
                return logic_val > 0.5 ? -buffer_val : buffer_val;
            };
            break;

        case ModulationType::HOLD_ON_FALSE:
            if (min_size > 0) {
                m_last_held_value = buffer_data[0];
            }

            modulation_func = [this](double logic_val, double buffer_val) mutable {
                if (logic_val > 0.5) {
                    m_last_held_value = buffer_val;
                    return buffer_val;
                }
                return m_last_held_value;
            };
            break;

        case ModulationType::ZERO_ON_FALSE:
            modulation_func = [](double logic_val, double buffer_val) {
                return logic_val > 0.5 ? buffer_val : 0.0;
            };
            break;

        case ModulationType::CROSSFADE:
            modulation_func = [](double logic_val, double buffer_val) {
                return buffer_val * logic_val;
            };
            break;

        case ModulationType::THRESHOLD_REMAP:
            modulation_func = [this](double logic_val, double /*buffer_val*/) {
                return logic_val > 0.5 ? m_high_value : m_low_value;
            };
            break;

        case ModulationType::SAMPLE_AND_HOLD:
            if (min_size > 0) {
                m_last_held_value = buffer_data[0];
                m_last_logic_value = m_logic_data[0];
            }

            modulation_func = [this, first_sample = true](double logic_val, double buffer_val) mutable {
                if (first_sample) {
                    first_sample = false;
                    return buffer_val;
                }

                bool logic_changed = std::abs(logic_val - m_last_logic_value) > 0.01;
                m_last_logic_value = logic_val;

                if (logic_changed) {
                    m_last_held_value = buffer_val;
                }
                return m_last_held_value;
            };
            break;

        case ModulationType::CUSTOM:
            modulation_func = m_modulation_function;
            break;

        default:
            modulation_func = [](double logic_val, double /*buffer_val*/) {
                return logic_val;
            };
        }
    }

    for (size_t i = 0; i < min_size; ++i) {
        buffer_data[i] = modulation_func(m_logic_data[i], buffer_data[i]);
    }

    return true;
}

void LogicProcessor::processing_function(std::shared_ptr<Buffer> buffer)
{
    if (!m_logic || !buffer) {
        return;
    }

    auto audio_buffer = std::dynamic_pointer_cast<AudioBuffer>(buffer);
    if (!audio_buffer || audio_buffer->get_data().empty()) {
        return;
    }

    generate(audio_buffer->get_num_samples(), audio_buffer->get_data());
    apply(buffer);
}

void LogicProcessor::on_attach(std::shared_ptr<Buffer> /*buffer*/)
{
    if (m_logic) {
        m_logic->reset();
    }

    m_last_held_value = 0.0;
    m_last_logic_value = 0.0;
}

void LogicProcessor::set_modulation_function(ModulationFunction func)
{
    m_modulation_function = std::move(func);
    m_modulation_type = ModulationType::CUSTOM;
}

} // namespace MayaFlux::Buffers
