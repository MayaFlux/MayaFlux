#include "IIR.hpp"

namespace MayaFlux::Nodes::Filters {

IIR::IIR(std::shared_ptr<Node> input, const std::string& zindex_shifts)
    : Filter(input, zindex_shifts)
{
}

IIR::IIR(std::shared_ptr<Node> input, std::vector<double> a_coef, std::vector<double> b_coef)
    : Filter(input, a_coef, b_coef)
{
}

double IIR::process_sample(double input)
{
    if (is_bypass_enabled()) {
        return input;
    }

    if (inputNode) {
        input += inputNode->process_sample(input);
        input *= 0.5f;
    }
    update_inputs(input);

    double output = 0.f;
    const size_t num_feedforward = std::min(coef_b.size(), input_history.size());
    for (size_t i = 0; i < num_feedforward; ++i) {
        output += coef_b[i] * input_history[i];
    }

    const size_t num_feedback = std::min(coef_a.size(), output_history.size());
    for (size_t i = 1; i < num_feedback; ++i) {
        output -= coef_a[i] * output_history[i];
    }

    output /= coef_a[0];

    update_outputs(output);

    return output * get_gain();
}

}
