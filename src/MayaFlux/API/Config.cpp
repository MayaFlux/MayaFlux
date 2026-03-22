#include "Config.hpp"
#include "Core.hpp"

#include "MayaFlux/Core/Engine.hpp"
#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Journal/ConsoleSink.hpp"
#include "MayaFlux/Journal/FileSink.hpp"

namespace MayaFlux {
bool is_engine_initialized()
{
    return is_initialized();
}
}

namespace MayaFlux::Config {

Core::GlobalStreamInfo& get_global_stream_info()
{
    if (is_initialized()) {
        MF_WARN(Journal::Component::API, Journal::Context::Configuration, "Accessing stream info while engine is running may lead to inconsistent state.");
    }
    return get_context().get_stream_info();
}

Core::GlobalGraphicsConfig& get_global_graphics_config()
{
    if (is_initialized()) {
        MF_WARN(Journal::Component::API, Journal::Context::Configuration, "Accessing graphics config while engine is running may lead to inconsistent state.");
    }
    return get_context().get_graphics_config();
}

Core::GlobalInputConfig& get_global_input_config()
{
    if (is_initialized()) {
        MF_WARN(Journal::Component::API, Journal::Context::Configuration, "Accessing input config while engine is running may lead to inconsistent state.");
    }
    return get_context().get_input_config();
}

Core::GlobalNetworkConfig& get_global_network_config()
{
    if (is_initialized()) {
        MF_WARN(Journal::Component::API, Journal::Context::Configuration, "Accessing network config while engine is running may lead to inconsistent state.");
    }
    return get_context().get_network_config();
}

Nodes::NodeConfig& get_node_config()
{
    return get_context().get_node_config();
}

void set_node_config(const Nodes::NodeConfig& config)
{
    if (!is_initialized()) {
        MF_WARN(Journal::Component::API, Journal::Context::Configuration, "Engine is not running. Setting node config on context directly.");
    }
    get_context().set_node_config(config);
}

uint32_t get_sample_rate()
{
    if (!is_initialized()) {
        MF_WARN(Journal::Component::API, Journal::Context::Configuration, "Engine is not running. Returning default sample rate");
        return 48000;
    }
    return get_context().get_stream_info().sample_rate;
}

uint32_t get_buffer_size()
{
    if (!is_initialized()) {
        MF_WARN(Journal::Component::API, Journal::Context::Configuration, "Engine is not running. Returning default buffer size");
        return 512;
    }
    return get_context().get_stream_info().buffer_size;
}

uint32_t get_num_out_channels()
{
    if (!is_initialized()) {
        MF_WARN(Journal::Component::API, Journal::Context::Configuration, "Engine is not running. Returning default number of output channels");
        return 2;
    }
    return get_context().get_stream_info().output.channels;
}

void set_journal_severity(Journal::Severity severity)
{
    Journal::Archivist::instance().set_min_severity(severity);
}

void store_journal_entries(const std::string& file_name)
{
    Journal::Archivist::instance().add_sink(std::make_unique<Journal::FileSink>(file_name));
    set_journal_severity(Journal::Severity::TRACE);
}

void sink_journal_to_console()
{
    Journal::Archivist::instance().add_sink(std::make_unique<Journal::ConsoleSink>());
}

}
