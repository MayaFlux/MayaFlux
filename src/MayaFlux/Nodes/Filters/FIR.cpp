#include "FIR.hpp"

namespace MayaFlux::Nodes::Filters {

FIR::FIR(std::shared_ptr<Node> input, const std::vector<double> coeffs)
    : Filter(input, std::vector<double> { 1.0f }, coeffs)
{
}

FIR::FIR(std::shared_ptr<Node> input, const std::string& zindex_shifts)
    : Filter(input, zindex_shifts)
{
}

double FIR::processSample(double input)
{
    if (inputNode) {
        input += inputNode->processSample(input);
        input *= 0.5f;
    }

    update_inputs(input);

    double output = 0.0;
    const size_t num_taps = std::min(coef_b.size(), input_history.size());

    for (size_t i = 0; i < num_taps; ++i) {
        output += coef_b[i] * input_history[i];
    }

    update_outputs(output);

    return output;
}

}
