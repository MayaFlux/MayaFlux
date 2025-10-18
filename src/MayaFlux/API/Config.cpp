#include "Config.hpp"
#include "Core.hpp"
#include "MayaFlux/Core/Engine.hpp"
#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Journal/ConsoleSink.hpp"
#include "MayaFlux/Journal/FileSink.hpp"

namespace MayaFlux::Config {

GraphConfig graph_config;
NodeConfig node_config;

MAYAFLUX_API GraphConfig::GraphConfig()
    : chain_semantics(Utils::NodeChainSemantics::REPLACE_TARGET)
    , binary_op_semantics(Utils::NodeBinaryOpSemantics::REPLACE)
{
}

MAYAFLUX_API Core::GlobalStreamInfo& get_global_stream_info()
{
    if (get_context().is_running()) {
        MF_WARN(Journal::Component::API, Journal::Context::Configuration, "Accessing stream info while engine is running may lead to inconsistent state.");
    }
    return get_context().get_stream_info();
}

MAYAFLUX_API Core::GlobalGraphicsConfig& get_global_graphics_config()
{
    if (get_context().is_running()) {
        MF_WARN(Journal::Component::API, Journal::Context::Configuration, "Accessing graphics config while engine is running may lead to inconsistent state.");
    }
    return get_context().get_graphics_config();
}

MAYAFLUX_API GraphConfig& get_graph_config() { return graph_config; }

MAYAFLUX_API NodeConfig& get_node_config() { return node_config; }

MAYAFLUX_API uint32_t get_sample_rate()
{
    return get_context().get_stream_info().sample_rate;
}

MAYAFLUX_API uint32_t get_buffer_size()
{
    return get_context().get_stream_info().buffer_size;
}

MAYAFLUX_API uint32_t get_num_out_channels()
{
    return get_context().get_stream_info().output.channels;
}

MAYAFLUX_API void set_journal_severity(Journal::Severity severity)
{
    Journal::Archivist::instance().set_min_severity(severity);
}

MAYAFLUX_API void store_journal_entries(const std::string& file_name)
{
    Journal::Archivist::instance().add_sink(std::make_unique<Journal::FileSink>(file_name));
    set_journal_severity(Journal::Severity::TRACE);
}

MAYAFLUX_API void sink_journal_to_console()
{
    Journal::Archivist::instance().add_sink(std::make_unique<Journal::ConsoleSink>());
}

}
