#include "LogicProcessor.hpp"
#include "MayaFlux/Buffers/AudioBuffer.hpp"

namespace MayaFlux::Buffers {

LogicProcessor::LogicProcessor(
    std::shared_ptr<Nodes::Generator::Logic> logic,
    ProcessMode mode,
    bool reset_between_buffers)
    : m_logic(logic)
    , m_process_mode(mode)
    , m_reset_between_buffers(reset_between_buffers)
    , m_threshold(0.5)
    , m_edge_type(Nodes::Generator::EdgeType::BOTH)
    , m_last_value(0.0)
    , m_last_state(false)
    , m_use_internal(false)
    , m_modulation_type(ModulationType::REPLACE)
    , m_has_generated_data(false)
{
    if (m_logic && m_logic->get_operator() == Nodes::Generator::LogicOperator::THRESHOLD) {
        m_threshold = m_logic->get_threshold();
    }

    if (m_logic && m_logic->get_operator() == Nodes::Generator::LogicOperator::EDGE) {
        m_edge_type = m_logic->get_edge_type();
    }
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

    std::vector<double> data;

    if (!input_data.empty()) {
        data = input_data;
        if (data.size() < num_samples) {
            data.resize(num_samples, 0.0);
        }
    } else {
        data.resize(num_samples, 0.0);
    }

    if (m_reset_between_buffers) {
        m_logic->reset();
        m_last_value = 0.0;
        m_last_state = false;
    }

    const auto& state = m_logic->m_state.load();

    switch (m_process_mode) {
    case ProcessMode::SAMPLE_BY_SAMPLE:
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
        break;

    case ProcessMode::THRESHOLD_CROSSING:
        if (state == Utils::NodeState::INACTIVE) {
            for (size_t i = 0; i < data.size(); ++i) {
                bool current_state = data[i] > m_threshold;

                if (current_state != m_last_state) {
                    m_last_state = current_state;
                    m_logic_data[i] = m_logic->process_sample(data[i]);
                } else {
                    m_logic_data[i] = m_logic->get_last_output();
                }
            }
        } else {
            m_logic->save_state();
            for (size_t i = 0; i < data.size(); ++i) {
                bool current_state = data[i] > m_threshold;

                if (current_state != m_last_state) {
                    m_last_state = current_state;
                    m_logic_data[i] = m_logic->process_sample(data[i]);
                } else {
                    m_logic_data[i] = m_logic->get_last_output();
                }
            }
            m_logic->restore_state();
        }
        break;

    case ProcessMode::EDGE_TRIGGERED:
        if (state == Utils::NodeState::INACTIVE) {
            for (size_t i = 0; i < data.size(); ++i) {
                bool current_state = data[i] > m_threshold;
                bool edge_detected = false;

                if (current_state != m_last_state) {
                    switch (m_edge_type) {
                    case Nodes::Generator::EdgeType::RISING:
                        edge_detected = current_state && !m_last_state;
                        break;
                    case Nodes::Generator::EdgeType::FALLING:
                        edge_detected = !current_state && m_last_state;
                        break;
                    case Nodes::Generator::EdgeType::BOTH:
                        edge_detected = true;
                        break;
                    }
                }

                if (edge_detected) {
                    m_logic->set_edge_detection(m_edge_type, m_threshold);
                    m_logic_data[i] = m_logic->process_sample(data[i]);
                } else {
                    m_logic_data[i] = m_logic->get_last_output();
                }

                m_last_state = current_state;
            }
        } else {
            m_logic->save_state();
            for (size_t i = 0; i < data.size(); ++i) {
                bool current_state = data[i] > m_threshold;
                bool edge_detected = false;

                if (current_state != m_last_state) {
                    switch (m_edge_type) {
                    case Nodes::Generator::EdgeType::RISING:
                        edge_detected = current_state && !m_last_state;
                        break;
                    case Nodes::Generator::EdgeType::FALLING:
                        edge_detected = !current_state && m_last_state;
                        break;
                    case Nodes::Generator::EdgeType::BOTH:
                        edge_detected = true;
                        break;
                    }
                }

                if (edge_detected) {
                    m_logic->set_edge_detection(m_edge_type, m_threshold);
                    m_logic_data[i] = m_logic->process_sample(data[i]);
                } else {
                    m_logic_data[i] = m_logic->get_last_output();
                }

                m_last_state = current_state;
            }
            m_logic->restore_state();
        }
        break;

    case ProcessMode::STATE_MACHINE:
        if (m_reset_between_buffers) {
            m_logic->reset();
        }

        if (m_logic->get_mode() == Nodes::Generator::LogicMode::SEQUENTIAL) {
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
        } else {
            auto history_size = std::min(data.size(), size_t(16));
            m_logic->set_sequential_function(
                [this, &data](const std::deque<bool>& history) -> bool {
                    return !history.empty() && history.front();
                },
                history_size);

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
        }
        break;
    }

    if (!m_logic_data.empty()) {
        m_last_value = m_logic_data.back();
    }
    m_has_generated_data = true;
    return true;
}

bool LogicProcessor::apply(std::shared_ptr<Buffer> buffer, ModulationFunction modulation_func)
{
    if (!buffer || !m_has_generated_data) {
        return false;
    }

    auto& buffer_data = std::dynamic_pointer_cast<AudioBuffer>(buffer)->get_data();
    size_t min_size = std::min(m_logic_data.size(), buffer_data.size());

    if (!modulation_func) {
        switch (m_modulation_type) {
        case ModulationType::REPLACE:
            modulation_func = [](double logic_val, double audio_val) {
                return logic_val;
            };
            break;

        case ModulationType::MULTIPLY:
            modulation_func = [](double logic_val, double audio_val) {
                return logic_val * audio_val;
            };
            break;

        case ModulationType::ADD:
            modulation_func = [](double logic_val, double audio_val) {
                return logic_val + audio_val;
            };
            break;

        case ModulationType::CUSTOM:
            modulation_func = m_modulation_function;
            break;

        default:
            modulation_func = [](double logic_val, double audio_val) {
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
    if (!m_logic || !buffer || std::dynamic_pointer_cast<AudioBuffer>(buffer)->get_data().empty()) {
        return;
    }

    generate(std::dynamic_pointer_cast<AudioBuffer>(buffer)->get_num_samples(), std::dynamic_pointer_cast<AudioBuffer>(buffer)->get_data());

    apply(buffer);
}

void LogicProcessor::on_attach(std::shared_ptr<Buffer> buffer)
{
    if (m_logic) {
        m_logic->reset();
        m_last_value = 0.0;
        m_last_state = false;
    }
}

void LogicProcessor::set_process_mode(ProcessMode mode)
{
    if (m_process_mode != mode) {
        m_process_mode = mode;
        m_logic_data.clear();
        m_has_generated_data = false;
    }
}

void LogicProcessor::set_modulation_function(ModulationFunction func)
{
    m_modulation_function = func;
    m_modulation_type = ModulationType::CUSTOM;
}

void LogicProcessor::set_threshold(double threshold)
{
    m_threshold = threshold;

    if (m_logic) {
        if (m_logic->get_operator() == Nodes::Generator::LogicOperator::THRESHOLD) {
            m_logic->set_threshold(threshold);
        } else if (m_logic->get_operator() == Nodes::Generator::LogicOperator::EDGE) {
            m_logic->set_edge_detection(m_edge_type, threshold);
        }
    }
}

void LogicProcessor::set_edge_type(Nodes::Generator::EdgeType type)
{
    m_edge_type = type;

    if (m_logic && m_logic->get_operator() == Nodes::Generator::LogicOperator::EDGE) {
        m_logic->set_edge_detection(type, m_threshold);
    }
}

} // namespace MayaFlux::Buffers
