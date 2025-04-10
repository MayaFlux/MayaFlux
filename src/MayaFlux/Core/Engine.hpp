#pragma once

#include "Buffer.hpp"
#include "MayaFlux/Core/Scheduler/Scheduler.hpp"

namespace MayaFlux::Nodes {
class NodeGraphManager;
namespace Generator::Stochastics {
    class NoiseEngine;
}
}

namespace MayaFlux::Core {

class Device;
class Stream;

struct GlobalStreamInfo {
    unsigned int sample_rate = 48000;
    unsigned int buffer_size = 512;
    unsigned int num_channels = 2;
};

using AudioProcessingFunction = std::function<void(AudioBuffer&, unsigned int)>;

class Engine {
public:
    //-------------------------------------------------------------------------
    // Initialization and Lifecycle
    //-------------------------------------------------------------------------

    Engine();

    ~Engine();

    void Init(GlobalStreamInfo stream_info = GlobalStreamInfo { 48000, 512, 2 });

    void Start();

    void End();

    //-------------------------------------------------------------------------
    // Configuration Access
    //-------------------------------------------------------------------------

    inline GlobalStreamInfo& get_stream_info() { return m_stream_info; }

    inline const Stream* get_stream_manager() const { return m_Stream_manager.get(); }

    RtAudio* get_handle() { return m_Context.get(); }

    //-------------------------------------------------------------------------
    // Component Access
    //-------------------------------------------------------------------------

    inline std::shared_ptr<Nodes::NodeGraphManager> get_node_graph_manager() { return m_node_graph_manager; }

    inline std::shared_ptr<Scheduler::TaskScheduler> get_scheduler() { return m_scheduler; }

    inline std::shared_ptr<BufferManager> get_buffer_manager() { return m_Buffer_manager; }

    inline Nodes::Generator::Stochastics::NoiseEngine* get_random_engine() { return m_rng; }

    //-------------------------------------------------------------------------
    // Audio Processing
    //-------------------------------------------------------------------------

    int process_input(double* input_buffer, unsigned int num_frames);

    int process_output(double* output_buffer, unsigned int num_frames);

    int process_audio(double* input_buffer, double* output_buffer, unsigned int num_frames);

    void add_processor(AudioProcessingFunction processor);

    void add_processor(AudioProcessingFunction processor, const std::vector<unsigned int>& channels);

    void clear_processors();

    void process_buffer(std::vector<double>& buffer, unsigned int num_frames);

    //-------------------------------------------------------------------------
    // Task Scheduling
    //-------------------------------------------------------------------------

    float* get_line_value(const std::string& name);

    std::function<float()> line_value(const std::string& name);

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

private:
    //-------------------------------------------------------------------------
    // Audio System Components
    //-------------------------------------------------------------------------

    std::unique_ptr<RtAudio> m_Context;
    std::unique_ptr<Device> m_Device;
    std::unique_ptr<Stream> m_Stream_manager;
    GlobalStreamInfo m_stream_info;

    //-------------------------------------------------------------------------
    // Core Components
    //-------------------------------------------------------------------------

    std::shared_ptr<Scheduler::TaskScheduler> m_scheduler;
    std::shared_ptr<Nodes::NodeGraphManager> m_node_graph_manager;
    std::shared_ptr<BufferManager> m_Buffer_manager;
    Nodes::Generator::Stochastics::NoiseEngine* m_rng;

    //-------------------------------------------------------------------------
    // Task Management
    //-------------------------------------------------------------------------

    std::unordered_map<std::string, std::shared_ptr<Scheduler::SoundRoutine>> m_named_tasks;

    //-------------------------------------------------------------------------
    // Audio Processing
    //-------------------------------------------------------------------------

    struct ProcessorInfo {
        AudioProcessingFunction processor;
        std::vector<unsigned int> channels;
    };

    std::vector<ProcessorInfo> m_Processing_chain;

    void execute_processing_chain();
};

} // namespace MayaFlux::Core
