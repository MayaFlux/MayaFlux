#include "Filter.hpp"

namespace MayaFlux::Nodes::Filters {

std::pair<int, int> shift_parser(const std::string& str)
{
    size_t underscore = str.find('_');
    if (underscore == std::string::npos) {
        throw std::invalid_argument("Invalid format. Supply numberical format of nInputs_nOutpits like 25_2");
    }

    int inputs = std::stoi(str.substr(0, underscore));
    int outputs = std::stoi(str.substr(underscore + 1));
    return std::make_pair(inputs, outputs);
}

Filter::Filter(std::shared_ptr<Node> input, const std::string& zindex_shifts)
    : m_input_node(input)
{
    m_shift_config = shift_parser(zindex_shifts);
    initialize_shift_buffers();
}

Filter::Filter(std::shared_ptr<Node> input, std::vector<double> a_coef, std::vector<double> b_coef)
    : m_input_node(input)
    , m_coef_a(a_coef)
    , m_coef_b(b_coef)
{
    m_shift_config = shift_parser(std::to_string(b_coef.size() - 1) + "_" + std::to_string(a_coef.size() - 1));

    if (m_coef_a.empty() || m_coef_b.empty()) {
        throw std::invalid_argument("IIR coefficients cannot be empty");
    }
    if (m_coef_a[0] == 0.0f) {
        throw std::invalid_argument("First denominator coefficient (a[0]) cannot be zero");
    }

    initialize_shift_buffers();
}

void Filter::initialize_shift_buffers()
{
    m_input_history.resize(m_shift_config.first + 1, 0.0f);
    m_output_history.resize(m_shift_config.second + 1, 0.0f);
}

void Filter::set_coefs(const std::vector<double>& new_coefs, coefficients type)
{
    if (type == coefficients::OUTPUT) {
        setACoefficients(new_coefs);
    } else if (type == coefficients::INPUT) {
        setBCoefficients(new_coefs);
    } else {
        setACoefficients(new_coefs);
        setBCoefficients(new_coefs);
    }
}

void Filter::update_inputs(double current_sample)
{
    for (unsigned int i = m_input_history.size() - 1; i > 0; i--) {
        m_input_history[i] = m_input_history[i - 1];
    }
    m_input_history[0] = current_sample;
}

void Filter::update_outputs(double current_sample)
{
    m_output_history[0] = current_sample;
    for (unsigned int i = m_output_history.size() - 1; i > 0; i--) {
        m_output_history[i] = m_output_history[i - 1];
    }
}

void Filter::setACoefficients(const std::vector<double>& new_coefs)
{
    if (new_coefs.empty()) {
        throw std::invalid_argument("Denominator coefficients cannot be empty");
    }
    if (new_coefs[0] == 0.0f) {
        throw std::invalid_argument("First denominator coefficient (a[0]) cannot be zero");
    }

    m_coef_a = new_coefs;

    if (static_cast<int>(m_coef_a.size()) - 1 != m_shift_config.second) {
        m_shift_config.second = static_cast<int>(m_coef_a.size()) - 1;
        initialize_shift_buffers();
    }
}

void Filter::setBCoefficients(const std::vector<double>& new_coefs)
{
    if (new_coefs.empty()) {
        throw std::invalid_argument("Numerator coefficients cannot be empty");
    }

    m_coef_b = new_coefs;

    if (static_cast<int>(m_coef_b.size()) - 1 != m_shift_config.first) {
        m_shift_config.first = static_cast<int>(m_coef_b.size()) - 1;
        initialize_shift_buffers();
    }
}

void Filter::update_coefs_from_node(int length, std::shared_ptr<Node> source, coefficients type)
{
    std::vector<double> samples = source->process_batch(length);
    set_coefs(samples, type);
}

void Filter::update_coef_from_input(int length, coefficients type)
{
    if (m_input_node) {
        std::vector<double> samples = m_input_node->process_batch(length);
        set_coefs(samples, type);
    } else {
        std::cerr << "No input node set for Filter. Use Filter::setInputNode() to set an input node.\n Alternatively, use Filter::updateCoefficientsFromNode() to specify a different source node.\n";
    }
}

void Filter::add_coef_internal(u_int64_t index, double value, std::vector<double>& buffer)
{
    if (index > buffer.size()) {
        buffer.resize(index + 1, 1.f);
    }
    buffer.at(index) = value;
}

void Filter::add_coef(int index, double value, coefficients type)
{
    switch (type) {
    case coefficients::INPUT:
        add_coef_internal(index, value, m_coef_a);
        break;
    case coefficients::OUTPUT:
        add_coef_internal(index, value, m_coef_b);
        break;
    default:
        add_coef_internal(index, value, m_coef_a);
        add_coef_internal(index, value, m_coef_b);
        break;
    }
}

void Filter::reset()
{
    std::fill(m_input_history.begin(), m_input_history.end(), 0.0);
    std::fill(m_output_history.begin(), m_output_history.end(), 0.0);
}

void Filter::normalize_coefficients(coefficients type)
{
    if (type == coefficients::OUTPUT || type == coefficients::ALL) {
        if (!m_coef_a.empty() && m_coef_a[0] != 0.0) {
            double a0 = m_coef_a[0];
            for (auto& coef : m_coef_a) {
                coef /= a0;
            }
        }
    }

    if (type == coefficients::INPUT || type == coefficients::ALL) {
        if (!m_coef_b.empty()) {
            double max_coef = 0.0;
            for (const auto& coef : m_coef_b) {
                max_coef = std::max(max_coef, std::abs(coef));
            }

            if (max_coef > 0.0) {
                for (auto& coef : m_coef_b) {
                    coef /= max_coef;
                }
            }
        }
    }
}

std::complex<double> Filter::get_frequency_response(double frequency, double sample_rate) const
{
    double omega = 2.0 * M_PI * frequency / sample_rate;
    std::complex<double> z = std::exp(std::complex<double>(0, omega));

    std::complex<double> numerator = 0.0;
    for (size_t i = 0; i < m_coef_b.size(); ++i) {
        numerator += m_coef_b[i] * std::pow(z, -static_cast<int>(i));
    }

    std::complex<double> denominator = 0.0;
    for (size_t i = 0; i < m_coef_a.size(); ++i) {
        denominator += m_coef_a[i] * std::pow(z, -static_cast<int>(i));
    }

    return numerator / denominator;
}

double Filter::get_magnitude_response(double frequency, double sample_rate) const
{
    return std::abs(get_frequency_response(frequency, sample_rate));
}

double Filter::get_phase_response(double frequency, double sample_rate) const
{
    return std::arg(get_frequency_response(frequency, sample_rate));
}

std::vector<double> Filter::process_batch(unsigned int num_samples)
{
    std::vector<double> output(num_samples);
    for (unsigned int i = 0; i < num_samples; ++i) {
        output[i] = process_sample(0.0); // The input value doesn't matter since we have an input node
    }
    return output;
}

std::unique_ptr<NodeContext> Filter::create_context(double value)
{
    return std::make_unique<FilterContext>(value, m_input_history, m_output_history, m_coef_a, m_coef_b);
}

void Filter::notify_tick(double value)
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

}
