#include "Polynomial.hpp"

namespace MayaFlux::Nodes::Generator {

Polynomial::Polynomial(const std::vector<double>& coefficients)
    : m_mode(PolynomialMode::DIRECT)
    , m_coefficients(coefficients)
    , m_scale_factor(1.F)
    , m_context(0.0, m_mode, m_buffer_size, {}, {}, m_coefficients)
    , m_context_gpu(0.0, m_mode, m_buffer_size, {}, {}, m_coefficients, get_gpu_data_buffer())
{
    m_direct_function = create_polynomial_function(coefficients);
}

Polynomial::Polynomial(DirectFunction function)
    : m_mode(PolynomialMode::DIRECT)
    , m_direct_function(std::move(function))
    , m_scale_factor(1.F)
    , m_context(0.0, m_mode, m_buffer_size, {}, {}, m_coefficients)
    , m_context_gpu(0.0, m_mode, m_buffer_size, {}, {}, m_coefficients, get_gpu_data_buffer())
{
}

Polynomial::Polynomial(BufferFunction function, PolynomialMode mode, size_t buffer_size)
    : m_mode(mode)
    , m_buffer_function(std::move(function))
    , m_ring_count(buffer_size)
    , m_buffer_size(buffer_size)
    , m_ring_data(buffer_size, 0.0)
    , m_linear_view(buffer_size, 0.0)
    , m_scale_factor(1.F)
    , m_context(0.0, m_mode, m_buffer_size, {}, {}, m_coefficients)
    , m_context_gpu(0.0, m_mode, m_buffer_size, {}, {}, m_coefficients, get_gpu_data_buffer())
{
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
            std::span<double> view;

            if (m_use_external_context && !m_external_buffer_context.empty()) {
                view = external_context_view(input);
            } else {
                ring_push(input);
                view = linearized_view();
            }

            result = m_buffer_function(view);

            m_ring_data[m_ring_head] = result;
        }
        break;

    case PolynomialMode::FEEDFORWARD:
        if (m_buffer_size > 0) {
            std::span<double> view;

            if (m_use_external_context && !m_external_buffer_context.empty()) {
                view = external_context_view(input);
            } else {
                ring_push(input);
                view = linearized_view();
            }

            result = m_buffer_function(view);
        }
        break;
    }

    result *= m_scale_factor;

    m_last_output = result;

    if ((!m_state_saved || (m_state_saved && m_fire_events_during_snapshot))
        && !m_networked_node) {
        notify_tick(result);
    }

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
    std::ranges::fill(m_ring_data, 0.0);
    m_ring_head = 0;
    m_ring_count = m_buffer_size;
    m_last_output = 0.0;
}

void Polynomial::set_coefficients(const std::vector<double>& coefficients)
{
    m_coefficients = coefficients;
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
        m_ring_data.resize(buffer_size, 0.0);
        m_linear_view.resize(buffer_size, 0.0);
        m_ring_head = 0;
        m_ring_count = 0;
    }
}

void Polynomial::set_initial_conditions(const std::vector<double>& initial_values)
{
    std::ranges::fill(m_ring_data, 0.0);

    size_t count = std::min(initial_values.size(), m_buffer_size);
    for (size_t i = 0; i < count; ++i) {
        m_ring_data[i] = initial_values[i];
    }

    m_ring_head = 0;
    m_ring_count = m_buffer_size;
}

void Polynomial::ring_push(double val)
{
    if (m_buffer_size == 0)
        return;
    m_ring_head = (m_ring_head == 0 ? m_buffer_size : m_ring_head) - 1;
    m_ring_data[m_ring_head] = val;
    if (m_ring_count < m_buffer_size)
        ++m_ring_count;
}

std::span<double> Polynomial::linearized_view()
{
    for (size_t i = 0; i < m_ring_count; ++i) {
        m_linear_view[i] = m_ring_data[(m_ring_head + i) % m_buffer_size];
    }
    return { m_linear_view.data(), m_ring_count };
}

std::span<double> Polynomial::external_context_view(double input)
{
    size_t lookback = std::min(m_current_buffer_position, m_buffer_size - 1);
    size_t view_size = std::min(lookback + 1, m_buffer_size);

    m_linear_view[0] = input;

    for (size_t i = 1; i < view_size && i <= lookback; ++i) {
        m_linear_view[i] = m_external_buffer_context[m_current_buffer_position - i];
    }

    m_current_buffer_position++;
    return { m_linear_view.data(), view_size };
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

void Polynomial::update_context(double value)
{
    auto view = linearized_view();

    if (m_gpu_compatible) {
        m_context_gpu.value = value;
        m_context_gpu.m_mode = m_mode;
        m_context_gpu.m_buffer_size = m_buffer_size;
        m_context_gpu.m_input_buffer = view;
        m_context_gpu.m_output_buffer = view;
    } else {
        m_context.value = value;
        m_context.m_mode = m_mode;
        m_context.m_buffer_size = m_buffer_size;
        m_context.m_input_buffer = view;
        m_context.m_output_buffer = view;
    }
}

void Polynomial::notify_tick(double value)
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

NodeContext& Polynomial::get_last_context()
{
    if (m_gpu_compatible) {
        return m_context_gpu;
    }
    return m_context;
}

void Polynomial::save_state()
{
    m_saved_ring_data = m_ring_data;
    m_saved_ring_head = m_ring_head;
    m_saved_ring_count = m_ring_count;
    m_saved_last_output = m_last_output;

    if (m_input_node)
        m_input_node->save_state();

    m_state_saved = true;
}

void Polynomial::restore_state()
{
    m_ring_data = m_saved_ring_data;
    m_ring_head = m_saved_ring_head;
    m_ring_count = m_saved_ring_count;
    m_last_output = m_saved_last_output;

    if (m_input_node)
        m_input_node->restore_state();

    m_state_saved = false;
}

} // namespace MayaFlux::Nodes::Generator
