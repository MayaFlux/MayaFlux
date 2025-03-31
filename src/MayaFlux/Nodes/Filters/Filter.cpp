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
    : inputNode(input)
{
    shift_config = shift_parser(zindex_shifts);
    initialize_shift_buffers();
}

Filter::Filter(std::shared_ptr<Node> input, std::vector<double> a_coef, std::vector<double> b_coef)
    : inputNode(input)
    , coef_a(a_coef)
    , coef_b(b_coef)
{
    shift_config = shift_parser(std::to_string(b_coef.size() - 1) + "_" + std::to_string(a_coef.size() - 1));

    if (coef_a.empty() || coef_b.empty()) {
        throw std::invalid_argument("IIR coefficients cannot be empty");
    }
    if (coef_a[0] == 0.0f) {
        throw std::invalid_argument("First denominator coefficient (a[0]) cannot be zero");
    }
}

void Filter::initialize_shift_buffers()
{
    input_history.resize(shift_config.first + 1, 0.0f);
    output_history.resize(shift_config.second + 1, 0.0f);
}

void Filter::setCoefficients(const std::vector<double>& new_coefs, Utils::coefficients type)
{
    if (type == Utils::coefficients::OUTPUT) {
        setACoefficients(new_coefs);
    } else if (type == Utils::coefficients::INPUT) {
        setBCoefficients(new_coefs);
    } else {
        setACoefficients(new_coefs);
        setBCoefficients(new_coefs);
    }
}

void Filter::update_inputs(double current_sample)
{
    input_history[0] = current_sample;
    for (unsigned int i = input_history.size() - 1; i > 0; i--) {
        input_history[i] = input_history[i - 1];
    }
}

void Filter::update_outputs(double current_sample)
{
    output_history[0] = current_sample;
    for (unsigned int i = output_history.size() - 1; i > 0; i--) {
        output_history[i] = output_history[i - 1];
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

    coef_a = new_coefs;

    if (static_cast<int>(coef_a.size()) - 1 != shift_config.second) {
        shift_config.second = static_cast<int>(coef_a.size()) - 1;
        initialize_shift_buffers();
    }
}

void Filter::setBCoefficients(const std::vector<double>& new_coefs)
{
    if (new_coefs.empty()) {
        throw std::invalid_argument("Numerator coefficients cannot be empty");
    }

    coef_b = new_coefs;

    if (static_cast<int>(coef_b.size()) - 1 != shift_config.first) {
        shift_config.first = static_cast<int>(coef_b.size()) - 1;
        initialize_shift_buffers();
    }
}

void Filter::updateCoefficientsFromNode(int length, std::shared_ptr<Node> source, Utils::coefficients type)
{
    std::vector<double> samples = source->processFull(length);
    setCoefficients(samples, type);
}

void Filter::updateCoefficientsFromInput(int length, Utils::coefficients type)
{
    if (inputNode) {
        std::vector<double> samples = inputNode->processFull(length);
        setCoefficients(samples, type);
    } else {
        std::cerr << "No input node set for Filter. Use Filter::setInputNode() to set an input node.\n Alternatively, use Filter::updateCoefficientsFromNode() to specify a different source node.\n";
    }
}

}
