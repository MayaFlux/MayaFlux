#include "NodeStructure.hpp"
#include "MayaFlux/API/Config.hpp"
#include "MayaFlux/API/Graph.hpp"

namespace MayaFlux::Nodes {

ChainNode::ChainNode(std::shared_ptr<Node> source, std::shared_ptr<Node> target)
    : m_Source(source)
    , m_Target(target)
    , m_is_initialized(false)
{
}

void ChainNode::initialize()
{
    if (!m_is_initialized) {
        auto self = shared_from_this();

        if (m_Target) {
            for (auto& channel : get_active_channels(m_Target, 0)) {
                MayaFlux::register_audio_node(self, channel);
            }
        } else {
            MayaFlux::register_audio_node(self, 0);
        }
        m_is_initialized = true;
    }

    auto semantics = MayaFlux::Config::get_graph_config().chain_semantics;
    switch (semantics) {
    case Utils::NodeChainSemantics::REPLACE_TARGET:
        if (m_Target) {
            for (auto& channel : get_active_channels(m_Target, 0)) {
                MayaFlux::unregister_audio_node(m_Target, channel);
            }
        }
        break;
    case Utils::NodeChainSemantics::ONLY_CHAIN:
        if (m_Source) {
            for (auto& channel : get_active_channels(m_Source, 0)) {
                MayaFlux::unregister_audio_node(m_Source, channel);
            }
        }
        if (m_Target) {
            for (auto& channel : get_active_channels(m_Target, 0)) {
                MayaFlux::unregister_audio_node(m_Target, channel);
            }
        }
        break;
    case Utils::NodeChainSemantics::PRESERVE_BOTH:
    default:
        break;
    }
}

double ChainNode::process_sample(double input)
{
    if (!m_Source || !m_Target) {
        return input;
    }
    if (!is_initialized())
        initialize();

    atomic_inc_modulator_count(m_Source->m_modulator_count, 1);
    atomic_inc_modulator_count(m_Target->m_modulator_count, 1);

    m_last_output = input;

    u_int32_t sstate = m_Source->m_state.load();
    if (sstate & Utils::NodeState::PROCESSED) {
        m_last_output = input + m_Source->get_last_output();
    } else {
        m_last_output = m_Source->process_sample(input);
        atomic_add_flag(m_Source->m_state, Utils::NodeState::PROCESSED);
    }

    u_int32_t tstate = m_Target->m_state.load();
    if (tstate & Utils::NodeState::PROCESSED) {
        m_last_output += m_Target->get_last_output();
    } else {
        m_last_output = m_Target->process_sample(m_last_output);
        tstate = tstate | Utils::NodeState::PROCESSED;
        atomic_add_flag(m_Target->m_state, Utils::NodeState::PROCESSED);
    }

    atomic_dec_modulator_count(m_Source->m_modulator_count, 1);
    atomic_dec_modulator_count(m_Target->m_modulator_count, 1);

    try_reset_processed_state(m_Source);
    try_reset_processed_state(m_Target);

    return m_last_output;
}

std::vector<double> ChainNode::process_batch(unsigned int num_samples)
{
    std::vector<double> output(num_samples);
    for (size_t i = 0; i < num_samples; i++) {
        output[i] = process_sample(0.F);
    }
    return output;
}

void ChainNode::reset_processed_state()
{
    atomic_remove_flag(m_state, Utils::NodeState::PROCESSED);
    if (m_Source)
        m_Source->reset_processed_state();
    if (m_Target)
        m_Target->reset_processed_state();
}

BinaryOpNode::BinaryOpNode(std::shared_ptr<Node> lhs, std::shared_ptr<Node> rhs, CombineFunc func)
    : m_lhs(lhs)
    , m_rhs(rhs)
    , m_func(func)
    , m_is_initialized(false)
{
}

void BinaryOpNode::initialize()
{
    if (!m_is_initialized) {
        auto self = shared_from_this();
        u_int32_t lhs_mask = m_lhs ? m_lhs->get_channel_mask().load() : 0;
        u_int32_t rhs_mask = m_rhs ? m_rhs->get_channel_mask().load() : 0;
        u_int32_t combined_mask = lhs_mask | rhs_mask;

        if (combined_mask != 0) {
            for (auto& channel : get_active_channels(combined_mask, 0)) {
                MayaFlux::register_audio_node(self, channel);
            }
        } else {
            MayaFlux::register_audio_node(self, 0);
        }
        m_is_initialized = true;
    }

    auto semantics = MayaFlux::Config::get_graph_config().binary_op_semantics;
    switch (semantics) {
    case Utils::NodeBinaryOpSemantics::REPLACE:
        if (m_lhs) {
            for (auto& channel : get_active_channels(m_lhs, 0)) {
                MayaFlux::unregister_audio_node(m_lhs, channel);
            }
        }
        if (m_rhs) {
            for (auto& channel : get_active_channels(m_rhs, 0)) {
                MayaFlux::unregister_audio_node(m_rhs, channel);
            }
        }
        break;
    case Utils::NodeBinaryOpSemantics::KEEP:
    default:
        break;
    }
}

double BinaryOpNode::process_sample(double input)
{
    if (!m_lhs || !m_rhs) {
        return input;
    }

    if (!is_initialized())
        initialize();

    atomic_inc_modulator_count(m_lhs->m_modulator_count, 1);
    atomic_inc_modulator_count(m_rhs->m_modulator_count, 1);

    u_int32_t lstate = m_lhs->m_state.load();
    if (lstate & Utils::NodeState::PROCESSED) {
        m_last_lhs_value = input + m_lhs->get_last_output();
    } else {
        m_last_lhs_value = m_lhs->process_sample(input);
        atomic_add_flag(m_lhs->m_state, Utils::NodeState::PROCESSED);
    }

    u_int32_t rstate = m_rhs->m_state.load();
    if (rstate & Utils::NodeState::PROCESSED) {
        m_last_rhs_value = input + m_rhs->get_last_output();
    } else {
        m_last_rhs_value = m_rhs->process_sample(input);
        atomic_add_flag(m_rhs->m_state, Utils::NodeState::PROCESSED);
    }

    m_last_output = m_func(m_last_lhs_value, m_last_rhs_value);

    notify_tick(m_last_output);

    atomic_dec_modulator_count(m_lhs->m_modulator_count, 1);
    atomic_dec_modulator_count(m_rhs->m_modulator_count, 1);

    try_reset_processed_state(m_lhs);
    try_reset_processed_state(m_rhs);

    return m_last_output;
}

std::vector<double> BinaryOpNode::process_batch(unsigned int num_samples)
{
    std::vector<double> output(num_samples);
    for (unsigned int i = 0; i < num_samples; ++i) {
        output[i] = process_sample(0.0);
    }
    return output;
}

void BinaryOpNode::notify_tick(double value)
{
    auto context = create_context(value);
    for (const auto& callback : m_callbacks) {
        callback(*context);
    }

    for (const auto& [callback, condition] : m_conditional_callbacks) {
        if (condition(*context)) {
            callback(*context);
        }
    }
}

void BinaryOpNode::reset_processed_state()
{
    atomic_remove_flag(m_state, Utils::NodeState::PROCESSED);
    if (m_lhs)
        m_lhs->reset_processed_state();
    if (m_rhs)
        m_rhs->reset_processed_state();
}
}
