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

double FIR::process_sample(double input)
{
    if (is_bypass_enabled()) {
        return input;
    }

    double processed_input = input;
    if (m_input_node) {
        if (!m_input_node->is_processed()) {
            processed_input = m_input_node->process_sample(input);
            m_input_node->mark_processed(true);
        } else {
            processed_input = m_input_node->get_last_output();
        }
    }

    update_inputs(input);

    double output = 0.0;
    const size_t num_taps = std::min(m_coef_b.size(), m_input_history.size());

    for (size_t i = 0; i < num_taps; ++i) {
        output += m_coef_b[i] * m_input_history[i];
    }

    update_outputs(output);

    notify_tick(output);

    return output * get_gain();
}

void FIR::reset_processed_state()
{
    mark_processed(false);
    if (m_input_node) {
        m_input_node->reset_processed_state();
    }
}

}
