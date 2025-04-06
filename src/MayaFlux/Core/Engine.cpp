#include "Engine.hpp"
#include "AudioCallback.hpp"
#include "MayaFlux/Nodes/Generators/Stochastic.hpp"
#include "MayaFlux/Nodes/NodeGraphManager.hpp"

namespace MayaFlux::Core {

Engine::Engine()
    : m_Context(std::make_unique<RtAudio>())
    , m_Device(std::make_shared<Device>(get_handle()))
    , m_rng(new Nodes::Generator::Stochastics::NoiseEngine())
{
}

Engine::~Engine()
{
    End();
}

void Engine::Init(GlobalStreamInfo stream_info)
{
    m_StreamSettings = std::make_shared<Stream>(m_Device->get_default_output_device(), stream_info);
    MayaFlux::set_context(*this);

    m_scheduler = Scheduler::TaskScheduler(stream_info.sample_rate);

    if (MayaFlux::graph_manager == nullptr) {
        MayaFlux::graph_manager = new Nodes::NodeGraphManager();
    }
}

void Engine::Start()
{
    auto stream_info = m_StreamSettings->get_global_stream_info();
    RtAudio::StreamParameters output_params = m_StreamSettings->get_stream_parameters();
    RtAudio::StreamOptions options = m_StreamSettings->get_stream_options();

    try {
        m_Context->openStream(
            &output_params,
            nullptr,
            RTAUDIO_FLOAT64,
            stream_info.sample_rate,
            &stream_info.buffer_size,
            rtaudio_callback,
            this,
            &options);

        m_Context->startStream();
    } catch (const RtAudioErrorType& e) {
        throw std::runtime_error("Failed to open RtAudio stream");
    }
}

void Engine::End()
{
    if (m_Context->isStreamRunning()) {
        try {
            m_Context->stopStream();
        } catch (const RtAudioErrorType& e) {
            throw std::runtime_error("Could not stop stream");
        }
    }

    if (m_Context->isStreamOpen()) {
        m_Context->closeStream();
    }

    m_named_tasks.clear();
}

int Engine::process_input(double* input_buffer, unsigned int num_frames)
{
    return 0;
}

int Engine::process_output(double* output_buffer, unsigned int num_frames)
{
    m_scheduler.process_buffer(num_frames);

    unsigned int num_channels = m_StreamSettings->get_global_stream_info().num_channels;

    for (unsigned int channel = 0; channel < num_channels; channel++) {
        auto& channel_root = MayaFlux::get_node_graph_manager().get_root_node(channel);

        std::vector<double> channel_data = channel_root.process(num_frames);

        for (unsigned int i = 0; i < num_frames; i++) {
            output_buffer[i * num_channels + channel] = channel_data[i];
        }
    }

    execute_processing_chain(nullptr, output_buffer, num_frames);

    return 0;
}

int Engine::process_audio(double* input_buffer, double* output_buffer, unsigned int num_frames)
{
    process_input(input_buffer, num_frames);
    process_output(output_buffer, num_frames);

    execute_processing_chain(input_buffer, output_buffer, num_frames);

    return 0;
}

void Engine::execute_processing_chain(double* input_buffer, double* output_buffer, unsigned int num_frames)
{
    unsigned int num_channels = m_StreamSettings->get_global_stream_info().num_channels;

    if (m_Processing_chain.empty()) {
        return;
    }

    for (auto& processor : m_Processing_chain) {
        std::vector<double> temp_buffer(num_frames * num_channels);

        std::copy(output_buffer, output_buffer + num_frames * num_channels, temp_buffer.data());

        processor(input_buffer, temp_buffer.data(), num_frames, num_channels);

        std::copy(temp_buffer.data(), temp_buffer.data() + num_frames * num_channels, output_buffer);
    }
}

void Engine::add_processor(AudioProcessingFunction processor)
{
    m_Processing_chain.push_back(processor);
}

void Engine::clear_processors()
{
    m_Processing_chain.clear();
}

void Engine::process_buffer(std::vector<double>& buffer, unsigned int num_frames)
{
    unsigned int num_channels = m_StreamSettings->get_global_stream_info().num_channels;
    std::vector<std::vector<double>> channel_buffers(num_channels);

    for (unsigned int channel = 0; channel < num_channels; channel++) {
        auto& channel_root = MayaFlux::get_node_graph_manager().get_root_node(channel);
        channel_buffers[channel] = channel_root.process(num_frames);
    }

    buffer.resize(num_frames * num_channels);

    for (unsigned int i = 0; i < num_frames; i++) {
        for (unsigned int channel = 0; channel < num_channels; channel++) {
            buffer[i * num_channels + channel] = channel_buffers[channel][i];
        }
    }

    if (!m_Processing_chain.empty() && num_frames > 0) {
        double* buffer_ptr = buffer.data();
        execute_processing_chain(nullptr, buffer_ptr, num_frames);
    }
}

Scheduler::SoundRoutine Engine::schedule_metro(double interval_seconds, std::function<void()> callback)
{
    return metro(m_scheduler, interval_seconds, callback);
}
Scheduler::SoundRoutine Engine::schedule_sequence(std::vector<std::pair<double, std::function<void()>>> seq)
{
    return sequence(m_scheduler, seq);
}

Scheduler::SoundRoutine Engine::create_line(float start_value, float end_value, float duration_seconds, float step_duration, bool loop)
{
    return line(m_scheduler, start_value, end_value, duration_seconds, step_duration, loop);
}

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

    m_scheduler.add_task(std::move(task));

    auto& tasks = m_scheduler.get_tasks();
    if (!tasks.empty()) {
        m_named_tasks[name] = &tasks.back();
    }
}

bool Engine::cancel_task(const std::string& name)
{
    auto it = m_named_tasks.find(name);
    if (it != m_named_tasks.end()) {
        if (m_scheduler.cancel_task(it->second)) {
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

double Engine::get_uniform_random(double start, double end)
{
    m_rng->set_type(Utils::distribution::UNIFORM);
    return m_rng->random_sample(start, end);
}

double Engine::get_gaussian_random(double start, double end)
{
    m_rng->set_type(Utils::distribution::NORMAL);
    return m_rng->random_sample(start, end);
}

double Engine::get_exponential_random(double start, double end)
{
    m_rng->set_type(Utils::distribution::EXPONENTIAL);
    return m_rng->random_sample(start, end);
}

double Engine::get_poisson_random(double start, double end)
{
    m_rng->set_type(Utils::distribution::POISSON);
    return m_rng->random_sample(start, end);
}
}
