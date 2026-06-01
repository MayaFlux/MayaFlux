#include "AudioSubsystem.hpp"

#include "MayaFlux/Registry/BackendRegistry.hpp"
#include "MayaFlux/Registry/Service/AudioBackendService.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Core {

AudioSubsystem::AudioSubsystem(GlobalStreamInfo& stream_info)
    : m_stream_info(stream_info)
    , m_audiobackend(AudioBackendFactory::create_backend(stream_info.backend))
    , m_audio_device(m_audiobackend->create_device_manager())
    , m_subsystem_tokens {
        .Buffer = MayaFlux::Buffers::ProcessingToken::AUDIO_BACKEND,
        .Node = MayaFlux::Nodes::ProcessingToken::AUDIO_RATE,
        .Task = MayaFlux::Vruta::ProcessingToken::SAMPLE_ACCURATE
    }
{
}

void AudioSubsystem::initialize(SubsystemProcessingHandle& handle)
{
    m_handle = &handle;

    m_audio_stream = m_audiobackend->create_stream(
        m_audio_device->get_default_output_device(),
        m_audio_device->get_default_input_device(),
        m_stream_info,
        this);

    register_backend_service();
    m_notify_running.store(true, std::memory_order_release);

#ifdef MAYAFLUX_PLATFORM_MACOS
    m_observers_ptr.store(new ObserverMap(), std::memory_order_release);
#endif

    m_notify_thread = std::thread(&AudioSubsystem::notify_loop, this);
    m_is_ready = true;
}

void AudioSubsystem::register_callbacks()
{
    if (!m_is_ready || !m_audio_stream) {
        error<std::runtime_error>(
            Journal::Component::Core,
            Journal::Context::AudioSubsystem,
            std::source_location::current(),
            "AudioSubsystem not initialized");
    }

    m_audio_stream->set_process_callback(
        [this](void* output_buffer, void* input_buffer, unsigned int num_frames) -> int {
            auto input_ptr = static_cast<double*>(input_buffer);
            auto output_ptr = static_cast<double*>(output_buffer);

            if (input_ptr && output_ptr) {
                return this->process_audio(input_ptr, output_ptr, num_frames);
            }

            if (output_ptr) {
                return this->process_output(output_ptr, num_frames);
            }

            if (input_ptr) {
                return this->process_input(input_ptr, num_frames);
            }
            return 0;
        });
}

int AudioSubsystem::process_output(double* output_buffer, unsigned int num_frames)
{
    m_callback_active.fetch_add(1, std::memory_order_acquire);

    if (output_buffer == nullptr) {
        MF_RT_ERROR(Journal::Component::Core, Journal::Context::AudioCallback,
            "No output available");
        m_callback_active.fetch_sub(1, std::memory_order_release);
        return 1;
    }

    if (!m_is_running.load(std::memory_order_acquire)) {
        if (output_buffer) {
            auto total_samples = static_cast<uint32_t>(num_frames * m_stream_info.output.channels);
            std::memset(output_buffer, 0, total_samples * sizeof(double));
        }
        m_callback_active.fetch_sub(1, std::memory_order_release);
        return 0;
    }

    if (m_handle == nullptr) {
        MF_RT_ERROR(Journal::Component::Core, Journal::Context::AudioCallback,
            "Invalid processing handle");
        m_callback_active.fetch_sub(1, std::memory_order_release);
        return 1;
    }

    try {
        uint32_t num_channels = m_stream_info.output.channels;
        size_t total_samples = static_cast<size_t>(num_frames) * num_channels;
        std::span<double> output_span(output_buffer, total_samples);

        m_handle->nodes.update_routing_states();
        m_handle->buffers.update_routing_states();

        std::vector<std::span<const double>> buffer_data(num_channels);
        std::vector<std::vector<std::vector<double>>> all_network_outputs(num_channels);
        bool has_underrun = false;

        m_handle->tasks.process_buffer_cycle();

        for (uint32_t channel = 0; channel < num_channels; channel++) {
            m_handle->buffers.process_channel(channel, num_frames);
            all_network_outputs[channel] = m_handle->nodes.process_audio_networks(num_frames, channel);

            auto channel_data = m_handle->buffers.read_channel_data(channel);

            if (channel_data.size() < num_frames) {
                MF_RT_WARN(Journal::Component::Core, Journal::Context::AudioCallback,
                    "Channel buffer underrun");
                has_underrun = true;

                buffer_data[channel] = std::span<const double>();
            } else {
                buffer_data[channel] = channel_data;
            }
        }

        for (size_t i = 0; i < num_frames; ++i) {

            m_handle->tasks.process(1);

            for (size_t j = 0; j < num_channels; ++j) {
                double buffer_sample = 0.0;
                if (!buffer_data[j].empty() && i < buffer_data[j].size()) {
                    buffer_sample = buffer_data[j][i];
                }

                double sample = m_handle->nodes.process_sample(j) + buffer_sample;

                for (const auto& network_buffer : all_network_outputs[j]) {
                    if (i < network_buffer.size()) {
                        sample += network_buffer[i];
                    }
                }

                size_t index = i * num_channels + j;
                output_span[index] = std::clamp(sample, -1., 1.);
            }
        }

        m_handle->nodes.cleanup_completed_routing();
        m_handle->buffers.cleanup_completed_routing();

        m_snapshot_size.store(static_cast<uint32_t>(total_samples), std::memory_order_release);
        m_snapshot_ptr.store(output_buffer, std::memory_order_release);
        m_snapshot_generation.fetch_add(1, std::memory_order_release);
        m_snapshot_generation.notify_all();

        m_callback_active.fetch_sub(1, std::memory_order_release);
        return has_underrun ? 1 : 0;

    } catch (const std::exception& e) {
        MF_RT_ERROR(Journal::Component::Core, Journal::Context::AudioCallback,
            "Exception during audio output processing: {}", e.what());

        if (output_buffer) {
            auto total_samples = static_cast<uint32_t>(num_frames * m_stream_info.output.channels);
            std::memset(output_buffer, 0, total_samples * sizeof(double));
        }

        m_callback_active.fetch_sub(1, std::memory_order_release);
        return 1;
    }
}

