#pragma once

#include "config.h"

namespace MayaFlux {

namespace Core {
    struct GlobalStreamInfo;
    class Engine;
    class BufferManager;

    namespace Scheduler {
        class TaskScheduler;
        class SoundRoutine;
    }
}

namespace Buffers {
    class AudioBuffer;
}

namespace Nodes {
    class Node;
    class NodeGraphManager;
    class RootNode;
}

namespace Tasks {
    class ActionToken;
}

using AudioProcessingFunction = std::function<void(std::shared_ptr<Buffers::AudioBuffer>)>;

//-------------------------------------------------------------------------
// Engine Management
//-------------------------------------------------------------------------

bool is_engine_initialized();

Core::Engine* get_context();

void set_context(Core::Engine* instance);

void Init(u_int32_t sample_rate = 48000, u_int32_t buffer_size = 512, u_int32_t num_out_channels = 2);

void Init(Core::GlobalStreamInfo stream_info);

void Start();

void Pause();

void Resume();

void End();

//-------------------------------------------------------------------------
// Configuration Access
//-------------------------------------------------------------------------

Core::GlobalStreamInfo get_global_stream_info();

u_int32_t get_sample_rate();

u_int32_t get_buffer_size();

u_int32_t get_num_out_channels();

//-------------------------------------------------------------------------
// Component Access
//-------------------------------------------------------------------------

std::shared_ptr<Core::Scheduler::TaskScheduler> get_scheduler();

std::shared_ptr<Core::BufferManager> get_buffer_manager();

std::shared_ptr<Nodes::NodeGraphManager> get_node_graph_manager();

//-------------------------------------------------------------------------
// Random Number Generation
//-------------------------------------------------------------------------

double get_uniform_random(double start = 0, double end = 1);

double get_gaussian_random(double start = 0, double end = 1);

double get_exponential_random(double start = 0, double end = 1);

double get_poisson_random(double start = 0, double end = 1);

//-------------------------------------------------------------------------
// Task Scheduling
//-------------------------------------------------------------------------

Core::Scheduler::SoundRoutine schedule_metro(double interval_seconds, std::function<void()> callback);

Core::Scheduler::SoundRoutine schedule_sequence(std::vector<std::pair<double, std::function<void()>>> sequence);

Core::Scheduler::SoundRoutine create_line(float start_value, float end_value, float duration_seconds, float step_duration, bool loop);

Core::Scheduler::SoundRoutine schedule_pattern(std::function<std::any(u_int64_t)> pattern_func, std::function<void(std::any)> callback, double interval_seconds);

float* get_line_value(const std::string& name);

std::function<float()> line_value(const std::string& name);

void schedule_task(std::string name, Core::Scheduler::SoundRoutine&& task, bool initialize = false);

bool cancel_task(const std::string& name);

bool restart_task(const std::string& name);

template <typename... Args>
bool update_task_params(const std::string& name, Args... args);

Tasks::ActionToken Play(std::shared_ptr<Nodes::Node> node);

Tasks::ActionToken Wait(double seconds);

Tasks::ActionToken Action(std::function<void()> func);

//-------------------------------------------------------------------------
// Audio Processing
//-------------------------------------------------------------------------

void attach_quick_process(AudioProcessingFunction processor, std::shared_ptr<Buffers::AudioBuffer> buffer);

void attach_quick_process_to_channel(AudioProcessingFunction processor, unsigned int channel_id = 0);

void attach_quick_process_to_channels(AudioProcessingFunction processor, const std::vector<unsigned int> channels);

//-------------------------------------------------------------------------
// Buffer Management
//-------------------------------------------------------------------------

Buffers::AudioBuffer& get_channel(unsigned int channel);

void connect_node_to_channel(std::shared_ptr<Nodes::Node> node, u_int32_t channel_index = 0, float mix = 0.5f, bool clear_before = false);

void connect_node_to_buffer(std::shared_ptr<Nodes::Node> node, std::shared_ptr<Buffers::AudioBuffer> buffer, float mix = 0.5f, bool clear_before = true);

//-------------------------------------------------------------------------
// Node Graph Management
//-------------------------------------------------------------------------

void add_node_to_root(std::shared_ptr<Nodes::Node> node, unsigned int channel = 0);

void remove_node_from_root(std::shared_ptr<Nodes::Node> node, unsigned int channel = 0);

void connect_nodes(std::string& source, std::string& target);

void connect_nodes(std::shared_ptr<Nodes::Node> source, std::shared_ptr<Nodes::Node> target);

Nodes::RootNode get_root_node(u_int32_t channel = 0);

}
