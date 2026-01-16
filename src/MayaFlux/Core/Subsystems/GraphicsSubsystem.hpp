#pragma once

#include "MayaFlux/Core/GlobalGraphicsInfo.hpp"
#include "Subsystem.hpp"

namespace MayaFlux::Vruta {
class FrameClock;
}

namespace MayaFlux::Core {

class IGraphicsBackend;

/**
 * @class GraphicsSubsystem
 * @brief Backend agnostic graphics subsystem for visual processing
 *
 * Manages graphics thread, backend context, and frame-based processing.
 * Parallel to AudioSubsystem but with self-driven timing model.
 *
 * **Key Architectural Differences from AudioSubsystem:**
 *
 * AudioSubsystem:
 *   RTAudio callback → process() → scheduler.tick(samples)
 *   Clock is externally driven by audio hardware
 *
 * GraphicsSubsystem:
 *   Graphics thread loop → clock.tick() → process() → scheduler observes
 *   Clock is self-driven based on wall-clock time
 *
 * The FrameClock manages its own timing and the subsystem's process
 * methods are called from the graphics thread loop, not from an
 * external callback.
 */
class MAYAFLUX_API GraphicsSubsystem : public ISubsystem {
public:
    /**
     * @brief GraphicsSubsystem constructor
     * @param graphics_config Global graphics configuration
     */
    GraphicsSubsystem(const GlobalGraphicsConfig& graphics_config);
    ~GraphicsSubsystem() override;

    /**
     * @brief Initialize with graphics configuration
     * @param handle Processing handle for domain registration
     */
    void initialize(SubsystemProcessingHandle& handle) override;

    void register_callbacks() override;
    void start() override;
    void stop() override;
    void pause() override;
    void resume() override;

    [[nodiscard]] SubsystemTokens get_tokens() const override { return m_subsystem_tokens; }

    /**
     * @brief Get frame clock
     *
     * The FrameClock is self-driven and manages its own timing.
     * The scheduler reads from it but doesn't control it.
     */
    [[nodiscard]] Vruta::FrameClock& get_frame_clock() { return *m_frame_clock; }
    [[nodiscard]] const Vruta::FrameClock& get_frame_clock() const { return *m_frame_clock; }

    /**
     * @brief Get graphics thread ID
     */
    [[nodiscard]] std::thread::id get_graphics_thread_id() const
    {
        return m_graphics_thread_id;
    }

    /**
     * @brief Check if currently on graphics thread
     */
    [[nodiscard]] bool is_graphics_thread() const
    {
        return std::this_thread::get_id() == m_graphics_thread_id;
    }

    /**
     * @brief Get target frame rate
     */
    [[nodiscard]] uint32_t get_target_fps() const;

    /**
     * @brief Get actual measured FPS
     */
    [[nodiscard]] double get_measured_fps() const;

    /**
     * @brief Set target frame rate (can be changed at runtime)
     */
    void set_target_fps(uint32_t fps);

    /**
     * @brief Unified processing callback (alternative to separate methods)
     *
     * This is called once per frame and handles all processing:
     * - Visual nodes (VISUAL_RATE)
     * - Graphics buffers (GRAPHICS_BACKEND)
     * - Frame coroutines (FRAME_ACCURATE)
     *
     * Can be overridden or extended via hooks.
     */
    void process();

    /**
     * @brief Register markend windows from window manager for swapchain processing
     *
     * Creates surfaces and swapchains for each window.
     * Called during initialization and whenever new windows are created.
     */
    void register_windows_for_processing();

    /**
     * @brief Render all registered windows
     *
     * Acquires swapchain images, records command buffers,
     * submits to graphics queue, and presents.
     */
    void render_all_windows();

    bool is_ready() const override { return m_is_ready; }
    bool is_running() const override { return m_running.load(std::memory_order_acquire) && !m_paused.load(std::memory_order_acquire); }
    void shutdown() override;
    SubsystemType get_type() const override { return SubsystemType::GRAPHICS; }
    SubsystemProcessingHandle* get_processing_context_handle() override { return m_handle; }

    /**
     * @brief Get underlying graphics backend for advanced usage
     * Can be cast to VulkanBackend* or OpenGLBackend* for backend-specific operations
     */
    IGraphicsBackend* get_backend() { return m_backend.get(); }
    const IGraphicsBackend* get_backend() const { return m_backend.get(); }

    /**
     * @brief Get the type of the graphics backend
     * @return GraphicsBackendType enum value representing the backend type
     */
    GlobalGraphicsConfig::GraphicsApi get_backend_type() const { return m_graphics_config.requested_api; }

private:
    std::unique_ptr<IGraphicsBackend> m_backend;
    std::shared_ptr<Vruta::FrameClock> m_frame_clock;

    std::thread m_graphics_thread;
    std::thread::id m_graphics_thread_id;
    std::atomic<bool> m_running { false };
    std::atomic<bool> m_paused { false };

    std::vector<std::shared_ptr<Window>> m_registered_windows;

    bool m_is_ready {};

    SubsystemTokens m_subsystem_tokens; ///< Processing token configuration
    SubsystemProcessingHandle* m_handle; ///< Reference to processing handle
    GlobalGraphicsConfig m_graphics_config; ///< Graphics/windowing configuration

    /**
     * @brief Initialize Portal::Graphics subsystem
     *
     * Sets up all Portal::Graphics managers and resources.
     * Must be called after graphics backend initialization.
     */
    void initialize_graphics_portal();

    /**
     * @brief Register custom frame processor with scheduler
     *
     * This is the key integration point that makes graphics timing work.
     * Registers a custom processor for FRAME_ACCURATE token that:
     * - Does NOT tick the clock (already done)
     * - Just processes coroutines based on current clock position
     */
    void register_frame_processor();

    /**
     * @brief Graphics thread main loop
     *
     * Self-driven frame processing:
     * 1. Tick frame clock (advances based on wall-clock time)
     * 2. Process visual nodes (VISUAL_RATE nodes)
     * 3. Process graphics buffers (GRAPHICS_BACKEND buffers)
     * 4. Tick scheduler coroutines (FRAME_ACCURATE tasks)
     * 5. Record/submit Vulkan commands or OpenGL draw calls
     * 6. Wait for next frame (vsync timing)
     */
    void graphics_thread_loop();

    /**
     * @brief Process all FRAME_ACCURATE coroutines for given frames
     * @param tasks Vector of routines to process
     * @param processing_units Number of frames to process
     *
     * Advances the frame clock and processes all FRAME_ACCURATE tasks
     * that are ready to execute for each frame. This is called from
     * the graphics thread loop after ticking the frame clock.
     */
    void process_frame_coroutines_impl(const std::vector<std::shared_ptr<Vruta::Routine>>& tasks, uint64_t processing_units);

    /**
     * @brief Cleanup resources for windows that have been closed
     */
    void cleanup_closed_windows();

    /**
     * @brief Teardown all window resources during shutdown
     */
    void teardown_windows();
};
}
