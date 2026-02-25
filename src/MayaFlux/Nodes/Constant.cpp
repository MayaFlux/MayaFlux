#include "Constant.hpp"

namespace MayaFlux::Nodes {

Constant::Constant(double value)
    : m_value(value)
{
    m_last_output = value;
}

//-----------------------------------------------------------------------------
// Core processing
//-----------------------------------------------------------------------------

double Constant::process_sample(double /*input*/)
{
    m_last_output = m_value;

    if ((!m_state_saved || m_fire_events_during_snapshot) && !m_networked_node) {
        notify_tick(m_value);
    }

    return m_value;
}

std::vector<double> Constant::process_batch(unsigned int num_samples)
{
    std::vector<double> out(num_samples);
    for (unsigned int i = 0; i < num_samples; ++i) {
        out[i] = process_sample(0.0);
    }
    return out;
}

//-----------------------------------------------------------------------------
// Parameter control
//-----------------------------------------------------------------------------

void Constant::set_constant(double value)
{
    m_value = value;
    m_last_output = value;
}

//-----------------------------------------------------------------------------
// State snapshot
//-----------------------------------------------------------------------------

void Constant::save_state()
{
    m_saved_value = m_value;
    m_state_saved = true;
}

void Constant::restore_state()
{
    m_value = m_saved_value;
    m_last_output = m_saved_value;
    m_state_saved = false;
}

//-----------------------------------------------------------------------------
// Context / callbacks
//-----------------------------------------------------------------------------

void Constant::update_context(double value)
{
    m_context.value = value;
}

NodeContext& Constant::get_last_context()
{
    return m_context;
}

void Constant::notify_tick(double value)
{
    update_context(value);

    for (auto& cb : m_callbacks) {
        cb(m_context);
    }
    for (auto& [cb, cond] : m_conditional_callbacks) {
        if (cond(m_context)) {
            cb(m_context);
        }
    }
}

} // namespace MayaFlux::Nodes
