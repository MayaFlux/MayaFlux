#include "NodeStructure.hpp"
#include "Generators/Generator.hpp"
#include "MayaFlux/MayaFlux.hpp"

namespace MayaFlux::Nodes {

void RootNode::register_node(std::shared_ptr<Node> node)
{
    m_Nodes.push_back(node);
    node->mark_registered_for_processing(true);
}

void RootNode::unregister_node(std::shared_ptr<Node> node)
{
    auto it = m_Nodes.begin();
    while (it != m_Nodes.end()) {
        if ((*it).get() == node.get()) {
            it = m_Nodes.erase(it);
            break;
        }
        ++it;
    }
    node->reset_processed_state();
    node->mark_registered_for_processing(false);
}

std::vector<double> RootNode::process(unsigned int num_samples)
{
    std::vector<double> output(num_samples);

    for (auto& node : m_Nodes) {
        node->mark_processed(false);
    }

    for (unsigned int i = 0; i < num_samples; i++) {
        double sample = 0.f;

        for (auto& node : m_Nodes) {
            std::lock_guard<std::mutex> lock(m_mutex);

            auto generator = std::dynamic_pointer_cast<Nodes::Generator::Generator>(node);
            if (generator && generator->should_mock_process()) {
                generator->process_sample(0.f);
            } else {
                sample += node->process_sample(0.f);
            }
            node->mark_processed(true);
        }
        output[i] = sample;
    }

    for (auto& node : m_Nodes) {
        node->reset_processed_state();
    }

    return output;
}

ChainNode::ChainNode(std::shared_ptr<Node> source, std::shared_ptr<Node> target)
    : m_Source(source)
    , m_Target(target)
    , m_is_registered(false)
    , m_is_processed(false)
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
    if (!is_initialized())
        initialize();

    m_last_output = input;

    if (m_Source->is_processed()) {
        m_last_output += m_Source->get_last_output();
    } else {
        m_last_output += m_Source->process_sample(input);
        m_Source->mark_processed(true);
    }
    if (m_Target->is_processed()) {
        m_last_output += m_Target->get_last_output();
    } else {
        m_last_output = m_Target->process_sample(m_last_output);
        m_Target->mark_processed(true);
    }
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
    mark_processed(false);
    if (m_Source)
        m_Source->reset_processed_state();
    if (m_Target)
        m_Target->reset_processed_state();
}

BinaryOpNode::BinaryOpNode(std::shared_ptr<Node> lhs, std::shared_ptr<Node> rhs, CombineFunc func)
    : m_lhs(lhs)
    , m_rhs(rhs)
    , m_func(func)
    , m_is_registered(false)
    , m_is_processed(false)
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
    if (!is_initialized())
        initialize();

    if (m_lhs->is_processed()) {
        m_last_lhs_value = m_lhs->get_last_output();
    } else {
        m_last_lhs_value = m_lhs->process_sample(input);
        m_lhs->mark_processed(true);
    }
    if (m_rhs->is_processed()) {
        m_last_rhs_value = m_rhs->get_last_output();
    } else {
        m_last_rhs_value = m_rhs->process_sample(input);
        m_rhs->mark_processed(true);
    }

    m_last_output = m_func(m_last_lhs_value, m_last_rhs_value);

    notify_tick(m_last_output);

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
    mark_processed(false);
    if (m_lhs)
        m_lhs->reset_processed_state();
    if (m_rhs)
        m_rhs->reset_processed_state();
}
}
