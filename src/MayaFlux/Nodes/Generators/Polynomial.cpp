#include "Polynomial.hpp"

namespace MayaFlux::Nodes::Generator {

Polynomial::Polynomial(const std::vector<double>& coefficients)
    : m_mode(PolynomialMode::DIRECT)
    , m_coefficients(coefficients)
    , m_buffer_size(0)
    , m_last_output(0.0)
{
}

Polynomial::Polynomial(DirectFunction function)
    : m_mode(PolynomialMode::DIRECT)
    , m_direct_function(function)
    , m_buffer_size(0)
    , m_last_output(0.0)
{
}

Polynomial::Polynomial(BufferFunction function, PolynomialMode mode, size_t buffer_size)
    : m_mode(mode)
    , m_buffer_function(function)
    , m_buffer_size(buffer_size)
    , m_last_output(0.0)
{
    m_input_buffer.resize(buffer_size, 0.0);
    m_output_buffer.resize(buffer_size, 0.0);
}

double Polynomial::process_sample(double input)
{
    double result = 0.0;

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

    m_last_output = result;
    notify_tick(result);
    return result;
}

std::vector<double> Polynomial::processFull(unsigned int num_samples)
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

void Polynomial::printGraph()
{
    if (m_mode == PolynomialMode::DIRECT) {
        const int width = 60;
        const int height = 20;
        const double x_min = -1.0;
        const double x_max = 1.0;
        const double y_min = -1.0;
        const double y_max = 1.0;

        std::vector<std::vector<char>> graph(height, std::vector<char>(width, ' '));

        int x_axis = static_cast<int>((0 - y_min) / (y_max - y_min) * height);
        x_axis = std::min(std::max(x_axis, 0), height - 1);
        std::fill(graph[x_axis].begin(), graph[x_axis].end(), '-');

        int y_axis = static_cast<int>((0 - x_min) / (x_max - x_min) * width);
        y_axis = std::min(std::max(y_axis, 0), width - 1);
        for (int i = 0; i < height; ++i) {
            graph[i][y_axis] = '|';
        }

        for (int i = 0; i < width; ++i) {
            double x = x_min + (x_max - x_min) * i / (width - 1);
            double y = m_direct_function(x);

            y = std::min(std::max(y, y_min), y_max);

            int graph_y = static_cast<int>((y_max - y) / (y_max - y_min) * height);
            graph_y = std::min(std::max(graph_y, 0), height - 1);

            graph[graph_y][i] = '*';
        }

        std::cout << "Polynomial Function Graph:" << std::endl;
        for (const auto& row : graph) {
            for (char c : row) {
                std::cout << c;
            }
            std::cout << std::endl;
        }

        std::cout << "x range: [" << x_min << ", " << x_max << "]" << std::endl;
        std::cout << "y range: [" << y_min << ", " << y_max << "]" << std::endl;
    } else {
        std::cout << "Graph visualization is only available for direct mode." << std::endl;
    }
}

void Polynomial::printCurrent()
{
    std::cout << "Polynomial Generator State:" << std::endl;
    std::cout << "  Mode: ";

    switch (m_mode) {
    case PolynomialMode::DIRECT:
        std::cout << "Direct" << std::endl;
        break;
    case PolynomialMode::RECURSIVE:
        std::cout << "Recursive" << std::endl;
        break;
    case PolynomialMode::FEEDFORWARD:
        std::cout << "Feedforward" << std::endl;
        break;
    }

    std::cout << "  Last output: " << m_last_output << std::endl;

    if (!m_coefficients.empty()) {
        std::cout << "  Coefficients: ";
        for (size_t i = 0; i < m_coefficients.size(); ++i) {
            if (i > 0) {
                std::cout << " + ";
            }
            std::cout << m_coefficients[i] << "x^" << (m_coefficients.size() - i - 1);
        }
        std::cout << std::endl;
    }

    if (m_mode != PolynomialMode::DIRECT) {
        std::cout << "  Buffer size: " << m_buffer_size << std::endl;

        std::cout << "  Input buffer: ";
        for (const auto& val : m_input_buffer) {
            std::cout << val << " ";
        }
        std::cout << std::endl;

        std::cout << "  Output buffer: ";
        for (const auto& val : m_output_buffer) {
            std::cout << val << " ";
        }
        std::cout << std::endl;
    }
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

void Polynomial::reset_processed_state()
{
    mark_processed(false);
}

} // namespace MayaFlux::Nodes::Generator
