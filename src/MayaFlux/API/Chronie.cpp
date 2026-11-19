#include "Chronie.hpp"

#include "Core.hpp"

#include "MayaFlux/Core/Engine.hpp"
#include "MayaFlux/Kriya/BufferPipeline.hpp"
#include "MayaFlux/Kriya/Chain.hpp"
#include "MayaFlux/Kriya/Tasks.hpp"
#include "MayaFlux/Vruta/EventManager.hpp"
#include "MayaFlux/Vruta/Scheduler.hpp"

#include "MayaFlux/Vruta/ChronUtils.hpp"

#include "MayaFlux/Kriya/InputEvents.hpp"
#include "MayaFlux/Kriya/NetworkEvents.hpp"
#include "MayaFlux/Vruta/NetworkSource.hpp"

#include "MayaFlux/Portal/Forma/Primitives/Picker.hpp"
#include "MayaFlux/Portal/Forma/Primitives/Prompt.hpp"
#include "MayaFlux/Transitive/Parallel/Dispatch.hpp"
#include "MayaFlux/Transitive/Platform/UnixSocket.hpp"
#include "MayaFlux/Transitive/Protocol/HostPort.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux {

std::shared_ptr<Vruta::TaskScheduler> get_scheduler()
{
    return get_context().get_scheduler();
}

std::shared_ptr<Vruta::EventManager> get_event_manager()
{
    return get_context().get_event_manager();
}

template <typename... Args>
bool update_task_params(const std::string& name, Args... args)
{
    return get_scheduler()->update_task_params(name, args...);
}

void schedule_metro(double interval_seconds, std::function<void()> callback, std::string name, Vruta::ProcessingToken token)
{
    auto scheduler = get_scheduler();
    if (name.empty()) {
        name = "metro_" + std::to_string(scheduler->get_next_task_id());
    }
    auto metronome = Kriya::metro(interval_seconds, std::move(callback), token);

    get_scheduler()->add_task(metronome, name, false);
}

void schedule_sequence(std::vector<std::pair<double, std::function<void()>>> seq, std::string name, Vruta::ProcessingToken token)
{
    auto scheduler = get_scheduler();
    if (name.empty()) {
        name = "seq_" + std::to_string(scheduler->get_next_task_id());
    }

    auto tseq = Kriya::sequence(std::move(seq), token);

    get_scheduler()->add_task(tseq, name, false);
}

std::shared_ptr<Vruta::SoundRoutine> schedule_line(float start_value, float end_value, float duration_seconds, uint32_t step_duration, bool retain, std::string name)
{
    auto scheduler = get_scheduler();
    if (name.empty()) {
        name = "seq_" + std::to_string(scheduler->get_next_task_id());
    }

    auto line = std::make_shared<Vruta::SoundRoutine>(Kriya::line(start_value, end_value, duration_seconds, step_duration, retain));

    get_scheduler()->add_task(line, name, true);
    return line;
}

void schedule_pattern(std::function<std::any(uint64_t)> pattern_func, std::function<void(std::any)> callback, std::vector<double> durations, std::string name, Vruta::ProcessingToken token)
{
    auto scheduler = get_scheduler();
    if (name.empty()) {
        name = "pattern_" + std::to_string(scheduler->get_next_task_id());
    }

    scheduler->add_task(Kriya::pattern(std::move(pattern_func), std::move(callback), std::move(durations), token), name, false);
}

float* get_line_value(const std::string& name)
{
    if (auto task = get_scheduler()->get_task(name)) {
        auto cur_val = task->get_state<float>("current_value");
        if (cur_val) {
            return cur_val;
        }

        MF_ERROR(Journal::Component::API, Journal::Context::CoroutineScheduling, "line value not returned from task. Verify that tasks has not returned");
        return nullptr;
    }
    MF_ERROR(Journal::Component::API, Journal::Context::CoroutineScheduling, "Task: {} not found. Verify task validity or if its been scheduled, or its set to retain after completion", name);
    return nullptr;
}

void schedule_task(const std::string& name, Vruta::SoundRoutine&& task, bool initialize)
{
    auto task_ptr = std::make_shared<Vruta::SoundRoutine>(std::move(task));
    get_scheduler()->add_task(std::move(task_ptr), name, initialize);
}

