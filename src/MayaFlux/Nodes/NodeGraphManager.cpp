#include "NodeGraphManager.hpp"

#include "Network/NodeNetwork.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Nodes {

NodeGraphManager::NodeGraphManager()
{
    ensure_root_exists(ProcessingToken::AUDIO_RATE, 0);
}

void NodeGraphManager::add_to_root(const std::shared_ptr<Node>& node,
    ProcessingToken token,
    unsigned int channel)
{
    set_channel_mask(node, channel);

    auto& root = get_root_node(token, channel);
    root.register_node(node);
}

void NodeGraphManager::remove_from_root(const std::shared_ptr<Node>& node,
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
    m_token_processors[token] = std::move(processor);
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

bool NodeGraphManager::preprocess_networks(ProcessingToken token)
{
    auto& processing_ptr = m_token_network_processing[token];

    if (!processing_ptr) {
        processing_ptr = std::make_unique<std::atomic<bool>>(false);
    }

    bool expected = false;
    return processing_ptr->compare_exchange_strong(
        expected, true,
        std::memory_order_acquire,
        std::memory_order_relaxed);
}

void NodeGraphManager::process_token(ProcessingToken token, unsigned int num_samples)
{
    if (m_terminate_requested.load())
        return;

    auto roots = get_all_root_nodes(token);

    if (auto it = m_token_processors.find(token); it != m_token_processors.end()) {
        it->second(std::span<RootNode*>(roots.data(), roots.size()));
        return;
    }

    if (!preprocess_networks(token)) {
        return;
    }

    auto it = m_token_networks.find(token);
    if (it != m_token_networks.end()) {
        for (auto& network : it->second) {
            if (!network || !network->is_enabled()) {
                continue;
            }

            if (!network->is_processed_this_cycle()) {
                network->mark_processing(true);
                network->process_batch(num_samples);
                network->mark_processing(false);
                network->mark_processed(true);
            }
        }
    }

    postprocess_networks(token, std::nullopt);

    if (token == ProcessingToken::AUDIO_RATE) {
        for (auto* root : roots) {
            root->process_batch(num_samples);
        }
    } else if (token == ProcessingToken::VISUAL_RATE) {
        for (auto* root : roots) {
            root->process_batch_frame(num_samples);
        }
    }
}

std::vector<std::vector<double>> NodeGraphManager::process_audio_networks(ProcessingToken token, uint32_t num_samples, uint32_t channel)
{
    if (!preprocess_networks(token)) {
        return {};
    }

    std::vector<std::vector<double>> all_network_outputs;

    auto audio_it = m_audio_networks.find(token);
    if (audio_it != m_audio_networks.end()) {
        for (auto& network : audio_it->second) {
            if (!network || !network->is_enabled()) {
                continue;
            }

            if (!network->is_registered_on_channel(channel)) {
                continue;
            }

            if (!network->is_processed_this_cycle()) {
                network->mark_processing(true);
                network->process_batch(num_samples);
                network->mark_processing(false);
                network->mark_processed(true);
            }

            const auto& net_buffer = network->get_audio_buffer();
            if (net_buffer) {
                if (network->needs_channel_routing()) {
                    double scale = network->get_routing_state().amount[channel];
                    if (scale == 0.0)
                        continue;

                    if (scale == 1.0) {
                        all_network_outputs.push_back(*net_buffer);
                    } else {
                        std::vector<double> scaled_buffer = *net_buffer;
                        for (auto& sample : scaled_buffer)
                            sample *= scale;

                        all_network_outputs.push_back(std::move(scaled_buffer));
                    }
                } else {
                    all_network_outputs.push_back(*net_buffer);
                }
            }
        }
    }

    postprocess_networks(token, channel);
    return all_network_outputs;
}

void NodeGraphManager::postprocess_networks(ProcessingToken token, std::optional<uint32_t> channel)
{
    if (token == ProcessingToken::AUDIO_RATE && channel.has_value()) {
        auto ch = channel.value_or(0U);
        reset_audio_network_state(token, ch);
    } else if (auto it = m_token_networks.find(token); it != m_token_networks.end()) {
        for (auto& network : it->second) {
            if (network && network->is_enabled()) {
                network->mark_processed(false);
            }
        }
    }

    if (auto it = m_token_network_processing.find(token); it != m_token_network_processing.end()) {
        it->second->store(false, std::memory_order_release);
    }
}

void NodeGraphManager::reset_audio_network_state(ProcessingToken token, uint32_t channel)
{
    auto audio_it = m_audio_networks.find(token);
    if (audio_it != m_audio_networks.end()) {
        for (auto& network : audio_it->second) {
            if (network) {
                if (network->is_registered_on_channel(channel)) {
                    network->request_reset_from_channel(channel);
                }
            }
        }
    }
}

void NodeGraphManager::register_token_channel_processor(ProcessingToken token,
    TokenChannelProcessor processor)
{
    m_token_channel_processors[token] = std::move(processor);
}

void NodeGraphManager::register_token_sample_processor(ProcessingToken token,
    TokenSampleProcessor processor)
{
    m_token_sample_processors[token] = std::move(processor);
}

std::vector<double> NodeGraphManager::process_channel(ProcessingToken token,
    unsigned int channel, unsigned int num_samples)
{
    if (channel == 0) {
        reset_audio_network_state(token);
    }

    auto& root = get_root_node(token, channel);

    if (auto it = m_token_channel_processors.find(token); it != m_token_channel_processors.end()) {
        return it->second(&root, num_samples);
    }

    std::vector<double> samples = root.process_batch(num_samples);

    uint32_t normalize_coef = root.get_node_size();
    for (double& sample : samples) {
        normalize_sample(sample, normalize_coef);
    }
    return samples;
}

double NodeGraphManager::process_sample(ProcessingToken token, uint32_t channel)
{
    if (m_terminate_requested.load())
        return 0.0;

    auto& root = get_root_node(token, channel);

    if (auto it = m_token_sample_processors.find(token); it != m_token_sample_processors.end()) {
        return it->second(&root, channel);
    }

    double sample = root.process_sample();
    normalize_sample(sample, root.get_node_size());
    return sample;
}

void NodeGraphManager::normalize_sample(double& sample, uint32_t num_nodes)
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

void NodeGraphManager::ensure_token_exists(ProcessingToken token, uint32_t num_channels)
{
    for (uint32_t ch = 0; ch < num_channels; ++ch) {
        ensure_root_exists(token, ch);
    }
}

void NodeGraphManager::register_global(const std::shared_ptr<Node>& node)
{
    if (!is_node_registered(node)) {
        std::stringstream ss;
        ss << "node_" << node.get();
        std::string generated_id = ss.str();
        m_Node_registry[generated_id] = node;
    }
}

void NodeGraphManager::set_channel_mask(const std::shared_ptr<Node>& node, uint32_t channel_id)
{
    register_global(node);
    node->register_channel_usage(channel_id);
}

void NodeGraphManager::unregister_global(const std::shared_ptr<Node>& node)
{
    for (const auto& pair : m_Node_registry) {
        if (pair.second == node) {
            m_Node_registry.erase(pair.first);
            break;
        }
    }
}

void NodeGraphManager::unset_channel_mask(const std::shared_ptr<Node>& node, uint32_t channel_id)
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
    MF_PRINT(Journal::Component::Nodes,
        Journal::Context::NodeProcessing,
        "=== NodeGraphManager Summary ===");

    for (auto token : get_active_tokens()) {
        auto channels = get_all_channels(token);
        size_t total_nodes = get_node_count(token);

        MF_PRINT(Journal::Component::Nodes,
            Journal::Context::NodeProcessing,
            "Token {}: {} nodes across {} channels",
            static_cast<int>(token), total_nodes, channels.size());

        for (auto channel : channels) {
            auto& root = const_cast<NodeGraphManager*>(this)->get_root_node(token, channel);
            auto networks = get_networks(token, channel);

            MF_PRINT(Journal::Component::Nodes,
                Journal::Context::NodeProcessing,
                "  Channel {}: {} nodes, {} networks",
                channel, root.get_node_size(), networks.size());

            for (const auto& network : networks) {
                if (network) {
                    MF_PRINT(Journal::Component::Nodes,
                        Journal::Context::NodeProcessing,
                        "    Network: {} internal nodes, mode={}, enabled={}",
                        network->get_node_count(),
                        static_cast<int>(network->get_output_mode()),
                        network->is_enabled());
                }
            }
        }
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

bool NodeGraphManager::is_node_registered(const std::shared_ptr<Node>& node)
{
    return std::ranges::any_of(m_Node_registry,
        [&node](const auto& pair) { return pair.second == node; });
}

void NodeGraphManager::connect(const std::string& source_id, const std::string& target_id)
{
    auto source = get_node(source_id);
    auto target = get_node(target_id);

    if (source && target) {
        source >> target;
    }
}

//-----------------------------------------------------------------------------
// NodeNetwork Management
//-----------------------------------------------------------------------------

void NodeGraphManager::add_network(const std::shared_ptr<Network::NodeNetwork>& network,
    ProcessingToken token)
{
    if (!network) {
        return;
    }

    register_network_global(network);

    network->set_enabled(true);

    if (network->get_output_mode() == Network::NodeNetwork::OutputMode::AUDIO_SINK) {
        uint32_t channel_mask = network->get_channel_mask();

        if (channel_mask == 0) {
            channel_mask = 1;
            network->add_channel_usage(0);
        }

        auto channels = network->get_registered_channels();
        m_audio_networks[token].push_back(network);

        for (auto ch : channels) {
            ensure_root_exists(token, ch);
            MF_INFO(Journal::Component::Nodes,
                Journal::Context::NodeProcessing,
                "Added audio network to token {} channel {}: {} nodes",
                static_cast<int>(token), ch, network->get_node_count());
        }

    } else {
        m_token_networks[token].push_back(network);

        MF_INFO(Journal::Component::Nodes,
            Journal::Context::NodeProcessing,
            "Added network to token {}: {} nodes, mode={}",
            static_cast<int>(token),
            network->get_node_count(),
            static_cast<int>(network->get_output_mode()));
    }
}

void NodeGraphManager::remove_network(const std::shared_ptr<Network::NodeNetwork>& network,
    ProcessingToken token)
{
    if (!network) {
        return;
    }

    if (network->get_output_mode() == Network::NodeNetwork::OutputMode::AUDIO_SINK) {
        auto token_it = m_audio_networks.find(token);
        if (token_it != m_audio_networks.end()) {
            auto& networks = token_it->second;
            std::erase_if(networks, [&](const auto& n) { return n == network; });
        }
    } else {
        auto it = m_token_networks.find(token);
        if (it != m_token_networks.end()) {
            auto& networks = it->second;
            std::erase_if(networks, [&](const auto& n) { return n == network; });
        }
    }

    unregister_network_global(network);
}

std::vector<std::shared_ptr<Network::NodeNetwork>>
NodeGraphManager::get_networks(ProcessingToken token, unsigned int channel) const
{
    auto token_it = m_audio_networks.find(token);
    if (token_it != m_audio_networks.end()) {
        std::vector<std::shared_ptr<Network::NodeNetwork>> networks_on_channel;
        for (const auto& network : token_it->second) {
            if (network && network->is_registered_on_channel(channel)) {
                networks_on_channel.push_back(network);
            }
        }
        return networks_on_channel;
    }
    return {};
}

std::vector<std::shared_ptr<Network::NodeNetwork>>
NodeGraphManager::get_all_networks(ProcessingToken token) const
{
    std::vector<std::shared_ptr<Network::NodeNetwork>> all_networks;

    auto audio_it = m_audio_networks.find(token);
    if (audio_it != m_audio_networks.end()) {
        all_networks.insert(all_networks.end(),
            audio_it->second.begin(),
            audio_it->second.end());
    }

    auto token_it = m_token_networks.find(token);
    if (token_it != m_token_networks.end()) {
        all_networks.insert(all_networks.end(),
            token_it->second.begin(),
            token_it->second.end());
    }

    return all_networks;
}

size_t NodeGraphManager::get_network_count(ProcessingToken token) const
{
    size_t count = 0;

    auto audio_it = m_audio_networks.find(token);
    if (audio_it != m_audio_networks.end()) {
        count += audio_it->second.size();
    }

    auto token_it = m_token_networks.find(token);
    if (token_it != m_token_networks.end()) {
        count += token_it->second.size();
    }

    return count;
}

void NodeGraphManager::clear_networks(ProcessingToken token)
{
    m_audio_networks.erase(token);
    m_token_networks.erase(token);
}

void NodeGraphManager::register_network_global(const std::shared_ptr<Network::NodeNetwork>& network)
{
    if (!is_network_registered(network)) {
        std::stringstream ss;
        ss << "network_" << network.get();
        std::string generated_id = ss.str();
        m_network_registry[generated_id] = network;
    }
}

void NodeGraphManager::unregister_network_global(const std::shared_ptr<Network::NodeNetwork>& network)
{
    for (const auto& pair : m_network_registry) {
        if (pair.second == network) {
            m_network_registry.erase(pair.first);
            break;
        }
    }
}

bool NodeGraphManager::is_network_registered(const std::shared_ptr<Network::NodeNetwork>& network)
{
    return std::ranges::any_of(m_network_registry,
        [&network](const auto& pair) { return pair.second == network; });
}

void NodeGraphManager::terminate_active_processing()
{
    if (m_terminate_requested.load())
        return;

    for (auto& [token, networks] : m_audio_networks) {
        for (auto& network : networks) {
            if (network) {
                unregister_network_global(network);
            }
        }
    }

    for (auto& [token, networks] : m_token_networks) {
        for (auto& network : networks) {
            if (network) {
                unregister_network_global(network);
            }
        }
    }

    m_terminate_requested.store(true);

    for (auto token : get_active_tokens()) {
        auto roots = get_all_root_nodes(token);
        for (auto* root : roots) {
            root->terminate_all_nodes();
        }
    }
}

NodeGraphManager::~NodeGraphManager()
{
    terminate_active_processing();
    m_token_roots.clear();
    m_audio_networks.clear();
    m_token_networks.clear();

    m_Node_registry.clear();
    m_network_registry.clear();
}

void NodeGraphManager::update_routing_states_for_cycle(ProcessingToken token)
{
    for (const auto& [id, node] : m_Node_registry) {
        if (!node->needs_channel_routing())
            continue;
        update_routing_state(node->get_routing_state());
    }

    for (const auto& network : get_all_networks(token)) {
        if (!network || !network->needs_channel_routing())
            continue;
        update_routing_state(network->get_routing_state());
    }
}

void NodeGraphManager::route_node_to_channels(
    const std::shared_ptr<Node>& node,
    const std::vector<uint32_t>& target_channels,
    uint32_t fade_cycles,
    ProcessingToken token)
{
    uint32_t current_channels = node->get_channel_mask();

    uint32_t target_bitmask = 0;
    for (auto ch : target_channels) {
        target_bitmask |= (1 << ch);
    }

    uint32_t block_size = 512; // temporary
    uint32_t fade_blocks = (fade_cycles + block_size - 1) / block_size;
    fade_blocks = std::max(1u, fade_blocks);

    RoutingState state;
    state.from_channels = current_channels;
    state.to_channels = target_bitmask;
    state.fade_cycles = fade_blocks;
    state.phase = RoutingState::ACTIVE;

    for (uint32_t ch = 0; ch < 32; ch++) {
        state.amount[ch] = (current_channels & (1 << ch)) ? 1.0 : 0.0;
    }

    node->get_routing_state() = state;

    for (auto ch : target_channels) {
        if (!(current_channels & (1 << ch))) {
            add_to_root(node, token, ch);
        }
    }
}

void NodeGraphManager::route_network_to_channels(
    const std::shared_ptr<Network::NodeNetwork>& network,
    const std::vector<uint32_t>& target_channels,
    uint32_t fade_cycles,
    ProcessingToken token)
{
    if (network->get_output_mode() != Network::NodeNetwork::OutputMode::AUDIO_SINK) {
        MF_WARN(Journal::Component::Nodes,
            Journal::Context::NodeProcessing,
            "Attempted to route network that is not an audio sink. Operation ignored.");
        return;
    }

    register_network_global(network);
    network->set_enabled(true);

    auto& networks = m_audio_networks[token];
    if (std::ranges::find(networks, network) == networks.end()) {
        networks.push_back(network);
    }

    uint32_t current_channels = network->get_channel_mask();

    uint32_t target_bitmask = 0;
    for (auto ch : target_channels) {
        target_bitmask |= (1 << ch);
    }

    network->set_channel_mask(target_bitmask);
    for (auto ch : target_channels) {
        network->add_channel_usage(ch);
        ensure_root_exists(token, ch);
    }

    RoutingState state;
    state.from_channels = current_channels;
    state.to_channels = target_bitmask;
    state.fade_cycles = fade_cycles;
    state.cycles_elapsed = 0;
    state.phase = RoutingState::ACTIVE;

    for (uint32_t ch = 0; ch < 32; ch++) {
        state.amount[ch] = (current_channels & (1 << ch)) ? 1.0 : 0.0;
    }

    network->get_routing_state() = state;
}

void NodeGraphManager::cleanup_completed_routing(ProcessingToken token)
{
    std::vector<std::pair<std::shared_ptr<Node>, uint32_t>> nodes_to_remove;

    for (const auto& [id, node] : m_Node_registry) {
        if (!node->needs_channel_routing())
            continue;

        auto& state = node->get_routing_state();

        if (state.phase == RoutingState::COMPLETED) {
            for (uint32_t ch = 0; ch < 32; ch++) {
                if ((state.from_channels & (1 << ch)) && !(state.to_channels & (1 << ch))) {
                    nodes_to_remove.emplace_back(node, ch);
                }
            }
            state = RoutingState {};
        }
    }

    for (auto& [node, channel] : nodes_to_remove) {
        remove_from_root(node, token, channel);
    }

    std::vector<std::pair<std::shared_ptr<Network::NodeNetwork>, uint32_t>> networks_to_cleanup;

    for (const auto& network : get_all_networks(token)) {
        if (!network || !network->needs_channel_routing())
            continue;

        auto& state = network->get_routing_state();

        if (state.phase == RoutingState::COMPLETED) {
            for (uint32_t ch = 0; ch < 32; ch++) {
                if ((state.from_channels & (1 << ch)) && !(state.to_channels & (1 << ch))) {
                    networks_to_cleanup.emplace_back(network, ch);
                }
            }
            state = RoutingState {};
        }
    }

    for (auto& [network, channel] : networks_to_cleanup) {
        network->remove_channel_usage(channel);

        if (network->get_channel_mask() == 0) {
            auto& networks = m_audio_networks[token];
            std::erase_if(networks, [&](const auto& n) { return n == network; });
            unregister_network_global(network);
        }
    }
}

}
