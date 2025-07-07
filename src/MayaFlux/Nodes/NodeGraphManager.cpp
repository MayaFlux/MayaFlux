#include "NodeGraphManager.hpp"

#include "NodeStructure.hpp"

namespace MayaFlux::Nodes {

NodeGraphManager::NodeGraphManager()
{
    ensure_token_root_exists(ProcessingToken::AUDIO_RATE, 0);
}

void NodeGraphManager::add_to_root(std::shared_ptr<Node> node,
    ProcessingToken token,
    unsigned int channel)
{
    register_node_globally(node);

    auto& root = get_token_root(token, channel);
    root.register_node(node);
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
    auto roots = get_token_roots(token);

    if (auto it = m_token_processors.find(token); it != m_token_processors.end()) {
        it->second(std::span<RootNode*>(roots.data(), roots.size()));
    } else {
        for (auto* root : roots) {
            root->process(num_samples);
        }
    }
}

void NodeGraphManager::register_token_channel_processor(ProcessingToken token,
    std::function<std::vector<double>(RootNode*, unsigned int)> processor)
{
    m_token_channel_processors[token] = processor;
}

std::vector<double> NodeGraphManager::process_token_channel(ProcessingToken token,
    unsigned int channel, unsigned int num_samples)
{
    auto& root = get_token_root(token, channel);

    if (auto it = m_token_channel_processors.find(token); it != m_token_channel_processors.end()) {
        return it->second(&root, num_samples);
    } else {
        return root.process(num_samples);
    }
}

std::unordered_map<unsigned int, std::vector<double>> NodeGraphManager::process_token_with_channel_data(
    ProcessingToken token, unsigned int num_samples)
{
    std::unordered_map<unsigned int, std::vector<double>> channel_data;

    auto channels = get_token_channels(token);

    for (unsigned int channel : channels) {
        channel_data[channel] = process_token_channel(token, channel, num_samples);
    }

    return channel_data;
}

unsigned int NodeGraphManager::get_token_channel_count(ProcessingToken token) const
{
    auto channels = get_token_channels(token);
    return static_cast<unsigned int>(channels.size());
}

std::vector<RootNode*> NodeGraphManager::get_token_roots(ProcessingToken token)
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

RootNode& NodeGraphManager::get_token_root(ProcessingToken token, unsigned int channel)
{
    ensure_token_root_exists(token, channel);
    return *m_token_roots[token][channel];
}

void NodeGraphManager::ensure_token_root_exists(ProcessingToken token, unsigned int channel)
{
    if (m_token_roots[token].find(channel) == m_token_roots[token].end()) {
        m_token_roots[token][channel] = std::make_shared<RootNode>(token, channel);
    }
}

void NodeGraphManager::register_node_globally(std::shared_ptr<Node> node)
{
    if (!is_node_registered(node)) {
        std::stringstream ss;
        ss << "node_" << node.get();
        std::string generated_id = ss.str();
        m_Node_registry[generated_id] = node;
    }
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

std::vector<unsigned int> NodeGraphManager::get_token_channels(ProcessingToken token) const
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

size_t NodeGraphManager::get_token_node_count(ProcessingToken token) const
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

void NodeGraphManager::print_token_summary() const
{
    std::cout << "=== NodeGraphManager Token Summary ===" << std::endl;
    for (auto token : get_active_tokens()) {
        std::cout << "Token " << static_cast<int>(token)
                  << ": " << get_token_node_count(token) << " nodes across "
                  << get_token_channels(token).size() << " channels" << std::endl;
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

void NodeGraphManager::connect(std::shared_ptr<Nodes::Node> source, std::shared_ptr<Nodes::Node> target)
{
    if (source && target) {
        source >> target;
    }
}

}