void schedule_task(const std::string& name, Vruta::GraphicsRoutine&& task, bool initialize)
{
    auto task_ptr = std::make_shared<Vruta::GraphicsRoutine>(std::move(task));
    get_scheduler()->add_task(std::move(task_ptr), name, initialize);
}

void schedule_task(const std::string& name, Vruta::FreeRoutine&& task, bool initialize)
{
    auto task_ptr = std::make_shared<Vruta::FreeRoutine>(std::move(task));
    get_scheduler()->add_task(std::move(task_ptr), name, initialize);
}

void schedule_task(const std::string& name, const std::shared_ptr<Vruta::Routine>& task, bool initialize)
{
    get_scheduler()->add_task(task, name, initialize);
}

bool cancel_task(const std::string& name)
{
    return get_scheduler()->cancel_task(name);
}

bool restart_task(const std::string& name)
{
    return get_scheduler()->restart_task(name);
}

std::shared_ptr<Kriya::BufferPipeline> create_buffer_pipeline()
{
    return Kriya::BufferPipeline::create(*get_scheduler(), get_context().get_buffer_manager());
}

std::shared_ptr<Kriya::BufferPipeline> create_buffer_pipeline(std::shared_ptr<IO::IOManager> io_manager)
{
    return Kriya::BufferPipeline::create(*get_scheduler(), get_context().get_buffer_manager(), std::move(io_manager));
}

void on_key_pressed(
    const std::shared_ptr<Core::Window>& window,
    IO::ModifiedKey key,
    std::function<void()> callback,
    std::string name)
{
    auto event_manager = get_event_manager();
    if (name.empty()) {
        name = "key_press_" + std::to_string(event_manager->get_next_event_id());
    }

    auto event = std::make_shared<Vruta::Event>(
        Kriya::key_pressed(window, key, std::move(callback)));

    event_manager->add_event(event, name);
}

void on_key_released(
    const std::shared_ptr<Core::Window>& window,
    IO::Keys key,
    std::function<void()> callback,
    std::string name)
{
    auto event_manager = get_event_manager();
    if (name.empty()) {
        name = "key_release_" + std::to_string(event_manager->get_next_event_id());
    }

    auto event = std::make_shared<Vruta::Event>(
        Kriya::key_released(window, key, std::move(callback)));

    event_manager->add_event(event, name);
}

void on_any_key(
    const std::shared_ptr<Core::Window>& window,
    std::function<void(IO::Keys)> callback,
    std::string name)
{
    auto event_manager = get_event_manager();
    if (name.empty()) {
        name = "any_key_" + std::to_string(event_manager->get_next_event_id());
    }

    auto event = std::make_shared<Vruta::Event>(
        Kriya::any_key(window, std::move(callback)));

    event_manager->add_event(event, name);
}

void on_mouse_pressed(
    const std::shared_ptr<Core::Window>& window,
    IO::MouseButtons button,
    std::function<void(double, double)> callback,
    std::string name)
{
    auto event_manager = get_event_manager();
    if (name.empty()) {
        name = "mouse_press_" + std::to_string(event_manager->get_next_event_id());
    }

    auto event = std::make_shared<Vruta::Event>(
        Kriya::mouse_pressed(window, button, std::move(callback)));

    event_manager->add_event(event, name);
}

void on_mouse_released(
    const std::shared_ptr<Core::Window>& window,
    IO::MouseButtons button,
    std::function<void(double, double)> callback,
    std::string name)
{
    auto event_manager = get_event_manager();
    if (name.empty()) {
        name = "mouse_release_" + std::to_string(event_manager->get_next_event_id());
    }

    auto event = std::make_shared<Vruta::Event>(
        Kriya::mouse_released(window, button, std::move(callback)));

    event_manager->add_event(event, name);
}

void on_mouse_move(
    const std::shared_ptr<Core::Window>& window,
    std::function<void(double, double)> callback,
    std::string name)
{
    auto event_manager = get_event_manager();
    if (name.empty()) {
        name = "mouse_move_" + std::to_string(event_manager->get_next_event_id());
    }

    auto event = std::make_shared<Vruta::Event>(
        Kriya::mouse_moved(window, std::move(callback)));

    event_manager->add_event(event, name);
}

