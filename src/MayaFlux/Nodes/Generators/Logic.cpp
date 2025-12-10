#include "Logic.hpp"

#include "MayaFlux/API/Config.hpp"

namespace MayaFlux::Nodes::Generator {

//-------------------------------------------------------------------------
// Constructors
//-------------------------------------------------------------------------

Logic::Logic(double threshold)
    : m_mode(LogicMode::DIRECT)
    , m_operator(LogicOperator::THRESHOLD)
    , m_history_size(1)
    , m_input_count(1)
    , m_threshold(threshold)
    , m_low_threshold(threshold * 0.9)
    , m_high_threshold(threshold)
    , m_edge_type(EdgeType::BOTH)
    , m_edge_detected(false)
    , m_hysteresis_state(false)
    , m_temporal_time(0.0)
{
    m_direct_function = [this](double input) {
        return input > m_threshold;
    };
}

Logic::Logic(LogicOperator op, double threshold)
    : m_mode(LogicMode::DIRECT)
    , m_operator(op)
    , m_history_size(1)
    , m_input_count(1)
    , m_threshold(threshold)
    , m_low_threshold(threshold * 0.9)
    , m_high_threshold(threshold)
    , m_edge_type(EdgeType::BOTH)
    , m_edge_detected(false)
    , m_hysteresis_state(false)
    , m_temporal_time(0.0)
{
    set_operator(op);
}

Logic::Logic(DirectFunction function)
    : m_mode(LogicMode::DIRECT)
    , m_operator(LogicOperator::CUSTOM)
    , m_direct_function(std::move(function))
    , m_history_size(1)
    , m_input_count(1)
    , m_threshold(0.5)
    , m_low_threshold(0.45)
    , m_high_threshold(0.5)
    , m_edge_type(EdgeType::BOTH)
    , m_edge_detected(false)
    , m_hysteresis_state(false)
    , m_temporal_time(0.0)
{
}

Logic::Logic(MultiInputFunction function, size_t input_count)
    : m_mode(LogicMode::MULTI_INPUT)
    , m_operator(LogicOperator::CUSTOM)
    , m_multi_input_function(std::move(function))
    , m_history_size(1)
    , m_input_count(input_count)
    , m_threshold(0.5)
    , m_low_threshold(0.45)
    , m_high_threshold(0.5)
    , m_edge_type(EdgeType::BOTH)
    , m_edge_detected(false)
    , m_hysteresis_state(false)
    , m_temporal_time(0.0)
{
    m_input_buffer.resize(input_count, 0.0);
}

Logic::Logic(SequentialFunction function, size_t history_size)
    : m_mode(LogicMode::SEQUENTIAL)
    , m_operator(LogicOperator::CUSTOM)
    , m_sequential_function(std::move(function))
    , m_history_size(history_size)
    , m_input_count(1)
    , m_threshold(0.5)
    , m_low_threshold(0.45)
    , m_high_threshold(0.5)
    , m_edge_type(EdgeType::BOTH)
    , m_edge_detected(false)
    , m_hysteresis_state(false)
    , m_temporal_time(0.0)
{
    m_history.assign(history_size, false);
}

Logic::Logic(TemporalFunction function)
    : m_mode(LogicMode::TEMPORAL)
    , m_operator(LogicOperator::CUSTOM)
    , m_temporal_function(std::move(function))
    , m_history_size(1)
    , m_input_count(1)
    , m_threshold(0.5)
    , m_low_threshold(0.45)
    , m_high_threshold(0.5)
    , m_edge_type(EdgeType::BOTH)
    , m_edge_detected(false)
    , m_hysteresis_state(false)
    , m_temporal_time(0.0)
{
}

//-------------------------------------------------------------------------
// Processing Methods
//-------------------------------------------------------------------------

double Logic::process_sample(double input)
{
    bool result = false;
    m_edge_detected = false;

    if (m_input_node) {
        atomic_inc_modulator_count(m_input_node->m_modulator_count, 1);
        uint32_t state = m_input_node->m_state.load();
        if (state & Utils::NodeState::PROCESSED) {
            input += m_input_node->get_last_output();
        } else {
            input = m_input_node->process_sample(input);
            atomic_add_flag(m_input_node->m_state, Utils::NodeState::PROCESSED);
        }
    }

    bool current_bool = input > m_threshold;
    bool previous_bool = m_last_output > 0.5;

    switch (m_mode) {
    case LogicMode::DIRECT:
        switch (m_operator) {
        case LogicOperator::THRESHOLD:
            result = current_bool;
            break;

        case LogicOperator::HYSTERESIS:
            if (input > m_high_threshold) {
                m_hysteresis_state = true;
            } else if (input < m_low_threshold) {
                m_hysteresis_state = false;
            }
            result = m_hysteresis_state;
            break;

        case LogicOperator::EDGE: {
            bool previous_bool_input = m_input > m_threshold;
            if (current_bool != previous_bool_input) {
                switch (m_edge_type) {
                case EdgeType::RISING:
                    m_edge_detected = current_bool && !previous_bool_input;
                    break;
                case EdgeType::FALLING:
                    m_edge_detected = !current_bool && previous_bool_input;
                    break;
                case EdgeType::BOTH:
                    m_edge_detected = true;
                    break;
                }
            }
            result = m_edge_detected;
            break;
        }

        case LogicOperator::AND:
            result = current_bool && previous_bool;
            break;

        case LogicOperator::OR:
            result = current_bool || previous_bool;
            break;

        case LogicOperator::XOR:
            result = current_bool != previous_bool;
            break;

        case LogicOperator::NOT:
            result = !current_bool;
            break;

        case LogicOperator::NAND:
            result = !(current_bool && previous_bool);
            break;

        case LogicOperator::NOR:
            result = !(current_bool || previous_bool);
            break;

        case LogicOperator::CUSTOM:
            result = m_direct_function(input);
            break;

        default:
            result = current_bool;
        }
        break;

    case LogicMode::SEQUENTIAL: {
        bool current_bool = input > m_threshold;
        m_history.push_front(current_bool);
        if (m_history.size() > m_history_size) {
            m_history.pop_back();
        }
        result = m_sequential_function(m_history);
        break;
    }

    case LogicMode::TEMPORAL: {
        m_temporal_time += 1.0 / MayaFlux::Config::get_sample_rate();
        result = m_temporal_function(input, m_temporal_time);
        break;
    }

    case LogicMode::MULTI_INPUT: {
        add_input(input, 0);
        result = m_multi_input_function(m_input_buffer);
        break;
    }
    }

    m_input = input;
    auto current = result ? 1.0 : 0.0;

    if (!m_state_saved || (m_state_saved && m_fire_events_during_snapshot))
        notify_tick(current);

    if (m_input_node) {
        atomic_dec_modulator_count(m_input_node->m_modulator_count, 1);
        try_reset_processed_state(m_input_node);
    }

    m_last_output = current;
    return m_last_output;
}

std::vector<double> Logic::process_batch(unsigned int num_samples)
{
    std::vector<double> output(num_samples);

    for (unsigned int i = 0; i < num_samples; ++i) {
        // For batch processing, we use 0.0 as input
        // This is mainly useful for temporal or sequential modes
        output[i] = process_sample(0.0);
    }

    return output;
}

double Logic::process_multi_input(const std::vector<double>& inputs)
{
    if (m_mode != LogicMode::MULTI_INPUT) {
        m_mode = LogicMode::MULTI_INPUT;
        m_operator = LogicOperator::CUSTOM;

        if (m_input_buffer.size() < inputs.size()) {
            m_input_buffer.resize(inputs.size(), 0.0);
            m_input_count = inputs.size();
        }

        if (!m_multi_input_function) {
            m_multi_input_function = [this](const std::vector<double>& inputs) {
                bool result = true;
                for (const auto& input : inputs) {
                    result = result && (input > m_threshold);
                }
                return result;
            };
        }
    }

    // Copy inputs to our buffer
    for (size_t i = 0; i < inputs.size() && i < m_input_buffer.size(); ++i) {
        m_input_buffer[i] = inputs[i];
    }

    bool result = m_multi_input_function(m_input_buffer);
    m_last_output = result ? 1.0 : 0.0;

    notify_tick(m_last_output);

    return m_last_output;
}

void Logic::add_input(double input, size_t index)
{
    if (index >= m_input_buffer.size()) {
        m_input_buffer.resize(index + 1, 0.0);
    }
    m_input_buffer[index] = input;
}

//-------------------------------------------------------------------------
// Configuration Methods
//-------------------------------------------------------------------------

void Logic::reset()
{
    m_history.clear();
    m_history.assign(m_history_size, false);
    m_edge_detected = false;
    m_last_output = 0.0;
    m_hysteresis_state = false;
    m_temporal_time = 0.0;
    m_input_buffer.assign(m_input_count, 0.0);
}

void Logic::set_threshold(double threshold, bool create_default_direct_function)
{
    m_threshold = threshold;
    m_high_threshold = threshold;
    m_low_threshold = threshold * 0.9; // Default hysteresis

    if (m_operator == LogicOperator::THRESHOLD && m_mode == LogicMode::DIRECT && create_default_direct_function) {
        m_direct_function = [this](double input) {
            return input > m_threshold;
        };
    }
}

void Logic::set_hysteresis(double low_threshold, double high_threshold, bool create_default_direct_function)
{
    m_low_threshold = low_threshold;
    m_high_threshold = high_threshold;
    m_threshold = high_threshold; // For compatibility with other methods

    if (m_operator == LogicOperator::HYSTERESIS && m_mode == LogicMode::DIRECT && create_default_direct_function) {
        m_direct_function = [this](double input) {
            if (input > m_high_threshold) {
                m_hysteresis_state = true;
            } else if (input < m_low_threshold) {
                m_hysteresis_state = false;
            }
            return m_hysteresis_state;
        };
    }
}

void Logic::set_edge_detection(EdgeType type, double threshold)
{
    m_edge_type = type;
    m_threshold = threshold;
    m_operator = LogicOperator::EDGE;
}

void Logic::set_operator(LogicOperator op, bool create_default_direct_function)
{
    m_operator = op;

    if (create_default_direct_function) {
        switch (op) {
        case LogicOperator::AND:
            m_direct_function = [this](double input) {
                bool current = input > m_threshold;
                bool previous = m_last_output > 0.5;
                return current && previous;
            };
            break;

        case LogicOperator::OR:
            m_direct_function = [this](double input) {
                bool current = input > m_threshold;
                bool previous = m_last_output > 0.5;
                return current || previous;
            };
            break;

        case LogicOperator::XOR:
            m_direct_function = [this](double input) {
                bool current = input > m_threshold;
                bool previous = m_last_output > 0.5;
                return current != previous;
            };
            break;

        case LogicOperator::NOT:
            m_direct_function = [this](double input) {
                return !(input > m_threshold);
            };
            break;

        case LogicOperator::NAND:
            m_direct_function = [this](double input) {
                bool current = input > m_threshold;
                bool previous = m_last_output > 0.5;
                return !(current && previous);
            };
            break;

        case LogicOperator::NOR:
            m_direct_function = [this](double input) {
                bool current = input > m_threshold;
                bool previous = m_last_output > 0.5;
                return !(current || previous);
            };
            break;

        case LogicOperator::THRESHOLD:
            m_direct_function = [this](double input) {
                return input > m_threshold;
            };
            break;

        case LogicOperator::HYSTERESIS:
            m_direct_function = [this](double input) {
                if (input > m_high_threshold) {
                    m_hysteresis_state = true;
                } else if (input < m_low_threshold) {
                    m_hysteresis_state = false;
                }
                return m_hysteresis_state;
            };
            break;

        case LogicOperator::EDGE:
            // Edge detection is handled in process_sample
            break;

        case LogicOperator::CUSTOM:
            // Custom function should be set directly
            break;
        }
    }
}

void Logic::set_direct_function(DirectFunction function)
{
    m_direct_function = std::move(function);
    m_mode = LogicMode::DIRECT;
    m_operator = LogicOperator::CUSTOM;
}

void Logic::set_multi_input_function(MultiInputFunction function, size_t input_count)
{
    m_multi_input_function = std::move(function);
    m_mode = LogicMode::MULTI_INPUT;
    m_operator = LogicOperator::CUSTOM;
    m_input_count = input_count;

    // Resize input buffer if needed
    if (m_input_buffer.size() != input_count) {
        m_input_buffer.resize(input_count, 0.0);
    }
}

void Logic::set_sequential_function(SequentialFunction function, size_t history_size)
{
    m_sequential_function = std::move(function);
    m_mode = LogicMode::SEQUENTIAL;
    m_operator = LogicOperator::CUSTOM;

    if (history_size != m_history_size) {
        m_history_size = history_size;
        m_history.clear();
        m_history.assign(history_size, false);
    }
}

void Logic::set_temporal_function(TemporalFunction function)
{
    m_temporal_function = std::move(function);
    m_mode = LogicMode::TEMPORAL;
    m_operator = LogicOperator::CUSTOM;
    // Reset temporal time when changing function
    m_temporal_time = 0.0;
}

void Logic::set_initial_conditions(const std::vector<bool>& initial_values)
{
    m_history.clear();

    for (const auto& value : initial_values) {
        m_history.push_back(value);
    }

    if (m_history.size() < m_history_size) {
        m_history.resize(m_history_size, false);
    } else if (m_history.size() > m_history_size) {
        m_history.resize(m_history_size);
    }
}

std::unique_ptr<NodeContext> Logic::create_context(double value)
{
    if (m_gpu_compatible) {
        return std::make_unique<LogicContextGpu>(
            value,
            m_mode,
            m_operator,
            m_history,
            m_threshold,
            m_edge_detected,
            m_edge_type,
            m_input_buffer,
            get_gpu_data_buffer());
    }

    return std::make_unique<LogicContext>(
        value,
        m_mode,
        m_operator,
        m_history,
        m_threshold,
        m_edge_detected,
        m_edge_type,
        m_input_buffer);
}

void Logic::notify_tick(double value)
{
    m_last_context = create_context(value);
    bool state_changed = (value != m_last_output);

    for (const auto& cb : m_all_callbacks) {
        bool should_call = false;

        switch (cb.event_type) {
        case LogicEventType::TICK:
            should_call = true;
            break;

        case LogicEventType::WHILE_TRUE:
            should_call = value;
            break;

        case LogicEventType::WHILE_FALSE:
            should_call = !value;
            break;

        case LogicEventType::CHANGE:
            should_call = state_changed;
            break;

        case LogicEventType::TRUE:
            should_call = state_changed && value;
            break;

        case LogicEventType::FALSE:
            should_call = state_changed && !value;
            break;

        case LogicEventType::CONDITIONAL:
            should_call = cb.condition && cb.condition.value()(*m_last_context);
            break;
        }

        if (should_call) {
            cb.callback(*m_last_context);
        }
    }
}

void Logic::on_tick(const NodeHook& callback)
{
    add_callback(callback, LogicEventType::TICK);
}

void Logic::on_tick_if(const NodeCondition& condition, const NodeHook& callback)
{
    add_callback(callback, LogicEventType::CONDITIONAL, condition);
}

void Logic::while_true(const NodeHook& callback)
{
    add_callback(callback, LogicEventType::WHILE_TRUE);
}

void Logic::while_false(const NodeHook& callback)
{
    add_callback(callback, LogicEventType::WHILE_FALSE);
}

void Logic::on_change(const NodeHook& callback)
{
    add_callback(callback, LogicEventType::CHANGE);
}

void Logic::on_change_to(bool target_state, const NodeHook& callback)
{
    add_callback(callback, target_state ? LogicEventType::TRUE : LogicEventType::FALSE);
}

bool Logic::remove_hook(const NodeHook& callback)
{
    auto it = std::remove_if(m_all_callbacks.begin(), m_all_callbacks.end(),
        [&callback](const LogicCallback& cb) {
            return cb.callback.target_type() == callback.target_type();
        });

    if (it != m_all_callbacks.end()) {
        m_all_callbacks.erase(it, m_all_callbacks.end());
        return true;
    }
    return false;
}

bool Logic::remove_conditional_hook(const NodeCondition& callback)
{
    auto it = std::remove_if(m_all_callbacks.begin(), m_all_callbacks.end(),
        [&callback](const LogicCallback& cb) {
            return cb.event_type == LogicEventType::CONDITIONAL && cb.condition && cb.condition->target_type() == callback.target_type();
        });

    if (it != m_all_callbacks.end()) {
        m_all_callbacks.erase(it, m_all_callbacks.end());
        return true;
    }
    return false;
}

void Logic::remove_hooks_of_type(LogicEventType type)
{
    auto it = std::remove_if(m_all_callbacks.begin(), m_all_callbacks.end(),
        [type](const LogicCallback& cb) {
            return cb.event_type == type;
        });
    m_all_callbacks.erase(it, m_all_callbacks.end());
}

void Logic::save_state()
{
    m_saved_history = m_history;
    m_saved_hysteresis_state = m_hysteresis_state;
    m_saved_edge_detected = m_edge_detected;
    m_saved_temporal_time = m_temporal_time;
    m_saved_last_output = m_last_output;

    if (m_input_node)
        m_input_node->save_state();

    m_state_saved = true;
}

void Logic::restore_state()
{
    m_history = m_saved_history;
    m_hysteresis_state = m_saved_hysteresis_state;
    m_edge_detected = m_saved_edge_detected;
    m_temporal_time = m_saved_temporal_time;
    m_last_output = m_saved_last_output;

    if (m_input_node)
        m_input_node->restore_state();

    m_state_saved = false;
}

}
