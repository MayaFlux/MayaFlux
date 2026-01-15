#include "NodeStructure.hpp"
#include "MayaFlux/API/Config.hpp"
#include "MayaFlux/API/Graph.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Nodes {

ChainNode::ChainNode(const std::shared_ptr<Node>& source, const std::shared_ptr<Node>& target)
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

    uint32_t sstate = m_Source->m_state.load();
    if (sstate & Utils::NodeState::PROCESSED) {
        m_last_output = input + m_Source->get_last_output();
    } else {
        m_last_output = m_Source->process_sample(input);
        atomic_add_flag(m_Source->m_state, Utils::NodeState::PROCESSED);
    }

    uint32_t tstate = m_Target->m_state.load();
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

void ChainNode::save_state()
{
    if (m_Source)
        m_Source->save_state();
    if (m_Target)
        m_Target->save_state();

    m_state_saved = true;
}

void ChainNode::restore_state()
{
    if (m_Source)
        m_Source->restore_state();
    if (m_Target)
        m_Target->restore_state();

    m_state_saved = false;
}

bool ChainNode::is_initialized() const
{
    auto sState = m_Source->m_state.load();
    auto tState = m_Target->m_state.load();

    bool is_source_registered = m_Source ? (sState & Utils::NodeState::ACTIVE) : false;
    bool is_target_registered = m_Target ? (tState & Utils::NodeState::ACTIVE) : false;
    return !is_source_registered && !is_target_registered && (m_state.load() & Utils::NodeState::ACTIVE);
}

NodeContext& ChainNode::get_last_context()
{
    if (!m_Target) {
        error<std::runtime_error>(
            Journal::Component::Nodes, Journal::Context::Runtime,
            std::source_location::current(),
            "ChainNode target node is null when retrieving last context");
    }
    return m_Target->get_last_context();
}

BinaryOpContext::BinaryOpContext(double value, double lhs_value, double rhs_value)
    : NodeContext(value, typeid(BinaryOpContext).name())
    , lhs_value(lhs_value)
    , rhs_value(rhs_value)
{
}

BinaryOpContextGpu::BinaryOpContextGpu(double value, double lhs_value, double rhs_value, std::span<const float> gpu_data)
    : BinaryOpContext(value, lhs_value, rhs_value)
    , GpuVectorData(gpu_data)
{
    type_id = typeid(BinaryOpContextGpu).name();
}

BinaryOpNode::BinaryOpNode(const std::shared_ptr<Node>& lhs, const std::shared_ptr<Node>& rhs, CombineFunc func)
    : m_lhs(lhs)
    , m_rhs(rhs)
    , m_func(std::move(func))
    , m_context(0.0, 0.0, 0.0)
    , m_context_gpu(0.0, 0.0, 0.0, get_gpu_data_buffer())
{
}

void BinaryOpNode::initialize()
{
    if (!m_is_initialized) {
        auto self = shared_from_this();
        uint32_t lhs_mask = m_lhs ? m_lhs->get_channel_mask().load() : 0;
        uint32_t rhs_mask = m_rhs ? m_rhs->get_channel_mask().load() : 0;
        uint32_t combined_mask = lhs_mask | rhs_mask;

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

    uint32_t lstate = m_lhs->m_state.load();
    if (lstate & Utils::NodeState::PROCESSED) {
        m_last_lhs_value = input + m_lhs->get_last_output();
    } else {
        m_last_lhs_value = m_lhs->process_sample(input);
        atomic_add_flag(m_lhs->m_state, Utils::NodeState::PROCESSED);
    }

    uint32_t rstate = m_rhs->m_state.load();
    if (rstate & Utils::NodeState::PROCESSED) {
        m_last_rhs_value = input + m_rhs->get_last_output();
    } else {
        m_last_rhs_value = m_rhs->process_sample(input);
        atomic_add_flag(m_rhs->m_state, Utils::NodeState::PROCESSED);
    }

    m_last_output = m_func(m_last_lhs_value, m_last_rhs_value);

    if (!m_state_saved || (m_state_saved && m_fire_events_during_snapshot))
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
    update_context(value);
    auto& ctx = get_last_context();

    for (const auto& callback : m_callbacks) {
        callback(ctx);
    }

    for (const auto& [callback, condition] : m_conditional_callbacks) {
        if (condition(ctx)) {
            callback(ctx);
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

void BinaryOpNode::save_state()
{
    m_saved_last_lhs_value = m_last_lhs_value;
    m_saved_last_rhs_value = m_last_rhs_value;

    if (m_lhs)
        m_lhs->save_state();
    if (m_rhs)
        m_rhs->save_state();

    m_state_saved = true;
}

void BinaryOpNode::restore_state()
{
    m_last_lhs_value = m_saved_last_lhs_value;
    m_last_rhs_value = m_saved_last_rhs_value;

    if (m_lhs)
        m_lhs->restore_state();
    if (m_rhs)
        m_rhs->restore_state();

    m_state_saved = false;
}

void BinaryOpNode::update_context(double /*value*/)
{
    if (m_gpu_compatible) {
        m_context_gpu.value = m_last_output;
        m_context_gpu.lhs_value = m_last_lhs_value;
        m_context_gpu.rhs_value = m_last_rhs_value;
    } else {
        m_context.value = m_last_output;
        m_context.lhs_value = m_last_lhs_value;
        m_context.rhs_value = m_last_rhs_value;
    }
}

NodeContext& BinaryOpNode::get_last_context()
{
    if (m_gpu_compatible) {
        return m_context_gpu;
    }
    return m_context;
}

bool BinaryOpNode::is_initialized() const
{
    auto lstate = m_lhs->m_state.load();
    auto rstate = m_rhs->m_state.load();
    bool is_lhs_registered = m_lhs ? (lstate & Utils::NodeState::ACTIVE) : false;
    bool is_rhs_registered = m_rhs ? (rstate & Utils::NodeState::ACTIVE) : false;
    return !is_lhs_registered && !is_rhs_registered;
}

} // namespace MayaFlux::Nodes