void on_mouse_drag(
    const std::shared_ptr<Core::Window>& window,
    IO::MouseButtons button,
    std::function<void(double, double)> callback,
    std::string name)
{
    auto event_manager = get_event_manager();
    if (name.empty()) {
        name = "mouse_drag_" + std::to_string(event_manager->get_next_event_id());
    }

    auto event = std::make_shared<Vruta::Event>(
        Kriya::mouse_dragged(window, button, std::move(callback)));

    event_manager->add_event(event, name);
}

void on_scroll(
    const std::shared_ptr<Core::Window>& window,
    std::function<void(double, double)> callback,
    std::string name)
{
    auto event_manager = get_event_manager();
    if (name.empty()) {
        name = "scroll_" + std::to_string(event_manager->get_next_event_id());
    }

    auto event = std::make_shared<Vruta::Event>(
        Kriya::mouse_scrolled(window, std::move(callback)));

    event_manager->add_event(event, name);
}

bool cancel_event_handler(const std::string& name)
{
    return get_event_manager()->cancel_event(name);
}

uint64_t seconds_to_samples(double seconds)
{
    uint64_t sample_rate = 48000;
    if (get_context().is_running()) {
        sample_rate = get_context().get_stream_info().sample_rate;
    }
    return static_cast<uint64_t>(seconds * (double)sample_rate);
}

uint64_t seconds_to_blocks(double seconds)
{
    uint32_t sample_rate = 48000;
    uint32_t block_size = 512;

    if (get_context().is_running()) {
        sample_rate = get_context().get_stream_info().sample_rate;
        block_size = get_context().get_stream_info().buffer_size;
    }

    return Vruta::seconds_to_blocks(seconds, sample_rate, block_size);
}

uint64_t samples_to_blocks(uint64_t samples)
{
    uint32_t block_size = 512;

    if (get_context().is_running()) {
        block_size = get_context().get_stream_info().buffer_size;
    }

    return Vruta::samples_to_blocks(samples, block_size);
}

double samples_to_seconds(uint64_t samples)
{
    uint64_t sample_rate = 48000;
    if (get_context().is_running()) {
        sample_rate = get_context().get_stream_info().sample_rate;
    }

    return static_cast<double>(samples) / (double)sample_rate;
}

namespace {

    void add_network_event(Vruta::Event event, std::string name, std::string_view prefix)
    {
        auto event_manager = get_event_manager();
        if (name.empty()) {
            name = std::string(prefix) + std::to_string(event_manager->get_next_event_id());
        }
        event_manager->add_event(std::make_shared<Vruta::Event>(std::move(event)), name);
    }

} // namespace

void on_message_received(
    std::shared_ptr<Vruta::NetworkSource> source,
    std::function<void(const Core::NetworkMessage&)> callback,
    std::string name)
{
    add_network_event(
        Kriya::message_received(std::move(source), std::move(callback)),
        std::move(name), "net_msg_");
}

void on_message_received(
    std::shared_ptr<Vruta::NetworkSource> source,
    std::string sender_address,
    std::function<void(const Core::NetworkMessage&)> callback,
    std::string name)
{
    add_network_event(
        Kriya::message_received(std::move(source), std::move(sender_address), std::move(callback)),
        std::move(name), "net_msg_from_");
}

void on_message_received(
    std::shared_ptr<Vruta::NetworkSource> source,
    uint64_t connection_id,
    std::function<void(const Core::NetworkMessage&)> callback,
    std::string name)
{
    add_network_event(
        Kriya::message_received(std::move(source), connection_id, std::move(callback)),
        std::move(name), "net_msg_conn_");
}

void on_message_received(
    std::shared_ptr<Vruta::NetworkSource> source,
    std::function<bool(const Core::NetworkMessage&)> predicate,
    std::function<void(const Core::NetworkMessage&)> callback,
    std::string name)
{
    add_network_event(
        Kriya::message_received(std::move(source), std::move(predicate), std::move(callback)),
        std::move(name), "net_msg_match_");
}

std::shared_ptr<Vruta::NetworkSource> on_message_received(
    const Core::EndpointInfo& info,
    std::function<void(const Core::NetworkMessage&)> callback,
    std::string name)
{
    auto source = std::make_shared<Vruta::NetworkSource>(info);
    on_message_received(source, std::move(callback), std::move(name));
    return source;
}

