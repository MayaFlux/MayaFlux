#include "Engine.hpp"
#include "Device.hpp"
#include "MayaFlux/Buffers/BufferManager.hpp"
#include "MayaFlux/Nodes/Generators/Stochastic.hpp"
#include "MayaFlux/Nodes/NodeGraphManager.hpp"
#include "Stream.hpp"

namespace MayaFlux::Core {

//-------------------------------------------------------------------------
// Initialization and Lifecycle
//-------------------------------------------------------------------------

Engine::Engine()
    : m_Context(std::make_unique<RtAudio>())
    , m_Device(std::make_unique<Device>(get_handle()))
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
    : m_Context(std::move(other.m_Context))
    , m_Device(std::move(other.m_Device))
    , m_Stream_manager(std::move(other.m_Stream_manager))
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

        m_Context = std::move(other.m_Context);
        m_Device = std::move(other.m_Device);
        m_Stream_manager = std::move(other.m_Stream_manager);
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

void Engine::Init(GlobalStreamInfo stream_info)
{
    m_stream_info = stream_info;
    m_Stream_manager = std::make_unique<Stream>(m_Device->get_default_output_device(), this);
    m_scheduler = std::make_shared<Vruta::TaskScheduler>(stream_info.sample_rate);
    m_Buffer_manager = std::make_shared<Buffers::BufferManager>(stream_info.num_channels, stream_info.buffer_size);
    m_node_graph_manager = std::make_shared<Nodes::NodeGraphManager>();
}

void Engine::Start()
{
    m_Stream_manager->open();
    m_Stream_manager->start();
}

void Engine::Pause()
{
    if (m_Stream_manager && m_Stream_manager->is_running()) {
        m_Stream_manager->stop();
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

    if (m_Stream_manager && !m_Stream_manager->is_running() && m_Stream_manager->is_open()) {
        m_Stream_manager->start();
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

    if (m_Stream_manager) {
        if (m_Stream_manager->is_running()) {
            m_Stream_manager->stop();
        }
        if (m_Stream_manager->is_open()) {
            m_Stream_manager->close();
        }
    }

    for (auto& [name, task] : m_named_tasks) {
        if (task && task->is_active()) {
            m_scheduler->cancel_task(task);
        }
    }
    m_named_tasks.clear();

    if (m_Buffer_manager && m_node_graph_manager) {
        for (unsigned int i = 0; i < m_stream_info.num_channels; i++) {
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
    if (!m_Stream_manager) {
        return false;
    }
    return m_Stream_manager->is_running();
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

    unsigned int num_channels = m_stream_info.num_channels;

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
