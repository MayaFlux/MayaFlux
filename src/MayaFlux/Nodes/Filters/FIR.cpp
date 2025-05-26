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
        u_int32_t state = m_input_node->m_state.load();
        if (state & Utils::NodeState::PROCESSED) {
            processed_input = m_input_node->get_last_output();
        } else {
            processed_input = m_input_node->process_sample(input);
            atomic_add_flag(m_input_node->m_state, Utils::NodeState::PROCESSED);
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
    atomic_remove_flag(m_state, Utils::NodeState::PROCESSED);
    if (m_input_node) {
        m_input_node->reset_processed_state();
    }
}

}
