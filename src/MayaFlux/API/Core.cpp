#include "Core.hpp"
#include "MayaFlux/Core/Engine.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include <GLFW/glfw3.h>

#ifdef MAYAFLUX_PLATFORM_MACOS
#include <CoreFoundation/CoreFoundation.h>
#endif // MAYAFLUX_PLATFORM_MACOS

namespace MayaFlux {

//-------------------------------------------------------------------------
// Internal Functions and Data
//-------------------------------------------------------------------------

namespace internal {

    std::unique_ptr<Core::Engine> engine_ref;
    bool initialized = false;
    std::recursive_mutex engine_mutex;

    void cleanup_engine()
    {
        std::lock_guard<std::recursive_mutex> lock(engine_mutex);
        if (engine_ref) {
            if (initialized) {
                if (engine_ref->is_running()) {
                    engine_ref->Pause();
                }
                engine_ref->End();
                Journal::Archivist::shutdown();
            }
            engine_ref.reset();
            initialized = false;
        }
    }

    Core::Engine& get_or_create_engine()
    {
        std::lock_guard<std::recursive_mutex> lock(engine_mutex);
        if (!engine_ref) {
            engine_ref = std::make_unique<Core::Engine>();
            std::atexit(cleanup_engine);
        }
        initialized = true;
        return *engine_ref;
    }

}

//-------------------------------------------------------------------------
// Engine Management
//-------------------------------------------------------------------------

bool is_initialized()
{
    return internal::initialized;
}

Core::Engine& get_context()
{
    return internal::get_or_create_engine();
}

void set_and_transfer_context(Core::Engine instance)
{
    bool is_same_instance = false;

    {
        std::lock_guard<std::recursive_mutex> lock(internal::engine_mutex);
        is_same_instance = (&instance == internal::engine_ref.get());
        if (internal::engine_ref && !is_same_instance && internal::engine_ref->is_running()) {
            internal::engine_ref->Pause();
        }
    }

    if (internal::engine_ref && !is_same_instance) {
        internal::engine_ref->End();
    }

    if (!is_same_instance) {
        std::lock_guard<std::recursive_mutex> lock(internal::engine_mutex);
        internal::engine_ref = std::make_unique<Core::Engine>(std::move(instance));
        internal::initialized = true;
    }
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

void Init(Core::GlobalStreamInfo stream_info)
{
    auto& engine = internal::get_or_create_engine();
    engine.Init(stream_info);
}

void Init(Core::GlobalStreamInfo stream_info, Core::GlobalGraphicsConfig graphics_config)
{
    auto& engine = internal::get_or_create_engine();
    engine.Init(stream_info, graphics_config);
}

void Start()
{
    get_context().Start();
}

void Pause()
{
    if (internal::initialized) {
        get_context().Pause();
    }
}

void Resume()
{
    if (internal::initialized) {
        get_context().Resume();
    }
}

void Await()
{
#ifdef MAYAFLUX_PLATFORM_MACOS
    CFRunLoopRef runLoop = CFRunLoopGetCurrent();

    __block bool shouldExit = false;

    CFRunLoopObserverRef observer = CFRunLoopObserverCreateWithHandler(
        kCFAllocatorDefault,
        kCFRunLoopBeforeWaiting | kCFRunLoopAfterWaiting,
        true,
        0,
        ^(CFRunLoopObserverRef observer, CFRunLoopActivity activity) {
            while (CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, true) == kCFRunLoopRunHandledSource) {
                // Blocks were processed
            }
        });

    CFRunLoopAddObserver(runLoop, observer, kCFRunLoopCommonModes);

    dispatch_source_t stdinSource = dispatch_source_create(
        DISPATCH_SOURCE_TYPE_READ,
        STDIN_FILENO,
        0,
        dispatch_get_main_queue());

    dispatch_source_set_event_handler(stdinSource, ^{
        char buf[1024];
        read(STDIN_FILENO, buf, sizeof(buf));
        shouldExit = true;
        CFRunLoopStop(runLoop);
    });

    dispatch_resume(stdinSource);

    while (!shouldExit) {
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.16, false);
    }

    dispatch_source_cancel(stdinSource);
    CFRunLoopRemoveObserver(runLoop, observer, kCFRunLoopCommonModes);
    CFRelease(observer);
    CFRelease(stdinSource);
#else
    std::cin.get();
#endif

    MF_PRINT(Journal::Component::API, Journal::Context::Runtime,
        "Input received - shutting down");
}

void End()
{
    if (internal::initialized) {
        internal::cleanup_engine();
    }
}

}