int AudioSubsystem::process_input(double* input_buffer, unsigned int num_frames)
{
    m_callback_active.fetch_add(1, std::memory_order_acquire);
    if (m_handle == nullptr) {
        MF_RT_ERROR(Journal::Component::Core, Journal::Context::AudioCallback,
            "Invalid processing handle");
        m_callback_active.fetch_sub(1, std::memory_order_release);
        return 1;
    }

    if (!m_is_running.load(std::memory_order_acquire)) {
        if (input_buffer) {
            auto total_samples = static_cast<uint32_t>(num_frames * m_stream_info.input.channels);
            std::memset(input_buffer, 0, total_samples * sizeof(double));
        }
        m_callback_active.fetch_sub(1, std::memory_order_release);
        return 0;
    }

    m_handle->buffers.process_input(input_buffer, m_stream_info.input.channels, num_frames);

    m_callback_active.fetch_sub(1, std::memory_order_release);
    return 0;
}

int AudioSubsystem::process_audio(double* input_buffer, double* output_buffer, unsigned int num_frames)
{
    for (const auto& [name, hook] : m_handle->pre_process_hooks) {
        hook(num_frames);
    }

    process_input(input_buffer, num_frames);
    process_output(output_buffer, num_frames);

    for (const auto& [name, hook] : m_handle->post_process_hooks) {
        hook(num_frames);
    }

    return 0;
}

void AudioSubsystem::register_backend_service()
{
    auto svc = std::make_shared<Registry::Service::AudioBackendService>();

    svc->get_output_snapshot = [this]() -> std::span<const double> {
        const double* ptr = m_snapshot_ptr.load(std::memory_order_acquire);
        uint32_t sz = m_snapshot_size.load(std::memory_order_acquire);
        if (!ptr || sz == 0)
            return {};
        return { ptr, sz };
    };

    svc->register_output_observer = [this](
                                        std::function<void(const double*, uint32_t)> cb) -> uint32_t {
        uint32_t id = m_next_observer_id.fetch_add(1, std::memory_order_relaxed);
#ifdef MAYAFLUX_PLATFORM_MACOS
        const ObserverMap* current = m_observers_ptr.load(std::memory_order_acquire);
        const ObserverMap* next;
        do {
            auto* copy = new ObserverMap(*current);
            (*copy)[id] = cb;
            next = copy;
        } while (!m_observers_ptr.compare_exchange_weak(
            current, next,
            std::memory_order_release, std::memory_order_acquire));
        retire_observers(current);
#else
        auto current = m_observers.load(std::memory_order_acquire);
        std::shared_ptr<ObserverMap> next;
        do {
            next = std::make_shared<ObserverMap>(*current);
            (*next)[id] = cb;
        } while (!m_observers.compare_exchange_weak(
            current, next,
            std::memory_order_release, std::memory_order_acquire));
#endif
        return id;
    };

    svc->unregister_output_observer = [this](uint32_t id) {
#ifdef MAYAFLUX_PLATFORM_MACOS
        const ObserverMap* current = m_observers_ptr.load(std::memory_order_acquire);
        const ObserverMap* next;
        do {
            auto* copy = new ObserverMap(*current);
            copy->erase(id);
            next = copy;
        } while (!m_observers_ptr.compare_exchange_weak(
            current, next,
            std::memory_order_release, std::memory_order_acquire));
        retire_observers(current);
#else
        auto current = m_observers.load(std::memory_order_acquire);
        std::shared_ptr<ObserverMap> next;
        do {
            next = std::make_shared<ObserverMap>(*current);
            next->erase(id);
        } while (!m_observers.compare_exchange_weak(
            current, next,
            std::memory_order_release, std::memory_order_acquire));
#endif
    };

    m_audio_backend_service = svc;
    Registry::BackendRegistry::instance()
        .register_service<Registry::Service::AudioBackendService>(
            [svc]() -> void* { return svc.get(); });
}

