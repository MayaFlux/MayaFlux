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
        atomic_inc_modulator_count(m_input_node->m_modulator_count, 1);
        uint32_t state = m_input_node->m_state.load();
        if (state & Utils::NodeState::PROCESSED) {
            processed_input += m_input_node->get_last_output();
        } else {
            processed_input += m_input_node->process_sample(input);
            atomic_add_flag(m_input_node->m_state, Utils::NodeState::PROCESSED);
        }
    }

    update_inputs(processed_input);

    double output = 0.0;
    const size_t num_taps = std::min(m_coef_b.size(), m_input_history.size());

    for (size_t i = 0; i < num_taps; ++i) {
        output += m_coef_b[i] * m_input_history[i];
    }

    update_outputs(output);

    if (!m_state_saved || (m_state_saved && m_fire_events_during_snapshot))
        notify_tick(output);

    if (m_input_node) {
        atomic_dec_modulator_count(m_input_node->m_modulator_count, 1);
        try_reset_processed_state(m_input_node);
    }
    return output * get_gain();
}

void FIR::save_state()
{
    m_saved_input_history = m_input_history;

    if (m_input_node)
        m_input_node->save_state();

    m_state_saved = true;
}

void FIR::restore_state()
{
    m_input_history = m_saved_input_history;

    if (m_input_node)
        m_input_node->restore_state();

    m_state_saved = false;
}

}
