#include "Engine.hpp"
#include "MayaFlux/Buffers/BufferManager.hpp"
#include "MayaFlux/Nodes/Generators/Stochastic.hpp"
#include "MayaFlux/Nodes/NodeGraphManager.hpp"
#include "MayaFlux/Vruta/Scheduler.hpp"

namespace MayaFlux::Core {

//-------------------------------------------------------------------------
// Initialization and Lifecycle
//-------------------------------------------------------------------------

Engine::Engine()
    : m_is_initialized(false)
    , m_rng(new Nodes::Generator::Stochastics::NoiseEngine())
    , m_is_paused(false)
{
}

Engine::~Engine()
{
    End();
}

Engine::Engine(Engine&& other) noexcept
    : m_subsystem_manager(std::move(other.m_subsystem_manager))
    , m_node_graph_manager(std::move(other.m_node_graph_manager))
    , m_buffer_manager(std::move(other.m_buffer_manager))
    , m_scheduler(std::move(other.m_scheduler))
    , m_rng(std::move(other.m_rng))
    , m_stream_info(other.m_stream_info)
    , m_is_initialized(other.m_is_initialized)
    , m_is_paused(other.m_is_paused)
{
    other.m_is_initialized = false;
    other.m_is_paused = false;
}

Engine& Engine::operator=(Engine&& other) noexcept
{
    if (this != &other) {
        End();

        m_stream_info = other.m_stream_info;

        m_subsystem_manager = std::move(other.m_subsystem_manager);
        m_node_graph_manager = std::move(other.m_node_graph_manager);
        m_buffer_manager = std::move(other.m_buffer_manager);
        m_scheduler = std::move(other.m_scheduler);
        m_rng = std::move(other.m_rng);

        m_is_initialized = other.m_is_initialized;
        m_is_paused = other.m_is_paused;

        other.m_is_initialized = false;
        other.m_is_paused = false;
    }
    return *this;
}

void Engine::Init(u_int32_t sample_rate, u_int32_t buffer_size, u_int32_t num_out_channels, u_int32_t num_in_channels)
{
    GlobalStreamInfo streamInfo;
    streamInfo.sample_rate = sample_rate;
    streamInfo.buffer_size = buffer_size;
    streamInfo.output.channels = num_out_channels;

    if (num_in_channels > 0) {
        streamInfo.input.enabled = true;
        streamInfo.input.channels = num_in_channels;
    } else {
        streamInfo.input.enabled = false;
    }

    Init(streamInfo);
}

void Engine::Init(const GlobalStreamInfo& streamInfo)
{
    m_stream_info = streamInfo;

    m_scheduler = std::make_shared<Vruta::TaskScheduler>(m_stream_info.sample_rate);

    m_buffer_manager = std::make_shared<Buffers::BufferManager>(
        m_stream_info.output.channels,
        m_stream_info.input.enabled ? m_stream_info.input.channels : 0,
        m_stream_info.buffer_size);

    m_node_graph_manager = std::make_shared<Nodes::NodeGraphManager>();

    m_subsystem_manager = std::make_shared<SubsystemManager>(
        m_node_graph_manager, m_buffer_manager, m_scheduler);

    m_subsystem_manager->create_audio_subsystem(m_stream_info, Utils::AudioBackendType::RTAUDIO);
    m_is_initialized = true;
}

void Engine::Start()
{
    if (!m_is_initialized) {
        Init();
    }
    m_subsystem_manager->start_all_subsystems();
}

void Engine::Pause()
{
    if (m_is_paused) {
        return;
    }

    m_is_paused = true;
}

void Engine::Resume()
{
    if (!m_is_paused) {
        return;
    }
    m_is_paused = false;
}

void Engine::End()
{
    if (m_subsystem_manager) {
        m_subsystem_manager->shutdown();
    }

    m_is_initialized = false;
    m_is_paused = false;

    if (m_buffer_manager) {
        for (auto token : m_buffer_manager->get_active_tokens()) {
            for (size_t i = 0; i < m_stream_info.output.channels; i++) {
                auto root = m_buffer_manager->get_root_audio_buffer(token, i);
                if (root) {
                    root->clear();
                    for (auto& child : root->get_child_buffers()) {
                        child->clear();
                    }
                }
            }
        }
    }

    if (m_node_graph_manager) {
        for (auto token : m_node_graph_manager->get_active_tokens()) {
            for (auto root : m_node_graph_manager->get_all_root_nodes(token)) {
                if (root) {
                    root->clear_all_nodes();
                }
            }
        }
    }
}

bool Engine::is_running() const
{
    if (!m_is_initialized || m_is_paused) {
        return false;
    }

    if (m_is_initialized) {
        auto status = m_subsystem_manager->query_subsystem_status();
        for (const auto& [type, readiness] : status) {
            const auto& [is_ready, is_running] = readiness;
            if (is_ready && is_running) {
                return true;
            }
        }
    }
    return false;
}

} // namespace MayaFlux::Core
