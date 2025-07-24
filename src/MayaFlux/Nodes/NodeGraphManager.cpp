#include "NodeGraphManager.hpp"

#include "NodeStructure.hpp"

namespace MayaFlux::Nodes {

NodeGraphManager::NodeGraphManager()
{
    ensure_root_exists(ProcessingToken::AUDIO_RATE, 0);
}

void NodeGraphManager::add_to_root(std::shared_ptr<Node> node,
    ProcessingToken token,
    unsigned int channel)
{
    set_channel_mask(node, channel);

    auto& root = get_root_node(token, channel);
    root.register_node(node);
}

void NodeGraphManager::remove_from_root(std::shared_ptr<Node> node,
    ProcessingToken token,
    unsigned int channel)
{
    unset_channel_mask(node, channel);

    auto& root = get_root_node(token, channel);
    root.unregister_node(node);
}

void NodeGraphManager::register_token_processor(ProcessingToken token,
    std::function<void(std::span<RootNode*>)> processor)
{
    m_token_processors[token] = processor;
}

const std::unordered_map<unsigned int, std::shared_ptr<RootNode>>& NodeGraphManager::get_all_channel_root_nodes(ProcessingToken token) const
{
    static std::unordered_map<unsigned int, std::shared_ptr<RootNode>> audio_roots;
    audio_roots.clear();

    auto it = m_token_roots.find(token);
    if (it != m_token_roots.end()) {
        for (const auto& [channel, root] : it->second) {
            audio_roots[channel] = root;
        }
    }
    return audio_roots;
}

void NodeGraphManager::process_token(ProcessingToken token, unsigned int num_samples)
{
    auto roots = get_all_root_nodes(token);

    if (auto it = m_token_processors.find(token); it != m_token_processors.end()) {
        it->second(std::span<RootNode*>(roots.data(), roots.size()));
    } else {
        for (auto* root : roots) {
            root->process_batch(num_samples);
        }
    }
}

void NodeGraphManager::register_token_channel_processor(ProcessingToken token,
    TokenChannelProcessor processor)
{
    m_token_channel_processors[token] = processor;
}

void NodeGraphManager::register_token_sample_processor(ProcessingToken token,
    TokenSampleProcessor processor)
{
    m_token_sample_processors[token] = processor;
}

std::vector<double> NodeGraphManager::process_channel(ProcessingToken token,
    unsigned int channel, unsigned int num_samples)
{
    auto& root = get_root_node(token, channel);

    if (auto it = m_token_channel_processors.find(token); it != m_token_channel_processors.end()) {
        return it->second(&root, num_samples);
    } else {
        std::vector<double> samples = root.process_batch(num_samples);
        u_int32_t node_size = root.get_node_size();
        for (double& sample : samples) {
            normalize_sample(sample, node_size);
        }
        return samples;
    }
}

double NodeGraphManager::process_sample(ProcessingToken token, u_int32_t channel)
{
    auto& root = get_root_node(token, channel);

    if (auto it = m_token_sample_processors.find(token); it != m_token_sample_processors.end()) {
        return it->second(&root, channel);
    } else {
        double sample = root.process_sample();
        normalize_sample(sample, root.get_node_size());
        return sample;
    }
}

void NodeGraphManager::normalize_sample(double& sample, u_int32_t num_nodes)
{
    if (num_nodes == 0)
        return;

    sample /= std::sqrt(static_cast<double>(num_nodes));

    const double threshold = 0.95;
    const double knee = 0.1;
    const double abs_sample = std::abs(sample);

    if (abs_sample > threshold) {
        const double excess = abs_sample - threshold;
        const double compressed_excess = std::tanh(excess / knee) * knee;
        const double limited_abs = threshold + compressed_excess;
        sample = std::copysign(limited_abs, sample);
    }
}

std::unordered_map<unsigned int, std::vector<double>> NodeGraphManager::process_token_with_channel_data(
    ProcessingToken token, unsigned int num_samples)
{
    std::unordered_map<unsigned int, std::vector<double>> channel_data;

    auto channels = get_all_channels(token);

    for (unsigned int channel : channels) {
        channel_data[channel] = process_channel(token, channel, num_samples);
    }

    return channel_data;
}

unsigned int NodeGraphManager::get_channel_count(ProcessingToken token) const
{
    auto channels = get_all_channels(token);
    return static_cast<unsigned int>(channels.size());
}

std::vector<RootNode*> NodeGraphManager::get_all_root_nodes(ProcessingToken token)
{
    std::vector<RootNode*> roots;

    auto it = m_token_roots.find(token);
    if (it != m_token_roots.end()) {
        for (auto& [channel, root] : it->second) {
            roots.push_back(root.get());
        }
    }

    return roots;
}

void NodeGraphManager::process_all_tokens(unsigned int num_samples)
{
    for (auto token : get_active_tokens()) {
        process_token(token, num_samples);
    }
}

RootNode& NodeGraphManager::get_root_node(ProcessingToken token, unsigned int channel)
{
    ensure_root_exists(token, channel);
    return *m_token_roots[token][channel];
}

void NodeGraphManager::ensure_root_exists(ProcessingToken token, unsigned int channel)
{
    if (m_token_roots[token].find(channel) == m_token_roots[token].end()) {
        m_token_roots[token][channel] = std::make_shared<RootNode>(token, channel);
    }
}

void NodeGraphManager::register_global(std::shared_ptr<Node> node)
{
    if (!is_node_registered(node)) {
        std::stringstream ss;
        ss << "node_" << node.get();
        std::string generated_id = ss.str();
        m_Node_registry[generated_id] = node;
    }
}

void NodeGraphManager::set_channel_mask(std::shared_ptr<Node> node, u_int32_t channel_id)
{
    register_global(node);
    node->register_channel_usage(channel_id);
}

void NodeGraphManager::unregister_global(std::shared_ptr<Node> node)
{
    for (const auto& pair : m_Node_registry) {
        if (pair.second == node) {
            m_Node_registry.erase(pair.first);
            break;
        }
    }
}

void NodeGraphManager::unset_channel_mask(std::shared_ptr<Node> node, u_int32_t channel_id)
{
    unregister_global(node);
    node->unregister_channel_usage(channel_id);
}

std::vector<ProcessingToken> NodeGraphManager::get_active_tokens() const
{
    std::vector<ProcessingToken> tokens;
    for (const auto& [token, channels] : m_token_roots) {
        if (!channels.empty()) {
            tokens.push_back(token);
        }
    }
    return tokens;
}

std::vector<unsigned int> NodeGraphManager::get_all_channels(ProcessingToken token) const
{
    std::vector<unsigned int> channels;
    auto it = m_token_roots.find(token);
    if (it != m_token_roots.end()) {
        for (const auto& [channel, root] : it->second) {
            channels.push_back(channel);
        }
    }
    return channels;
}

size_t NodeGraphManager::get_node_count(ProcessingToken token) const
{
    size_t count = 0;
    auto it = m_token_roots.find(token);
    if (it != m_token_roots.end()) {
        for (const auto& [channel, root] : it->second) {
            count += root->get_node_size();
        }
    }
    return count;
}

void NodeGraphManager::print_summary() const
{
    std::cout << "=== NodeGraphManager Token Summary ===" << std::endl;
    for (auto token : get_active_tokens()) {
        std::cout << "Token " << static_cast<int>(token)
                  << ": " << get_node_count(token) << " nodes across "
                  << get_all_channels(token).size() << " channels" << std::endl;
    }
}

std::shared_ptr<Node> NodeGraphManager::get_node(const std::string& id)
{
    auto it = m_Node_registry.find(id);

    if (it != m_Node_registry.end()) {
        return it->second;
    }
    return nullptr;
}

bool NodeGraphManager::is_node_registered(std::shared_ptr<Node> node)
{
    for (const auto& pair : m_Node_registry) {
        if (pair.second == node) {
            return true;
        }
    }
    return false;
}

void NodeGraphManager::connect(const std::string& source_id, const std::string& target_id)
{
    auto source = get_node(source_id);
    auto target = get_node(target_id);

    if (source && target) {
        source >> target;
    }
}

}
