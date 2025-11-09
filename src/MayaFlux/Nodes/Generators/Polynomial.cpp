#include "Polynomial.hpp"

#include <ranges>

namespace MayaFlux::Nodes::Generator {

Polynomial::Polynomial(const std::vector<double>& coefficients)
    : m_mode(PolynomialMode::DIRECT)
    , m_coefficients(coefficients)
    , m_buffer_size(0)
    , m_scale_factor(1.F)
{
    m_direct_function = create_polynomial_function(coefficients);
}

Polynomial::Polynomial(DirectFunction function)
    : m_mode(PolynomialMode::DIRECT)
    , m_direct_function(std::move(function))
    , m_buffer_size(0)
    , m_scale_factor(1.F)
{
}

Polynomial::Polynomial(BufferFunction function, PolynomialMode mode, size_t buffer_size)
    : m_mode(mode)
    , m_buffer_function(std::move(function))
    , m_buffer_size(buffer_size)
    , m_scale_factor(1.F)
{
    m_input_buffer.resize(buffer_size, 0.0);
    m_output_buffer.resize(buffer_size, 0.0);
}

std::deque<double> Polynomial::build_processing_buffer(double input, bool use_output_buffer)
{
    std::deque<double> buffer;

    if (m_use_external_context && !m_external_buffer_context.empty()) {
        size_t lookback = std::min(m_current_buffer_position, m_buffer_size - 1);

        if (lookback > 0) {
            auto start = m_external_buffer_context.begin() + (m_current_buffer_position - lookback);
            buffer.assign(start, m_external_buffer_context.begin() + m_current_buffer_position);
        }

        m_current_buffer_position++;
    } else {
        auto& internal_buffer = use_output_buffer ? m_output_buffer : m_input_buffer;
        buffer = internal_buffer;
    }

    buffer.push_front(input);
    if (buffer.size() > m_buffer_size) {
        buffer.resize(m_buffer_size);
    }

    return buffer;
}

double Polynomial::process_sample(double input)
{
    double result = 0.0;

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

    switch (m_mode) {
    case PolynomialMode::DIRECT:
        result = m_direct_function(input);
        break;

    case PolynomialMode::RECURSIVE:
        if (m_buffer_size > 0) {
            std::deque<double> combined_buffer = build_processing_buffer(input, true);
            result = m_buffer_function(combined_buffer);

            if (!m_use_external_context) {
                m_input_buffer.push_front(input);
                if (m_output_buffer.size() >= m_buffer_size) {
                    m_output_buffer.pop_back();
                }
            }

            m_output_buffer.push_front(result);
            if (m_input_buffer.size() > m_buffer_size) {
                m_input_buffer.pop_back();
            }
        }
        break;

    case PolynomialMode::FEEDFORWARD:
        if (m_buffer_size > 0) {
            std::deque<double> input_buffer = build_processing_buffer(input, false);
            result = m_buffer_function(input_buffer);

            if (!m_use_external_context) {
                m_input_buffer.push_front(input);
                if (m_input_buffer.size() > m_buffer_size) {
                    m_input_buffer.pop_back();
                }
            }

            m_output_buffer.push_front(result);
            if (m_output_buffer.size() > m_buffer_size) {
                m_output_buffer.pop_back();
            }
        }
        break;
    }

    result *= m_scale_factor;

    m_last_output = result;

    if (!m_state_saved || (m_state_saved && m_fire_events_during_snapshot))
        notify_tick(result);

    if (m_input_node) {
        atomic_dec_modulator_count(m_input_node->m_modulator_count, 1);
        try_reset_processed_state(m_input_node);
    }

    return result;
}

std::vector<double> Polynomial::process_batch(unsigned int num_samples)
{
    std::vector<double> buffer(num_samples);

    reset();

    for (size_t i = 0; i < num_samples; ++i) {
        buffer[i] = process_sample(0.0);
    }

    return buffer;
}

void Polynomial::reset()
{
    m_input_buffer.clear();
    m_output_buffer.clear();

    m_input_buffer.resize(m_buffer_size, 0.0);
    m_output_buffer.resize(m_buffer_size, 0.0);

    m_last_output = 0.0;
}

void Polynomial::set_coefficients(const std::vector<double>& coefficients)
{
    m_coefficients = coefficients;
    // m_direct_function = create_polynomial_function(coefficients);
}

void Polynomial::set_direct_function(DirectFunction function)
{
    m_direct_function = std::move(function);
    m_mode = PolynomialMode::DIRECT;
}

void Polynomial::set_buffer_function(BufferFunction function, PolynomialMode mode, size_t buffer_size)
{
    m_buffer_function = std::move(function);
    m_mode = mode;

    if (buffer_size != m_buffer_size) {
        m_buffer_size = buffer_size;
        m_input_buffer.resize(buffer_size, 0.0);
        m_output_buffer.resize(buffer_size, 0.0);
    }
}

void Polynomial::set_initial_conditions(const std::vector<double>& initial_values)
{
    m_output_buffer.clear();

    for (const auto& value : initial_values) {
        m_output_buffer.push_back(value);
    }

    if (m_output_buffer.size() < m_buffer_size) {
        m_output_buffer.resize(m_buffer_size, 0.0);
    } else if (m_output_buffer.size() > m_buffer_size) {
        m_output_buffer.resize(m_buffer_size);
    }
}

Polynomial::DirectFunction Polynomial::create_polynomial_function(const std::vector<double>& coefficients)
{
    return [coefficients](double x) {
        double result = 0.0;
        double x_power = 1.0;

        for (double coefficient : std::ranges::reverse_view(coefficients)) {
            result += coefficient * x_power;
            x_power *= x;
        }

        return result;
    };
}

std::unique_ptr<NodeContext> Polynomial::create_context(double value)
{
    if (m_gpu_compatible) {
        return std::make_unique<PolynomialContextGpu>(
            value,
            m_mode,
            m_buffer_size,
            m_input_buffer,
            m_output_buffer,
            m_coefficients,
            get_gpu_data_buffer());
    }

    return std::make_unique<PolynomialContext>(value, m_mode, m_buffer_size, m_input_buffer, m_output_buffer, m_coefficients);
}

void Polynomial::notify_tick(double value)
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
}

void Polynomial::save_state()
{
    m_saved_input_buffer = m_input_buffer;
    m_saved_output_buffer = m_output_buffer;
    m_saved_last_output = m_last_output;

    if (m_input_node)
        m_input_node->save_state();

    m_state_saved = true;
}

void Polynomial::restore_state()
{
    m_input_buffer = m_saved_input_buffer;
    m_output_buffer = m_saved_output_buffer;
    m_last_output = m_saved_last_output;

    if (m_input_node)
        m_input_node->restore_state();

    m_state_saved = false;
}

} // namespace MayaFlux::Nodes::Generator
