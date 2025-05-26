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
    if (m_input_node) {
        atomic_inc_modulator_count(m_input_node->m_modulator_count, 1);
        u_int32_t state = m_input_node->m_state.load();
        if (state & Utils::NodeState::PROCESSED) {
            processed_input = m_input_node->get_last_output();
        } else {
            processed_input = m_input_node->process_sample(input);
            atomic_add_flag(m_input_node->m_state, Utils::NodeState::PROCESSED);
        }
    }
    update_inputs(input);

    double output = 0.f;
    const size_t num_feedforward = std::min(m_coef_b.size(), m_input_history.size());
    for (size_t i = 0; i < num_feedforward; ++i) {
        output += m_coef_b[i] * m_input_history[i];
    }

    const size_t num_feedback = std::min(m_coef_a.size(), m_output_history.size());
    for (size_t i = 1; i < num_feedback; ++i) {
        output -= m_coef_a[i] * m_output_history[i];
    }

    // output /= m_coef_a[0];

    update_outputs(output);

    notify_tick(output);

    if (m_input_node) {
        atomic_dec_modulator_count(m_input_node->m_modulator_count, 1);
        try_reset_processed_state(m_input_node);
    }

    return output * get_gain();
}

void IIR::reset_processed_state()
{
    atomic_remove_flag(m_state, Utils::NodeState::PROCESSED);
    if (m_input_node) {
        m_input_node->reset_processed_state();
    }
}

}