std::shared_ptr<Vruta::NetworkSource> on_message_received(
    const Core::EndpointInfo& info,
    std::string sender_address,
    std::function<void(const Core::NetworkMessage&)> callback,
    std::string name)
{
    auto source = std::make_shared<Vruta::NetworkSource>(info);
    on_message_received(source, std::move(sender_address), std::move(callback), std::move(name));
    return source;
}

std::shared_ptr<Vruta::NetworkSource> on_message_received(
    const Core::EndpointInfo& info,
    std::function<bool(const Core::NetworkMessage&)> predicate,
    std::function<void(const Core::NetworkMessage&)> callback,
    std::string name)
{
    auto source = std::make_shared<Vruta::NetworkSource>(info);
    on_message_received(source, std::move(predicate), std::move(callback), std::move(name));
    return source;
}

void on_network_message(
    std::shared_ptr<Vruta::NetworkSource> source,
    std::function<void(const Core::NetworkMessage&)> callback,
    std::string name)
{
    on_message_received(std::move(source), std::move(callback), std::move(name));
}

std::shared_ptr<Vruta::NetworkSource> on_network_message(
    const Core::EndpointInfo& info,
    std::function<void(const Core::NetworkMessage&)> callback,
    std::string name)
{
    return on_message_received(info, std::move(callback), std::move(name));
}

void on_network_message_from(
    std::shared_ptr<Vruta::NetworkSource> source,
    std::string sender_address,
    std::function<void(const Core::NetworkMessage&)> callback,
    std::string name)
{
    on_message_received(std::move(source), std::move(sender_address), std::move(callback), std::move(name));
}

std::shared_ptr<Vruta::NetworkSource> on_network_message_from(
    const Core::EndpointInfo& info,
    std::string sender_address,
    std::function<void(const Core::NetworkMessage&)> callback,
    std::string name)
{
    return on_message_received(info, std::move(sender_address), std::move(callback), std::move(name));
}

void on_network_message_matching(
    std::shared_ptr<Vruta::NetworkSource> source,
    std::function<bool(const Core::NetworkMessage&)> predicate,
    std::function<void(const Core::NetworkMessage&)> callback,
    std::string name)
{
    on_message_received(std::move(source), std::move(predicate), std::move(callback), std::move(name));
}

std::shared_ptr<Vruta::NetworkSource> on_network_message_matching(
    const Core::EndpointInfo& info,
    std::function<bool(const Core::NetworkMessage&)> predicate,
    std::function<void(const Core::NetworkMessage&)> callback,
    std::string name)
{
    return on_message_received(info, std::move(predicate), std::move(callback), std::move(name));
}

namespace {

    struct EndpointChoice {
        Core::NetworkTransport transport;
        Core::EndpointRole role;
        std::string label;
    };

    struct FramingChoice {
        Core::FramingPolicy framing;
        std::string label;
    };

    template <typename T>
    std::optional<T> pick_one(std::string title, std::vector<T> items,
        std::function<std::string(const T&)> label)
    {
        auto promise = std::make_shared<std::promise<std::optional<T>>>();
        auto future = promise->get_future();

        Portal::Forma::pick<T>(std::move(title), std::move(items), std::move(label),
            [promise](std::optional<T> chosen) { promise->set_value(std::move(chosen)); });

        return Parallel::await_future(std::move(future));
    }

    std::optional<std::string> ask_one(std::string title, std::string initial,
        Portal::Forma::AskValidator validator)
    {
        auto promise = std::make_shared<std::promise<std::optional<std::string>>>();
        auto future = promise->get_future();

        Portal::Forma::ask(std::move(title), std::move(initial), std::move(validator),
            [promise](std::optional<std::string> text) { promise->set_value(std::move(text)); });

        return Parallel::await_future(std::move(future));
    }

    bool is_socket_name(std::string_view text)
    {
        return !text.empty()
            && std::ranges::none_of(text, [](char c) { return static_cast<unsigned char>(c) < ' ' || c == 0x7F; });
    }

    bool is_port(std::string_view text)
    {
        return Transitive::Protocol::parse_port(text).has_value();
    }

    bool is_host_port(std::string_view text)
    {
        return Transitive::Protocol::parse_host_port(text).has_value();
    }

