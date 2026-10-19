#include "Core.hpp"
#include "MayaFlux/Core/Engine.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Transitive/Memory/Persist.hpp"

namespace MayaFlux {

//-------------------------------------------------------------------------
// Internal Functions and Data
//-------------------------------------------------------------------------

namespace internal {

    /**
     * The engine is published through a raw atomic pointer and is freed only
     * after End() has joined every thread that can call a global getter, so a
     * getter is a single acquire load and never waits on a lifecycle change.
     */
    std::atomic<Core::Engine*> engine_ptr { nullptr };
    std::atomic<bool> initialized { false };
    std::atomic<bool> terminating { false };
    std::atomic<bool> ending { false };
    std::atomic<bool> atexit_registered { false };
    thread_local bool cleaning_up = false;

    void cleanup_engine()
    {
        if (cleaning_up) {
            return;
        }

        if (ending.exchange(true, std::memory_order_acq_rel)) {
            while (ending.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            return;
        }

        cleaning_up = true;

        if (std::unique_ptr<Core::Engine> engine { engine_ptr.load(std::memory_order_acquire) }) {
            if (initialized.load(std::memory_order_acquire)) {
                engine->End();
                Journal::Archivist::shutdown();
            }
            cleanup_persistent_store();
            initialized.store(false, std::memory_order_release);
            engine_ptr.store(nullptr, std::memory_order_release);
        }

        cleaning_up = false;
        ending.store(false, std::memory_order_release);
    }

    Core::Engine& get_or_create_engine()
    {
        Core::Engine* engine = engine_ptr.load(std::memory_order_acquire);
        if (engine) {
            return *engine;
        }

        auto candidate = std::make_unique<Core::Engine>();
        Core::Engine* expected = nullptr;
        if (engine_ptr.compare_exchange_strong(expected, candidate.get(),
                std::memory_order_acq_rel, std::memory_order_acquire)) {
            engine = candidate.release();
            if (!atexit_registered.exchange(true, std::memory_order_acq_rel)) {
                std::atexit(cleanup_engine);
            }
            terminating.store(false, std::memory_order_release);
            initialized.store(true, std::memory_order_release);
        } else {
            engine = expected;
        }
        return *engine;
    }

}

//-------------------------------------------------------------------------
// Engine Management
//-------------------------------------------------------------------------

bool is_initialized()
{
    if (internal::terminating.load(std::memory_order_acquire)) {
        return false;
    }
    return internal::initialized.load(std::memory_order_acquire);
}

bool is_configured()
{
    if (!is_initialized()) {
        return false;
    }
    return get_context().is_configured();
}

Core::Engine& get_context()
{
    return internal::get_or_create_engine();
}

void Init()
{
    auto& engine = internal::get_or_create_engine();
    engine.Init();
}

void Init(uint32_t sample_rate, uint32_t buffer_size, uint32_t num_out_channels, uint32_t num_in_channels)
{
    auto& engine = internal::get_or_create_engine();
    auto& stream_info = engine.get_stream_info();

    stream_info.sample_rate = sample_rate;
    stream_info.buffer_size = buffer_size;
    stream_info.output.channels = num_out_channels;
    stream_info.input.channels = num_in_channels;
    engine.Init();
}

void Init(Core::GlobalStreamInfo stream_info, Core::GlobalGraphicsConfig graphics_config, Core::GlobalInputConfig input_config, Core::GlobalNetworkConfig network_config)
{
    auto& engine = internal::get_or_create_engine();
    engine.Init(stream_info, graphics_config, input_config, network_config);
}

void Start()
{
    get_context().Start();
}

void Pause()
{
    if (internal::initialized.load(std::memory_order_acquire)) {
        get_context().Pause();
    }
}

void Resume()
{
    if (internal::initialized.load(std::memory_order_acquire)) {
        get_context().Resume();
    }
}

void Await()
{
    if (internal::initialized.load(std::memory_order_acquire)) {
        get_context().await_shutdown();
    }
}

void End()
{
    internal::terminating.store(true, std::memory_order_release);
    if (internal::initialized.load(std::memory_order_acquire)) {
        internal::cleanup_engine();
    }
}

}
