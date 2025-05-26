#include "NodeStructure.hpp"
#include "Generators/Generator.hpp"
#include "MayaFlux/MayaFlux.hpp"

namespace MayaFlux::Nodes {

void RootNode::register_node(std::shared_ptr<Node> node)
{
    if (m_is_processing.load(std::memory_order_acquire)) {
        if (m_Nodes.end() != std::find(m_Nodes.begin(), m_Nodes.end(), node)) {
            u_int32_t state = node->m_state.load();
            if (state & Utils::NodeState::INACTIVE) {
                atomic_remove_flag(node->m_state, Utils::NodeState::INACTIVE);
                atomic_add_flag(node->m_state, Utils::NodeState::ACTIVE);
            }
            return;
        }

        for (size_t i = 0; i < MAX_PENDING; ++i) {
            bool expected = false;
            if (m_pending_ops[i].active.compare_exchange_strong(
                    expected, true,
                    std::memory_order_acquire,
                    std::memory_order_relaxed)) {
                m_pending_ops[i].node = node;
                atomic_remove_flag(node->m_state, Utils::NodeState::ACTIVE);
                atomic_add_flag(node->m_state, Utils::NodeState::INACTIVE);
                m_pending_count.fetch_add(1, std::memory_order_relaxed);
                return;
            }
        }

        while (m_is_processing.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
    }

    m_Nodes.push_back(node);
    u_int32_t state = node->m_state.load();
    atomic_add_flag(node->m_state, Utils::NodeState::ACTIVE);
}

void RootNode::unregister_node(std::shared_ptr<Node> node)
{
    u_int32_t state = node->m_state.load();
    atomic_add_flag(node->m_state, Utils::NodeState::PENDING_REMOVAL);

    if (m_is_processing.load(std::memory_order_acquire)) {
        for (size_t i = 0; i < MAX_PENDING; ++i) {
            bool expected = false;
            if (m_pending_ops[i].active.compare_exchange_strong(
                    expected, true,
                    std::memory_order_acquire,
                    std::memory_order_relaxed)) {

                m_pending_ops[i].node = node;
                m_pending_count.fetch_add(1, std::memory_order_relaxed);
                return;
            }
        }

        while (m_is_processing.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
    }

    auto it = m_Nodes.begin();
    while (it != m_Nodes.end()) {
        if ((*it).get() == node.get()) {
            it = m_Nodes.erase(it);
            break;
        }
        ++it;
    }
    node->reset_processed_state();

    u_int32_t flag = node->m_state.load();
    flag &= ~static_cast<u_int32_t>(Utils::NodeState::PENDING_REMOVAL);
    flag &= ~static_cast<u_int32_t>(Utils::NodeState::ACTIVE);
    flag |= static_cast<u_int32_t>(Utils::NodeState::INACTIVE);
    atomic_set_flag_strong(node->m_state, static_cast<Utils::NodeState>(flag));
}

std::vector<double> RootNode::process(unsigned int num_samples)
{
    bool expected = false;
    if (!m_is_processing.compare_exchange_strong(expected, true,
            std::memory_order_acquire, std::memory_order_relaxed)) {
        return std::vector<double>(num_samples, 0.0);
    }

    if (m_pending_count.load(std::memory_order_relaxed) > 0) {
        process_pending_operations();
    }

    std::vector<double> output(num_samples);

    for (auto& node : m_Nodes) {
        node->reset_processed_state();
    }

    for (unsigned int i = 0; i < num_samples; i++) {
        double sample = 0.f;

        for (auto& node : m_Nodes) {

            auto generator = std::dynamic_pointer_cast<Nodes::Generator::Generator>(node);
            if (generator && generator->should_mock_process()) {
                generator->process_sample(0.f);
            } else {
                sample += node->process_sample(0.f);
            }
            u_int32_t state = node->m_state.load();
            atomic_add_flag(node->m_state, Utils::NodeState::PROCESSED);
        }
        output[i] = sample;
    }

    for (auto& node : m_Nodes) {
        node->reset_processed_state();
    }

    m_is_processing.store(false, std::memory_order_release);

    return output;
}

void RootNode::process_pending_operations()
{
    for (size_t i = 0; i < MAX_PENDING; ++i) {
        if (m_pending_ops[i].active.load(std::memory_order_acquire)) {
            auto& op = m_pending_ops[i];
            u_int32_t state = op.node->m_state.load();

            if (!(state & Utils::NodeState::ACTIVE)) {
                m_Nodes.push_back(op.node);

                state &= ~static_cast<u_int32_t>(Utils::NodeState::INACTIVE);
                state |= static_cast<u_int32_t>(Utils::NodeState::ACTIVE);
                atomic_set_flag_strong(op.node->m_state, static_cast<Utils::NodeState>(state));
            } else if (state & Utils::NodeState::PENDING_REMOVAL) {
                auto it = m_Nodes.begin();
                while (it != m_Nodes.end()) {
                    if ((*it).get() == op.node.get()) {
                        it = m_Nodes.erase(it);
                        break;
                    }
                    ++it;
                }
                op.node->reset_processed_state();

                state &= ~static_cast<u_int32_t>(Utils::NodeState::PENDING_REMOVAL);
                state &= ~static_cast<u_int32_t>(Utils::NodeState::ACTIVE);
                state |= static_cast<u_int32_t>(Utils::NodeState::INACTIVE);
                atomic_set_flag_strong(op.node->m_state, static_cast<Utils::NodeState>(state));
            }

            op.node.reset();
            op.active.store(false, std::memory_order_release);
            m_pending_count.fetch_sub(1, std::memory_order_relaxed);
        }
    }
}

ChainNode::ChainNode(std::shared_ptr<Node> source, std::shared_ptr<Node> target)
    : m_Source(source)
    , m_Target(target)
    , m_is_initialized(false)
{
}

void ChainNode::initialize()
{
    if (m_Source) {
        MayaFlux::remove_node_from_root(m_Source);
    }
    if (m_Target) {
        MayaFlux::remove_node_from_root(m_Target);
    }
    if (!m_is_initialized) {
        auto self = shared_from_this();
        MayaFlux::add_node_to_root(self);
        m_is_initialized = true;
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
        output[i] = process_sample(0.f);
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
    if (m_lhs) {
        MayaFlux::remove_node_from_root(m_lhs);
    }
    if (m_rhs) {
        MayaFlux::remove_node_from_root(m_rhs);
    }
    if (!m_is_initialized) {
        auto self = shared_from_this();
        MayaFlux::add_node_to_root(self);
        m_is_initialized = true;
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

bool BinaryOpNode::remove_hook(const NodeHook& callback)
{
    return safe_remove_callback(m_callbacks, callback);
}

bool BinaryOpNode::remove_conditional_hook(const NodeCondition& callback)
{
    return safe_remove_conditional_callback(m_conditional_callbacks, callback);
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
