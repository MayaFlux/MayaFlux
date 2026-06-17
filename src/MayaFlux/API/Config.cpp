#include "Config.hpp"
#include "Core.hpp"

#include "MayaFlux/Core/Engine.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Journal/ConsoleSink.hpp"
#include "MayaFlux/Journal/FileSink.hpp"

#include "MayaFlux/Transitive/IO/JSONSerializer.hpp"

#include "MayaFlux/IO/GlmSerializer.hpp"

namespace MayaFlux {

namespace {
    struct JournalConfig {
        std::string severity; // "TRACE" "DEBUG" "INFO" "WARN" "ERROR" "FATAL"
        bool sink_to_console { false };
        std::string log_file; // empty = no file sink
        std::vector<std::string> disable_components; // names from Journal::Component enum
        std::vector<std::string> disable_contexts; // names from Journal::Context enum

        static constexpr auto describe()
        {
            return std::make_tuple(
                Reflect::member("severity", &JournalConfig::severity),
                Reflect::member("sink_to_console", &JournalConfig::sink_to_console),
                Reflect::member("log_file", &JournalConfig::log_file),
                Reflect::member("disable_components", &JournalConfig::disable_components),
                Reflect::member("disable_contexts", &JournalConfig::disable_contexts));
        }
    };

    struct EngineConfig {
        Core::GlobalStreamInfo stream;
        Core::GlobalGraphicsConfig graphics;
        Core::GlobalInputConfig input;
        Core::GlobalNetworkConfig network;
        JournalConfig journal;

        static constexpr auto describe()
        {
            return std::make_tuple(
                Reflect::member("stream", &EngineConfig::stream),
                Reflect::member("graphics", &EngineConfig::graphics),
                Reflect::member("input", &EngineConfig::input),
                Reflect::member("network", &EngineConfig::network),
                Reflect::member("journal", &EngineConfig::journal));
        }
    };
}

bool is_engine_configured()
{
    return is_configured();
}
}

namespace MayaFlux::Config {

Core::GlobalStreamInfo& get_global_stream_info()
{
    if (is_engine_configured()) {
        MF_WARN(Journal::Component::API, Journal::Context::Configuration, "Accessing stream info while engine is running may lead to inconsistent state.");
    }
    return get_context().get_stream_info();
}

Core::GlobalGraphicsConfig& get_global_graphics_config()
{
    if (is_engine_configured()) {
        MF_WARN(Journal::Component::API, Journal::Context::Configuration, "Accessing graphics config while engine is running may lead to inconsistent state.");
    }
    return get_context().get_graphics_config();
}

Core::GlobalInputConfig& get_global_input_config()
{
    if (is_engine_configured()) {
        MF_WARN(Journal::Component::API, Journal::Context::Configuration, "Accessing input config while engine is running may lead to inconsistent state.");
    }
    return get_context().get_input_config();
}

Core::GlobalNetworkConfig& get_global_network_config()
{
    if (is_engine_configured()) {
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
    if (!is_engine_configured()) {
        MF_WARN(Journal::Component::API, Journal::Context::Configuration, "Engine is not running. Setting node config on context directly.");
    }
    get_context().set_node_config(config);
}

uint32_t get_sample_rate()
{
    if (!is_engine_configured()) {
        MF_WARN(Journal::Component::API, Journal::Context::Configuration, "Engine is not running. Returning default sample rate");
        return 48000;
    }
    return get_context().get_stream_info().sample_rate;
}

uint32_t get_buffer_size()
{
    if (!is_engine_configured()) {
        MF_WARN(Journal::Component::API, Journal::Context::Configuration, "Engine is not running. Returning default buffer size");
        return 512;
    }
    return get_context().get_stream_info().buffer_size;
}

uint32_t get_num_out_channels()
{
    if (!is_engine_configured()) {
        MF_WARN(Journal::Component::API, Journal::Context::Configuration, "Engine is not running. Returning default number of output channels");
        return 2;
    }
    return get_context().get_stream_info().output.channels;
}

void set_journal_severity(Journal::Severity severity)
{
    Journal::Archivist::instance().set_min_severity(severity);
}

void set_journal_component_filter(const std::vector<Journal::Component>& component, bool enabled)
{
    for (const auto& comp : component) {
        Journal::Archivist::instance().set_component_filter(comp, enabled);
    }
}

void set_journal_context_filter(const std::vector<Journal::Context>& context, bool enabled)
{
    for (const auto& ctx : context) {
        Journal::Archivist::instance().set_context_filter(ctx, enabled);
    }
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

bool load_config_from_file(const std::string& path)
{
    IO::JSONSerializer ser;
    auto result = ser.read<EngineConfig>(path);
    if (!result) {
        MF_ERROR(Journal::Component::API, Journal::Context::Configuration,
            "Failed to load config from {}: {}", path, ser.last_error());
        return false;
    }
    get_global_stream_info() = result->stream;
    get_global_graphics_config() = result->graphics;
    get_global_input_config() = result->input;
    get_global_network_config() = result->network;

    const auto& j = result->journal;

    if (!j.severity.empty()) {
        auto sev = Reflect::string_to_enum_case_insensitive<Journal::Severity>(j.severity);
        if (sev) {
            Config::set_journal_severity(*sev);
        } else {
            MF_WARN(Journal::Component::API, Journal::Context::Configuration,
                "Unknown journal severity: {}", j.severity);
        }
    }
    if (j.sink_to_console)
        Config::sink_journal_to_console();
    if (!j.log_file.empty())
        Config::store_journal_entries(j.log_file);
    for (const auto& name : j.disable_components) {
        auto comp = Reflect::string_to_enum_case_insensitive<Journal::Component>(name);
        if (comp) {
            Config::set_journal_component_filter({ *comp }, false);
        } else {
            MF_WARN(Journal::Component::API, Journal::Context::Configuration,
                "Unknown journal component: {}", name);
        }
    }

    for (const auto& name : j.disable_contexts) {
        auto ctx = Reflect::string_to_enum_case_insensitive<Journal::Context>(name);
        if (ctx) {
            Config::set_journal_context_filter({ *ctx }, false);
        } else {
            MF_WARN(Journal::Component::API, Journal::Context::Configuration,
                "Unknown journal context: {}", name);
        }
    }

    return true;
}

}
