#include "Polynomial.hpp"

namespace MayaFlux::Nodes::Generator {

Polynomial::Polynomial(const std::vector<double>& coefficients)
    : m_mode(PolynomialMode::DIRECT)
    , m_coefficients(coefficients)
    , m_buffer_size(0)
    , m_is_registered(false)
    , m_is_processed(false)
    , m_mock_process(false)
    , m_last_output(0.0)
    , m_scale_factor(1.f)
{
}

Polynomial::Polynomial(DirectFunction function)
    : m_mode(PolynomialMode::DIRECT)
    , m_direct_function(function)
    , m_buffer_size(0)
    , m_is_registered(false)
    , m_is_processed(false)
    , m_mock_process(false)
    , m_last_output(0.0)
    , m_scale_factor(1.f)
{
}

Polynomial::Polynomial(BufferFunction function, PolynomialMode mode, size_t buffer_size)
    : m_mode(mode)
    , m_buffer_function(function)
    , m_buffer_size(buffer_size)
    , m_is_registered(false)
    , m_is_processed(false)
    , m_mock_process(false)
    , m_last_output(0.0)
    , m_scale_factor(1.f)
{
    m_input_buffer.resize(buffer_size, 0.0);
    m_output_buffer.resize(buffer_size, 0.0);
}

double Polynomial::process_sample(double input)
{
    double result = 0.0;


    if (m_input_node) {

        auto state = m_input_node->GetState().load();

        if (! (state & Utils::MF_NodeState::MFOP_IS_PROCESSING) ) {

            input = m_input_node->process_sample(input);

            state = static_cast<Utils::MF_NodeState>( state | Utils::MF_NodeState::MFOP_IS_PROCESSING);
            AtomicSetFlagStrong(m_input_node->GetState(), state);

        } else {
            input += m_input_node->get_last_output();
        }
    }

    switch (m_mode) {
    case PolynomialMode::DIRECT:
        result = m_direct_function(input);
        break;

    case PolynomialMode::RECURSIVE:
        if (m_buffer_size > 0) {
            m_input_buffer.push_front(input);
            if (m_output_buffer.size() >= m_buffer_size) {
                m_output_buffer.pop_back();
            }

            std::deque<double> combined_buffer = m_output_buffer;
            combined_buffer.push_front(input);

            result = m_buffer_function(combined_buffer);
            m_output_buffer.push_front(result);

            if (m_input_buffer.size() > m_buffer_size) {
                m_input_buffer.pop_back();
            }
        }
        break;

    case PolynomialMode::FEEDFORWARD:
        if (m_buffer_size > 0) {
            m_input_buffer.push_front(input);
            if (m_input_buffer.size() > m_buffer_size) {
                m_input_buffer.pop_back();
            }

            result = m_buffer_function(m_input_buffer);

            m_output_buffer.push_front(result);
            if (m_output_buffer.size() > m_buffer_size) {
                m_output_buffer.pop_back();
            }
        }
        break;
    }

    result *= m_scale_factor;

    m_last_output = result;
    notify_tick(result);
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
    m_direct_function = function;
    m_mode = PolynomialMode::DIRECT;
}

void Polynomial::set_buffer_function(BufferFunction function, PolynomialMode mode, size_t buffer_size)
{
    m_buffer_function = function;
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

/* Polynomial::DirectFunction Polynomial::create_polynomial_function(const std::vector<double>& coefficients)
{
    return [coefficients](double x) {
        double result = 0.0;
        double x_power = 1.0;

        for (auto it = coefficients.rbegin(); it != coefficients.rend(); ++it) {
            result += (*it) * x_power;
            x_power *= x;
        }

        return result;
    };
} */

std::unique_ptr<NodeContext> Polynomial::create_context(double value)
{
    return std::make_unique<PolynomialContext>(value, m_mode, m_buffer_size, m_input_buffer, m_output_buffer, m_coefficients);
}

void Polynomial::notify_tick(double value)
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

void Polynomial::on_tick(NodeHook callback)
{
    safe_add_callback(m_callbacks, callback);
}

void Polynomial::on_tick_if(NodeHook callback, NodeCondition condition)
{
    safe_add_conditional_callback(m_conditional_callbacks, callback, condition);
}

bool Polynomial::remove_hook(const NodeHook& callback)
{
    return safe_remove_callback(m_callbacks, callback);
}

bool Polynomial::remove_conditional_hook(const NodeCondition& callback)
{
    return safe_remove_conditional_callback(m_conditional_callbacks, callback);
}

} // namespace MayaFlux::Nodes::Generator
