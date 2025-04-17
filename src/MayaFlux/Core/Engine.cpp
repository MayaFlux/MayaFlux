#include "Engine.hpp"
#include "BufferManager.hpp"
#include "Device.hpp"
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
{
}

Engine::~Engine()
{
    End();
    delete m_rng;
}

void Engine::Init(GlobalStreamInfo stream_info)
{
    m_stream_info = stream_info;
    m_Stream_manager = std::make_unique<Stream>(m_Device->get_default_output_device(), this);
    m_scheduler = std::make_shared<Scheduler::TaskScheduler>(stream_info.sample_rate);
    m_Buffer_manager = std::make_shared<BufferManager>(stream_info.num_channels, stream_info.buffer_size);
    m_node_graph_manager = std::make_shared<Nodes::NodeGraphManager>();
}

void Engine::Start()
{
    m_Stream_manager->open();
    m_Stream_manager->start();
}

void Engine::End()
{
    if (m_Stream_manager) {
        m_Stream_manager->stop();
        m_Stream_manager->close();
    }

    m_named_tasks.clear();
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

    m_Buffer_manager->fill_interleaved(output_buffer, num_frames);

    return 0;
}

int Engine::process_audio(double* input_buffer, double* output_buffer, unsigned int num_frames)
{
    process_input(input_buffer, num_frames);
    process_output(output_buffer, num_frames);

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

void Engine::schedule_task(std::string name, Scheduler::SoundRoutine&& task)
{
    cancel_task(name);

    auto task_ptr = std::make_shared<Scheduler::SoundRoutine>(std::move(task));

    m_scheduler->add_task(task_ptr);

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

} // namespace MayaFlux::Core
