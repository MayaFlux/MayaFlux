#include "Engine.hpp"
#include "MayaFlux/Buffers/BufferManager.hpp"
#include "MayaFlux/Nodes/Generators/Stochastic.hpp"
#include "MayaFlux/Nodes/NodeGraphManager.hpp"

namespace MayaFlux::Core {

//-------------------------------------------------------------------------
// Initialization and Lifecycle
//-------------------------------------------------------------------------

Engine::Engine(Utils::AudioBackendType audio_type)
    : m_audiobackend(AudioBackendFactory::create_backend(audio_type))
    , m_audio_device(m_audiobackend->create_device_manager())
    , m_rng(new Nodes::Generator::Stochastics::NoiseEngine())
    , m_is_paused(false)
{
}

Engine::~Engine()
{
    End();
    delete m_rng;
}

Engine::Engine(Engine&& other) noexcept
    : m_audiobackend(std::move(other.m_audiobackend))
    , m_audio_device(std::move(other.m_audio_device))
    , m_audio_stream(std::move(other.m_audio_stream))
    , m_stream_info(other.m_stream_info)
    , m_rng(std::exchange(other.m_rng, nullptr))
    , m_is_paused(other.m_is_paused)
    , m_scheduler(std::move(other.m_scheduler))
    , m_node_graph_manager(std::move(other.m_node_graph_manager))
    , m_Buffer_manager(std::move(other.m_Buffer_manager))
    , m_named_tasks(std::move(other.m_named_tasks))
{
}

Engine& Engine::operator=(Engine&& other) noexcept
{
    if (this != &other) {
        End();

        m_audiobackend = std::move(other.m_audiobackend);
        m_audio_device = std::move(other.m_audio_device);
        m_audio_stream = std::move(other.m_audio_stream);
        m_stream_info = other.m_stream_info;
        delete m_rng;
        m_rng = std::exchange(other.m_rng, nullptr);
        m_is_paused = other.m_is_paused;
        m_scheduler = std::move(other.m_scheduler);
        m_node_graph_manager = std::move(other.m_node_graph_manager);
        m_Buffer_manager = std::move(other.m_Buffer_manager);
        m_named_tasks = std::move(other.m_named_tasks);
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

    m_audio_stream = m_audiobackend->create_stream(
        m_audio_device->get_default_output_device(),
        m_stream_info,
        this);

    m_audio_stream->set_process_callback(
        [this](void* output_buffer, void* input_buffer, unsigned int num_frames) -> int {
            if (input_buffer && output_buffer) {
                return this->process_audio(static_cast<double*>(input_buffer), static_cast<double*>(output_buffer), num_frames);
            } else if (output_buffer) {
                return this->process_output(static_cast<double*>(output_buffer), num_frames);
            } else if (input_buffer) {
                return this->process_input(static_cast<double*>(input_buffer), num_frames);
            }
            return 0;
        });

    m_scheduler = std::make_shared<Vruta::TaskScheduler>(m_stream_info.sample_rate);
    m_Buffer_manager = std::make_shared<Buffers::BufferManager>(
        m_stream_info.output.channels,
        m_stream_info.buffer_size);
    m_node_graph_manager = std::make_shared<Nodes::NodeGraphManager>();
}

void Engine::Start()
{
    if (!m_audio_stream) {
        throw std::runtime_error("Cannot start stream: stream not initialized");
    }

    m_audio_stream->open();
    m_audio_stream->start();
}

void Engine::Pause()
{
    if (m_audio_stream && m_audio_stream->is_running()) {
        m_audio_stream->stop();
    } else {
        return;
    }

    for (auto& [name, task] : m_named_tasks) {
        if (task && task->is_active()) {
            bool current_auto_resume = task->get_handle().promise().auto_resume;
            task->set_state<bool>("was_auto_resume", current_auto_resume);

            task->get_handle().promise().auto_resume = false;
        }
    }
    m_is_paused = true;
}

void Engine::Resume()
{
    if (!m_is_paused) {
        return;
    }
    m_is_paused = false;

    for (auto& [name, task] : m_named_tasks) {
        if (task && task->is_active()) {
            bool* was_auto_resume = task->get_state<bool>("was_auto_resume");
            if (was_auto_resume) {
                task->get_handle().promise().auto_resume = *was_auto_resume;
            } else {
                task->get_handle().promise().auto_resume = true;
            }
        }
    }

    if (m_audio_stream && !m_audio_stream->is_running() && m_audio_stream->is_open()) {
        m_audio_stream->start();
    }
}

void Engine::End()
{
    for (auto& [name, task] : m_named_tasks) {
        if (task && task->is_active()) {
            task->get_handle().promise().should_terminate = true;

            task->get_handle().promise().auto_resume = true;
            task->get_handle().promise().next_sample = 0;
        }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    if (m_audio_stream) {
        try {
            if (m_audio_stream->is_running()) {
                m_audio_stream->stop();

                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }

            if (m_audio_stream->is_open()) {
                m_audio_stream->close();
            }
        } catch (const std::exception& e) {
            std::cerr << "Exception during audio stream shutdown: " << e.what() << std::endl;
        }
    }

    for (auto& [name, task] : m_named_tasks) {
        if (task && task->is_active()) {
            m_scheduler->cancel_task(task);
        }
    }
    m_named_tasks.clear();

    if (m_Buffer_manager && m_node_graph_manager) {
        for (unsigned int i = 0; i < m_stream_info.output.channels; i++) {
            auto channel_buffer = m_Buffer_manager->get_channel(i);
            if (channel_buffer) {
                channel_buffer->clear();

                if (auto root_buffer = std::dynamic_pointer_cast<Buffers::RootAudioBuffer>(channel_buffer)) {
                    for (auto& child : root_buffer->get_child_buffers()) {
                        child->clear();
                    }
                }
            }

            m_node_graph_manager->get_root_node(i).clear_all_nodes();
        }
    }
}

bool Engine::is_running() const
{
    if (!m_audio_stream) {
        return false;
    }
    return m_audio_stream->is_running();
}

//-------------------------------------------------------------------------
// Audio Processing
//-------------------------------------------------------------------------

int Engine::process_input(double* input_buffer, unsigned int num_frames)
{
    return 0;
}

int Engine::process_output(double* output_buffer, unsigned int num_frames)
{
    for (const auto& [name, hook] : m_pre_process_hooks) {
        hook(num_frames);
    }

    m_scheduler->process_buffer(num_frames);

    unsigned int num_channels = m_stream_info.output.channels;

    if (m_Buffer_manager->get_num_frames() < num_frames) {
        m_Buffer_manager->resize(num_frames);
    }

    for (unsigned int channel = 0; channel < num_channels; channel++) {
        auto& channel_root = get_node_graph_manager()->get_root_node(channel);
        std::vector<double> channel_data = channel_root.process(num_frames);

        auto root_buffer = std::dynamic_pointer_cast<Buffers::RootAudioBuffer>(
            m_Buffer_manager->get_channel(channel));
        root_buffer->set_node_output(channel_data);

        m_Buffer_manager->process_channel(channel);
    }

    for (const auto& [name, hook] : m_post_process_hooks) {
        hook(num_frames);
    }

    m_Buffer_manager->fill_interleaved(output_buffer, num_frames);

    return 0;
}

int Engine::process_audio(double* input_buffer, double* output_buffer, unsigned int num_frames)
{
    for (const auto& [name, hook] : m_pre_process_hooks) {
        hook(num_frames);
    }

    process_input(input_buffer, num_frames);
    process_output(output_buffer, num_frames);

    for (const auto& [name, hook] : m_post_process_hooks) {
        hook(num_frames);
    }

    return 0;
}

//-------------------------------------------------------------------------
// Task Scheduling
//-------------------------------------------------------------------------

float* Engine::get_line_value(const std::string& name)
{
    auto it = m_named_tasks.find(name);
    if (it != m_named_tasks.end() && it->second->is_active()) {
        auto& promise = it->second->get_handle().promise();
        float* value = promise.get_state<float>("current_value");
        if (value) {
            return value;
        }
    }
    return nullptr;
}

std::function<float()> Engine::line_value(const std::string& name)
{
    return [this, name]() -> float {
        auto it = m_named_tasks.find(name);
        if (it != m_named_tasks.end() && it->second->is_active()) {
            auto& promise = it->second->get_handle().promise();
            float* value = promise.get_state<float>("current_value");
            if (value) {
                return *value;
            }
        }
        return 0.0f;
    };
}

void Engine::schedule_task(std::string name, Vruta::SoundRoutine&& task, bool initialize)
{
    cancel_task(name);

    auto task_ptr = std::make_shared<Vruta::SoundRoutine>(std::move(task));

    m_scheduler->add_task(task_ptr, initialize);

    m_named_tasks[name] = task_ptr;
}

bool Engine::cancel_task(const std::string& name)
{
    auto it = m_named_tasks.find(name);
    if (it != m_named_tasks.end()) {
        if (m_scheduler->cancel_task(it->second)) {
            m_named_tasks.erase(it);
            return true;
        }
    }
    return false;
}

bool Engine::restart_task(const std::string& name)
{
    auto it = m_named_tasks.find(name);
    if (it != m_named_tasks.end()) {
        if (it->second) {
            return it->second->restart();
        }
    }
    return false;
}

//-------------------------------------------------------------------------
// Process Hook Registration
//-------------------------------------------------------------------------

void Engine::register_process_hook(const std::string& name, ProcessHook hook, HookPosition position)
{
    if (position == HookPosition::PRE_PROCESS) {
        m_pre_process_hooks[name] = std::move(hook);
    } else {
        m_post_process_hooks[name] = std::move(hook);
    }
}

void Engine::unregister_process_hook(const std::string& name)
{
    auto pre_it = m_pre_process_hooks.find(name);
    if (pre_it != m_pre_process_hooks.end()) {
        m_pre_process_hooks.erase(pre_it);
        return;
    }

    auto post_it = m_post_process_hooks.find(name);
    if (post_it != m_post_process_hooks.end()) {
        m_post_process_hooks.erase(post_it);
    }
}

bool Engine::has_process_hook(const std::string& name) const
{
    return m_pre_process_hooks.find(name) != m_pre_process_hooks.end() || m_post_process_hooks.find(name) != m_post_process_hooks.end();
}

} // namespace MayaFlux::Core
