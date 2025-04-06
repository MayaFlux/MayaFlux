#pragma once

#include "MayaFlux/Core/Scheduler/Tasks.hpp"
#include "MayaFlux/MayaFlux.hpp"

#include "Buffer.hpp"
#include "Device.hpp"
#include "Stream.hpp"

namespace MayaFlux::Nodes::Generator::Stochastics {
class NoiseEngine;
}

namespace MayaFlux::Core {

using AudioProcessingFunction = std::function<void(AudioBuffer&, unsigned int)>;

class Engine {
public:
    Engine();
    ~Engine();

    void Init(GlobalStreamInfo stream_info);

    inline const std::shared_ptr<Stream> get_stream_settings() const
    {
        return m_StreamSettings;
    }

    void Start();
    void End();

    int process_input(double* input_buffer, unsigned int num_frames);
    int process_output(double* output_buffer, unsigned int num_frames);
    int process_audio(double* input_buffer, double* output_buffer, unsigned int num_frames);

    void add_processor(AudioProcessingFunction processor);

    void add_processor(AudioProcessingFunction processor, const std::vector<unsigned int>& channels);

    void clear_processors();

    void process_buffer(std::vector<double>& buffer, unsigned int num_frames);

    Scheduler::SoundRoutine schedule_metro(double interval_seconds, std::function<void()> callback);
    Scheduler::SoundRoutine schedule_sequence(std::vector<std::pair<double, std::function<void()>>> sequence);
    Scheduler::SoundRoutine create_line(float start_value, float end_value, float duration_seconds, float step_duration, bool loop);

    float* get_line_value(const std::string& name);

    std::function<float()> line_value(const std::string& name);

    template <typename T>
    inline Scheduler::SoundRoutine schedule_pattern(std::function<T(u_int64_t)> pattern_func, std::function<void(T)> callback, double interval_seconds)
    {
        return pattern<T>(m_scheduler, pattern_func, callback, interval_seconds);
    }

    void schedule_task(std::string name, Scheduler::SoundRoutine&& task);
    bool cancel_task(const std::string& name);

    bool restart_task(const std::string& name);

    template <typename... Args>
    inline bool update_task_params(const std::string& name, Args... args)
    {
        auto it = m_named_tasks.find(name);
        if (it != m_named_tasks.end() && it->second->is_active()) {
            it->second->update_params(std::forward<Args>(args)...);
            return true;
        }
        return false;
    }

    inline Scheduler::TaskScheduler& get_scheduler() { return m_scheduler; }

    double get_uniform_random(double start = 0, double end = 1);
    double get_gaussian_random(double start = 0, double end = 1);
    double get_exponential_random(double start = 0, double end = 1);
    double get_poisson_random(double start = 0, double end = 1);

private:
    std::unique_ptr<RtAudio> m_Context;
    std::shared_ptr<Device> m_Device;
    std::shared_ptr<Stream> m_StreamSettings;
    Scheduler::TaskScheduler m_scheduler;
    std::unordered_map<std::string, std::shared_ptr<Scheduler::SoundRoutine>> m_named_tasks;

    struct ProcessorInfo {
        AudioProcessingFunction processor;
        std::vector<unsigned int> channels;
    };

    std::vector<ProcessorInfo> m_Processing_chain;
    std::unique_ptr<BufferManager> m_Buffer_manager;

    void execute_processing_chain();

    RtAudio* get_handle()
    {
        return m_Context.get();
    }

    Nodes::Generator::Stochastics::NoiseEngine* m_rng;
};
}