    bool has_stream_framing(Core::NetworkTransport transport)
    {
        return transport == Core::NetworkTransport::UDS || transport == Core::NetworkTransport::TCP;
    }

    std::string describe_endpoint(const Core::EndpointInfo& info)
    {
        const bool listens = info.role == Core::EndpointRole::RECEIVE;

        switch (info.transport) {
        case Core::NetworkTransport::UDS:
            return std::format("uds {} {}", listens ? "listen" : "connect",
                listens ? info.local_path : info.remote_path);
        case Core::NetworkTransport::TCP:
            return listens
                ? std::format("tcp listen {}", info.local_port)
                : std::format("tcp connect {}:{}", info.remote_address, info.remote_port);
        case Core::NetworkTransport::UDP:
            return listens
                ? std::format("udp receive {}", info.local_port)
                : std::format("udp send {}:{}", info.remote_address, info.remote_port);
        case Core::NetworkTransport::SHARED_MEMORY:
            break;
        }
        return "endpoint";
    }

    std::optional<std::string> choose_socket_to_connect()
    {
        const auto sockets = Platform::list_unix_sockets();

        if (sockets.empty()) {
            return ask_one("Socket to connect to, a name or a path", "", is_socket_name);
        }

        const auto chosen = pick_one<std::filesystem::path>("Choose a socket", sockets,
            [](const std::filesystem::path& path) { return path.string(); });
        return chosen ? std::optional<std::string>(chosen->string()) : std::nullopt;
    }

}

std::optional<Core::EndpointInfo> choose_endpoint()
{
    const std::vector<EndpointChoice> kinds {
        { Core::NetworkTransport::UDS, Core::EndpointRole::RECEIVE, "Unix socket: listen" },
        { Core::NetworkTransport::UDS, Core::EndpointRole::BIDIRECTIONAL, "Unix socket: connect" },
        { Core::NetworkTransport::TCP, Core::EndpointRole::RECEIVE, "TCP: listen on a port" },
        { Core::NetworkTransport::TCP, Core::EndpointRole::BIDIRECTIONAL, "TCP: connect to a host" },
        { Core::NetworkTransport::UDP, Core::EndpointRole::RECEIVE, "UDP: receive on a port" },
        { Core::NetworkTransport::UDP, Core::EndpointRole::SEND, "UDP: send to a host" },
    };

    const auto kind = pick_one<EndpointChoice>("Network endpoint", kinds,
        [](const EndpointChoice& choice) { return choice.label; });
    if (!kind) {
        return std::nullopt;
    }

    Core::EndpointInfo info;
    info.transport = kind->transport;
    info.role = kind->role;
    const bool listens = kind->role == Core::EndpointRole::RECEIVE;

    if (kind->transport == Core::NetworkTransport::UDS) {
        if (listens) {
            const auto name = ask_one("Socket name", "mayaflux.sock", is_socket_name);
            if (!name) {
                return std::nullopt;
            }
            info.local_path = *name;
        } else {
            const auto path = choose_socket_to_connect();
            if (!path) {
                return std::nullopt;
            }
            info.remote_path = *path;
        }
    } else if (listens) {
        const auto text = ask_one("Port to listen on", "9000", is_port);
        if (!text) {
            return std::nullopt;
        }
        info.local_port = *Transitive::Protocol::parse_port(*text);
    } else {
        const auto text = ask_one("Host and port", "127.0.0.1:9000", is_host_port);
        if (!text) {
            return std::nullopt;
        }
        const auto target = *Transitive::Protocol::parse_host_port(*text);
        info.remote_address = target.host;
        info.remote_port = target.port;
    }

    if (has_stream_framing(info.transport)) {
        const std::vector<FramingChoice> framings {
            { Core::FramingPolicy::LINE, "Lines of text, one message per line" },
            { Core::FramingPolicy::LENGTH_PREFIX, "Length prefixed bytes, a four byte size first" },
            { Core::FramingPolicy::NONE, "Raw bytes, no boundaries" },
        };

        const auto framing = pick_one<FramingChoice>("Message framing", framings,
            [](const FramingChoice& choice) { return choice.label; });
        if (!framing) {
            return std::nullopt;
        }
        info.framing = framing->framing;
    }

    info.label = describe_endpoint(info);
    return info;
}

}
