#include "MayaFlux.hpp"

#include "Core/Engine.hpp"
#include "MayaFlux/Buffers/BufferManager.hpp"
#include "MayaFlux/Nodes/Generators/Stochastic.hpp"
#include "Nodes/NodeGraphManager.hpp"
#include "Tasks/Chain.hpp"

namespace MayaFlux {

//-------------------------------------------------------------------------
// Internal Functions and Data
//-------------------------------------------------------------------------

namespace internal {

    std::unique_ptr<Core::Engine> engine_ref;
    bool initialized = false;
    std::mutex engine_mutex;

    void cleanup_engine()
    {
        std::lock_guard<std::mutex> lock(engine_mutex);
        if (engine_ref) {
            if (initialized) {
                if (engine_ref->is_running()) {
                    engine_ref->Pause();
                }
                engine_ref->End();
            }
            engine_ref.reset();
            initialized = false;
        }
    }

    Core::Engine* get_or_create_engine()
    {
        std::lock_guard<std::mutex> lock(engine_mutex);
        if (!engine_ref) {
            engine_ref = std::make_unique<Core::Engine>();
            engine_ref->Init();
            initialized = true;
            std::atexit(cleanup_engine);
        }
        return engine_ref.get();
    }

}

//-------------------------------------------------------------------------
// Engine Management
//-------------------------------------------------------------------------

bool is_engine_initialized()
{
    return internal::initialized;
}

Core::Engine* get_context()
{
    return internal::get_or_create_engine();
}

void set_context(Core::Engine* instance)
{
    if (!instance) {
        throw std::invalid_argument("Cannot set null Engine context");
    }

    bool is_same_instance = false;
    {
        std::lock_guard<std::mutex> lock(internal::engine_mutex);
        is_same_instance = (instance == internal::engine_ref.get());
        if (internal::engine_ref && !is_same_instance && internal::engine_ref->is_running()) {
            internal::engine_ref->Pause();
        }
    }

    if (internal::engine_ref && !is_same_instance) {
        internal::engine_ref->End();
        internal::engine_ref.reset();
    }

    if (!is_same_instance) {
        std::lock_guard<std::mutex> lock(internal::engine_mutex);
        internal::engine_ref.reset(instance);
        instance = nullptr;
        internal::initialized = true;
    }
}

void Init(unsigned int sample_rate, unsigned int buffer_size, unsigned int num_out_channels)
{
    auto* engine = internal::get_or_create_engine();
    engine->Init({ sample_rate, buffer_size, num_out_channels });
}

void Init(Core::GlobalStreamInfo stream_info)
{
    auto* engine = internal::get_or_create_engine();
    engine->Init(stream_info);
}

void Start()
{
    get_context()->Start();
}

void Pause()
{
    if (internal::initialized) {
        get_context()->Pause();
    }
}

void Resume()
{
    if (internal::initialized) {
        get_context()->Resume();
    }
}

void End()
{
    if (internal::initialized) {
        // get_context()->End();
        internal::cleanup_engine();
    }
}

//-------------------------------------------------------------------------
// Configuration Access
//-------------------------------------------------------------------------

Core::GlobalStreamInfo get_global_stream_info()
{
    return get_context()->get_stream_info();
}

u_int32_t get_sample_rate()
{
    return get_global_stream_info().sample_rate;
}

u_int32_t get_buffer_size()
{
    return get_global_stream_info().buffer_size;
}

u_int32_t get_num_out_channels()
{
    return get_global_stream_info().num_channels;
}

//-------------------------------------------------------------------------
// Component Access
//-------------------------------------------------------------------------

std::shared_ptr<Core::Scheduler::TaskScheduler> get_scheduler()
{
    return get_context()->get_scheduler();
}

std::shared_ptr<Buffers::BufferManager> get_buffer_manager()
{
    return get_context()->get_buffer_manager();
}

std::shared_ptr<Nodes::NodeGraphManager> get_node_graph_manager()
{
    return get_context()->get_node_graph_manager();
}

//-------------------------------------------------------------------------
// Random Number Generation
//-------------------------------------------------------------------------

double get_uniform_random(double start, double end)
{
    get_context()->get_random_engine()->set_type(Utils::distribution::UNIFORM);
    return get_context()->get_random_engine()->random_sample(start, end);
}

double get_gaussian_random(double start, double end)
{
    get_context()->get_random_engine()->set_type(Utils::distribution::NORMAL);
    return get_context()->get_random_engine()->random_sample(start, end);
}

double get_exponential_random(double start, double end)
{
    get_context()->get_random_engine()->set_type(Utils::distribution::EXPONENTIAL);
    return get_context()->get_random_engine()->random_sample(start, end);
}

double get_poisson_random(double start, double end)
{
    get_context()->get_random_engine()->set_type(Utils::distribution::POISSON);
    return get_context()->get_random_engine()->random_sample(start, end);
}

//-------------------------------------------------------------------------
// Task Scheduling
//-------------------------------------------------------------------------

template <typename... Args>
bool update_task_params(const std::string& name, Args... args)
{
    return get_context()->update_task_params(name, args...);
}

Core::Scheduler::SoundRoutine schedule_metro(double interval_seconds, std::function<void()> callback)
{
    return Tasks::metro(*get_scheduler(), interval_seconds, callback);
}

Core::Scheduler::SoundRoutine schedule_sequence(std::vector<std::pair<double, std::function<void()>>> seq)
{
    return Tasks::sequence(*get_scheduler(), seq);
}

Core::Scheduler::SoundRoutine create_line(float start_value, float end_value, float duration_seconds, float step_duration, bool loop)
{
    return Tasks::line(*get_scheduler(), start_value, end_value, duration_seconds, step_duration, loop);
}

Core::Scheduler::SoundRoutine schedule_pattern(std::function<std::any(u_int64_t)> pattern_func, std::function<void(std::any)> callback, double interval_seconds)
{
    return Tasks::pattern(*get_scheduler(), pattern_func, callback, interval_seconds);
}

float* get_line_value(const std::string& name)
{
    return get_context()->get_line_value(name);
}

std::function<float()> line_value(const std::string& name)
{
    return get_context()->line_value(name);
}

void schedule_task(std::string name, Core::Scheduler::SoundRoutine&& task, bool initialize)
{
    get_context()->schedule_task(name, std::move(task), initialize);
}

bool cancel_task(const std::string& name)
{
    return get_context()->cancel_task(name);
}

bool restart_task(const std::string& name)
{
    return get_context()->restart_task(name);
}

Tasks::ActionToken Play(std::shared_ptr<Nodes::Node> node)
{
    return Tasks::ActionToken(node);
}

Tasks::ActionToken Wait(double seconds)
{
    return Tasks::ActionToken(seconds);
}

Tasks::ActionToken Action(std::function<void()> func)
{
    return Tasks::ActionToken(func);
}

//-------------------------------------------------------------------------
// Audio Processing
//-------------------------------------------------------------------------

void attach_quick_process(AudioProcessingFunction processor, std::shared_ptr<Buffers::AudioBuffer> buffer)
{
    get_buffer_manager()->attach_quick_process(processor, buffer);
}

void attach_quick_process_to_channel(AudioProcessingFunction processor, unsigned int channel_id)
{
    get_buffer_manager()->attach_quick_process_to_channel(processor, channel_id);
}

void attach_quick_process_to_channels(AudioProcessingFunction processor, const std::vector<unsigned int> channels)
{
    for (unsigned int channel : channels) {
        get_buffer_manager()->attach_quick_process_to_channel(processor, channel);
    }
}

//-------------------------------------------------------------------------
// Buffer Management
//-------------------------------------------------------------------------

void clear_buffer(Buffers::AudioBuffer& buffer)
{
    buffer.clear();
}

Buffers::AudioBuffer& get_channel(unsigned int channel)
{
    return *get_buffer_manager()->get_channel(channel);
}

void connect_node_to_channel(std::shared_ptr<Nodes::Node> node, u_int32_t channel_index, float mix, bool clear_before)
{
    get_buffer_manager()->connect_node_to_channel(node, channel_index, mix, clear_before);
}

void connect_node_to_buffer(std::shared_ptr<Nodes::Node> node, std::shared_ptr<Buffers::AudioBuffer> buffer, float mix, bool clear_before)
{
    get_buffer_manager()->connect_node_to_buffer(node, buffer, mix, clear_before);
}

//-------------------------------------------------------------------------
// Node Graph Management
//-------------------------------------------------------------------------

void add_node_to_root(std::shared_ptr<Nodes::Node> node, unsigned int channel)
{
    get_context()->get_node_graph_manager()->add_to_root(node, channel);
}

void remove_node_from_root(std::shared_ptr<Nodes::Node> node, unsigned int channel)
{
    get_context()->get_node_graph_manager()->get_root_node(channel).unregister_node(node);
}

void connect_nodes(std::string& source, std::string& target)
{
    get_context()->get_node_graph_manager()->connect(source, target);
}

void connect_nodes(std::shared_ptr<Nodes::Node> source, std::shared_ptr<Nodes::Node> target)
{
    source >> target;
}

Nodes::RootNode get_root_node(u_int32_t channel)
{
    return get_node_graph_manager()->get_root_node(channel);
}

} // namespace MayaFlux
