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

    double processed_input = input;
    if (inputNode) {
        if (!inputNode->is_processed()) {
            processed_input = inputNode->process_sample(input);
            inputNode->mark_processed(true);
        } else {
            processed_input = inputNode->get_last_output();
        }
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

    notify_tick(output);

    return output * get_gain();
}

void IIR::reset_processed_state()
{
    mark_processed(false);
    if (inputNode) {
        inputNode->reset_processed_state();
    }
}

}
