#pragma once

#include "config.h"

namespace MayaFlux {

namespace Core {
    struct GlobalStreamInfo;
    class Engine;
    struct AudioBuffer;
    class BufferManager;

    namespace Scheduler {
        class TaskScheduler;
        class SoundRoutine;
    }
}

namespace Nodes {
    class Node;
    class NodeGraphManager;
    class RootNode;
}

namespace Tasks {
    class ActionToken;
}

using AudioProcessingFunction = std::function<void(Core::AudioBuffer&, unsigned int)>;

//-------------------------------------------------------------------------
// Engine Management
//-------------------------------------------------------------------------

bool is_engine_initialized();

Core::Engine* get_context();

void set_context(Core::Engine& instance);

void Init(u_int32_t sample_rate = 48000, u_int32_t buffer_size = 512, u_int32_t num_out_channels = 2);

void Init(Core::GlobalStreamInfo stream_info);

void Start();

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

void schedule_task(std::string name, Core::Scheduler::SoundRoutine&& task);

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

void add_processor(AudioProcessingFunction processor);

void add_processor(AudioProcessingFunction processor, const std::vector<unsigned int>& channels);

void clear_processors();

void process_buffer(std::vector<double>& buffer, unsigned int num_frames);

//-------------------------------------------------------------------------
// Buffer Management
//-------------------------------------------------------------------------

void clear_buffer(Core::AudioBuffer& buffer);

std::vector<double>& get_buffer_data(Core::AudioBuffer& buffer);

Core::AudioBuffer& get_channel(std::shared_ptr<Core::BufferManager> manager, unsigned int channel);

void process_channels(std::shared_ptr<Core::BufferManager> manager, AudioProcessingFunction processor);

//-------------------------------------------------------------------------
// Node Graph Management
//-------------------------------------------------------------------------

void add_node_to_root(std::shared_ptr<Nodes::Node> node, unsigned int channel = 0);

void remove_node_from_root(std::shared_ptr<Nodes::Node> node, unsigned int channel = 0);

void connect_nodes(std::string& source, std::string& target);

void connect_nodes(std::shared_ptr<Nodes::Node> source, std::shared_ptr<Nodes::Node> target);

Nodes::RootNode get_root_node(u_int32_t channel = 0);

}
