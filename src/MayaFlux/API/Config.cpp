#include "Config.hpp"
#include "Core.hpp"
#include "MayaFlux/Core/Engine.hpp"

namespace MayaFlux::Config {

GraphConfig graph_config;
NodeConfig node_config;

GraphConfig::GraphConfig()
    : chain_semantics(Utils::NodeChainSemantics::REPLACE_TARGET)
    , binary_op_semantics(Utils::NodeBinaryOpSemantics::REPLACE)
{
}

Core::GlobalStreamInfo& get_global_stream_info()
{
    return get_context().get_stream_info();
}

GraphConfig& get_graph_config() { return graph_config; }

NodeConfig& get_node_config() { return node_config; }

u_int32_t get_sample_rate()
{
    return get_global_stream_info().sample_rate;
}

u_int32_t get_buffer_size()
{
    return get_global_stream_info().buffer_size;
}

u_int32_t get_num_out_channels()
{
    return get_global_stream_info().output.channels;
}

}
