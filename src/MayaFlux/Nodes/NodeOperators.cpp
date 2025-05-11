#include "NodeOperators.hpp"

namespace MayaFlux::Nodes {

MixAdapter::MixAdapter(std::shared_ptr<Node> primary, std::shared_ptr<Node> secondary, float mix)
    : m_primary(primary)
    , m_secondary(secondary)
    , m_mix(mix)
{
}

double MixAdapter::process_sample(double input)
{
    if (!m_primary || !m_secondary)
        return 0.f;

    double primary_out = m_primary->process_sample(input);
    double secondary_out = m_secondary->process_sample(input);

    return primary_out * (1.0f - m_mix) + secondary_out * m_mix;
}

std::vector<double> MixAdapter::processFull(unsigned int num_samples)
{
    std::vector<double> output(num_samples);
    for (unsigned int i = 0; i < num_samples; ++i) {
        output[i] = process_sample(0.0);
    }
    return output;
}

void MixAdapter::on_tick(NodeHook callback)
{
    m_primary->on_tick(callback);
    m_secondary->on_tick(callback);
}

void MixAdapter::on_tick_if(NodeHook callback, NodeCondition condition)
{
    m_primary->on_tick_if(callback, condition);
    m_secondary->on_tick_if(callback, condition);
}

bool MixAdapter::remove_hook(const NodeHook& callback)
{
    bool removed1 = m_primary->remove_hook(callback);
    bool removed2 = m_secondary->remove_hook(callback);
    return removed1 && removed2;
}

bool MixAdapter::remove_conditional_hook(const NodeCondition& callback)
{
    bool removed1 = m_primary->remove_conditional_hook(callback);
    bool removed2 = m_secondary->remove_conditional_hook(callback);
    return removed1 && removed2;
}

void MixAdapter::remove_all_hooks()
{
    m_primary->remove_all_hooks();
    m_secondary->remove_all_hooks();
}

ChainAdapter::ChainAdapter(std::shared_ptr<Node> source)
    : m_source(source)
    , m_target(nullptr)
{
}

double ChainAdapter::process_sample(double input)
{
    if (!m_source)
        return 0.0;

    double source_out = m_source->process_sample(input);

    if (m_target) {
        return m_target->process_sample(source_out);
    }

    return source_out;
}

std::vector<double> ChainAdapter::processFull(unsigned int num_samples)
{
    std::vector<double> output(num_samples);
    for (unsigned int i = 0; i < num_samples; ++i) {
        output[i] = process_sample(0.0);
    }
    return output;
}

void ChainAdapter::on_tick(NodeHook callback)
{
    if (m_target) {
        m_target->on_tick(callback);
    }
    if (m_source) {
        m_source->on_tick(callback);
    }
}

void ChainAdapter::on_tick_if(NodeHook callback, NodeCondition condition)
{
    if (m_target) {
        m_target->on_tick_if(callback, condition);
    }
    if (m_source) {
        m_source->on_tick_if(callback, condition);
    }
}

bool ChainAdapter::remove_hook(const NodeHook& callback)
{
    bool target_cleared = false;
    bool source_cleared = false;
    if (m_target) {
        target_cleared = m_target->remove_hook(callback);
    }
    if (m_source) {
        source_cleared = m_source->remove_hook(callback);
    }

    return target_cleared || source_cleared;
}

bool ChainAdapter::remove_conditional_hook(const NodeCondition& callback)
{
    bool target_cleared = false;
    bool source_cleared = false;
    if (m_target) {
        target_cleared = m_target->remove_conditional_hook(callback);
    }
    if (m_source) {
        source_cleared = m_source->remove_conditional_hook(callback);
    }

    return target_cleared || source_cleared;
}

void ChainAdapter::remove_all_hooks()
{
    if (m_target) {
        m_target->remove_all_hooks();
    }
    if (m_source) {
        m_source->remove_all_hooks();
    }
}

NodeWrapper::NodeWrapper(std::shared_ptr<Node> wrapped)
    : m_wrapped(wrapped)
    , m_mode(WrapMode::Direct)
{
}

double NodeWrapper::process_sample(double input)
{
    switch (m_mode) {
    case WrapMode::Direct:
        return m_wrapped ? m_wrapped->process_sample(input) : 0.0;

    case WrapMode::Chain:
        if (m_upstream && m_wrapped) {
            double upstream_out = m_upstream->process_sample(input);
            return m_wrapped->process_sample(upstream_out);
        }
        return 0.0;

    case WrapMode::Mix:
        if (m_wrapped) {
            double sum = m_wrapped->process_sample(input);
            for (auto& source : m_mix_sources) {
                if (source) {
                    sum += source->process_sample(input);
                }
            }
            return sum;
        }
        return 0.0;
    }
    return 0.0;
}

std::vector<double> NodeWrapper::processFull(unsigned int num_samples)
{
    std::vector<double> output(num_samples);
    for (unsigned int i = 0; i < num_samples; ++i) {
        output[i] = process_sample(0.0);
    }
    return output;
}

void NodeWrapper::on_tick(NodeHook callback)
{
    if (m_wrapped) {
        m_wrapped->on_tick(callback);
    }
}

void NodeWrapper::on_tick_if(NodeHook callback, NodeCondition condition)
{
    if (m_wrapped) {
        m_wrapped->on_tick_if(callback, condition);
    }
}

bool NodeWrapper::remove_hook(const NodeHook& callback)
{
    if (m_wrapped) {
        return m_wrapped->remove_hook(callback);
    }
    return false;
}

bool NodeWrapper::remove_conditional_hook(const NodeCondition& callback)
{
    if (m_wrapped) {
        return m_wrapped->remove_conditional_hook(callback);
    }
    return false;
}

void NodeWrapper::remove_all_hooks()
{
    if (m_wrapped) {
        m_wrapped->remove_all_hooks();
    }
}

void NodeWrapper::set_upstream(std::shared_ptr<Node> upstream)
{
    m_upstream = upstream;
    m_mode = WrapMode::Chain;
}

void NodeWrapper::add_mix_source(std::shared_ptr<Node> source)
{
    m_mix_sources.push_back(source);
    m_mode = WrapMode::Mix;
}
}