void AudioSubsystem::notify_loop()
{
    uint64_t last_gen = 0;

    while (m_notify_running.load(std::memory_order_acquire)) {
        m_snapshot_generation.wait(last_gen, std::memory_order_relaxed);

        if (!m_notify_running.load(std::memory_order_acquire))
            break;

        last_gen = m_snapshot_generation.load(std::memory_order_acquire);

        const double* ptr = m_snapshot_ptr.load(std::memory_order_acquire);
        uint32_t sz = m_snapshot_size.load(std::memory_order_acquire);

        if (!ptr || sz == 0)
            continue;

#ifdef MAYAFLUX_PLATFORM_MACOS
        auto [obs, slot] = acquire_observers();
        if (obs) {
            for (auto& [id, cb] : *obs)
                cb(ptr, sz);
        }
        release_observers(slot);
#else
        auto obs = m_observers.load(std::memory_order_acquire);
        for (auto& [id, cb] : *obs)
            cb(ptr, sz);
#endif
    }
}

#ifdef MAYAFLUX_PLATFORM_MACOS
std::pair<const AudioSubsystem::ObserverMap*, size_t>
AudioSubsystem::acquire_observers() const
{
    size_t slot = m_obs_hazard_counter.fetch_add(1, std::memory_order_relaxed)
        % MAX_OBSERVER_READERS;
    const ObserverMap* current;
    do {
        current = m_observers_ptr.load(std::memory_order_acquire);
        m_obs_hazard_ptrs[slot].store(current, std::memory_order_release);
    } while (current != m_observers_ptr.load(std::memory_order_acquire));
    return { current, slot };
}

void AudioSubsystem::release_observers(size_t slot) const
{
    m_obs_hazard_ptrs[slot].store(nullptr, std::memory_order_release);
}

void AudioSubsystem::retire_observers(const ObserverMap* old)
{
    m_obs_retired.push_back(old);
    auto it = m_obs_retired.begin();
    while (it != m_obs_retired.end()) {
        bool referenced = false;
        for (size_t i = 0; i < MAX_OBSERVER_READERS; ++i) {
            if (m_obs_hazard_ptrs[i].load(std::memory_order_acquire) == *it) {
                referenced = true;
                break;
            }
        }
        if (!referenced) {
            delete *it;
            it = m_obs_retired.erase(it);
        } else {
            ++it;
        }
    }
}
#endif

void AudioSubsystem::start()
{
    if (!m_is_ready || !m_audio_stream) {
        error<std::runtime_error>(
            Journal::Component::Core,
            Journal::Context::AudioSubsystem,
            std::source_location::current(),
            "Cannot start AudioSubsystem: not initialized");
    }

    m_audio_stream->open();
    m_audio_stream->start();
    m_is_running.store(true);
}

void AudioSubsystem::stop()
{
    if (!m_is_running.load()) {
        return;
    }

    MF_INFO(Journal::Component::Core, Journal::Context::AudioSubsystem,
        "Stopping AudioSubsystem...");

    m_is_running.store(false, std::memory_order_release);

    if (m_audio_stream && m_audio_stream->is_running()) {
        m_audio_stream->stop();
    }

    if (m_callback_active.load() > 0) {
        MF_INFO(Journal::Component::Core, Journal::Context::AudioSubsystem,
            "Stopped while {} callback(s) active", m_callback_active.load());
    }

    m_notify_running.store(false, std::memory_order_release);
    m_snapshot_generation.fetch_add(1, std::memory_order_release);
    m_snapshot_generation.notify_all();

    if (m_notify_thread.joinable())
        m_notify_thread.join();

    MF_INFO(Journal::Component::Core, Journal::Context::AudioSubsystem,
        "AudioSubsystem stopped");
}

void AudioSubsystem::pause()
{
    if (m_audio_stream && m_is_running.load()) {
        m_audio_stream->pause();
        m_is_paused = true;
    }
}

void AudioSubsystem::resume()
{
    if (m_audio_stream && m_is_paused) {
        m_audio_stream->resume();
        m_is_paused = false;
    }
}

void AudioSubsystem::wait_until_running()
{
    while (!m_is_running.load(std::memory_order_acquire))
        std::this_thread::yield();
}

void AudioSubsystem::shutdown()
{
    stop();

    if (m_audio_stream) {
        m_audio_stream->close();
    }
    m_audio_stream.reset();
    m_audio_device.reset();
    m_audiobackend->cleanup();
    m_audiobackend.reset();

    Registry::BackendRegistry::instance()
        .unregister_service<Registry::Service::AudioBackendService>();

    m_audio_backend_service.reset();

#ifdef MAYAFLUX_PLATFORM_MACOS
    delete m_observers_ptr.exchange(nullptr, std::memory_order_acq_rel);
#endif

    m_is_ready = false;
}

}
