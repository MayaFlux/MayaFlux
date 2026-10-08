#pragma once

#ifdef MAYAFLUX_PLATFORM_MACOS

#include "MayaFlux/Core/Backends/Windowing/Window.hpp"
#include "MayaFlux/Vruta/WindowEventSource.hpp"

#include "MayaFlux/Transitive/Memory/RingBuffer.hpp"

namespace MayaFlux::Core {

/**
 * @class CocoaWindow
 * @brief Native AppKit window backend, no GLFW dependency.
 *
 * Usable on its own, without WindowManager, GraphicsSubsystem, or Engine.
 * Window operations may be called from any thread. Native events are queued
 * per window and delivered to the callback and event source only from poll(),
 * on whichever thread calls it. Fullscreen is a borderless window covering the
 * target display. Raw mouse motion is not supported; CAPTURED behaves as DISABLED.
 */
class MAYAFLUX_API CocoaWindow : public Window {
public:
    CocoaWindow(const WindowCreateInfo& create_info,
        const GlobalGraphicsConfig& graphics_config);

    ~CocoaWindow() override;

    CocoaWindow(const CocoaWindow&) = delete;
    CocoaWindow& operator=(const CocoaWindow&) = delete;
    CocoaWindow(CocoaWindow&&) = delete;
    CocoaWindow& operator=(CocoaWindow&&) = delete;

    void show() override;
    void hide() override;
    void destroy() override;
    void poll() override;

    [[nodiscard]] bool should_close() const override;

    [[nodiscard]] const WindowState& get_state() const override { return m_state; }
    [[nodiscard]] const WindowCreateInfo& get_create_info() const override { return m_create_info; }

    void set_input_config(const InputConfig& config) override;
    [[nodiscard]] const InputConfig& get_input_config() const override { return m_input_config; }

    void set_event_callback(WindowEventCallback callback) override;

    /**
     * @brief Get the NSWindow.
     * @return NSWindow pointer, or nullptr after destroy().
     */
    [[nodiscard]] void* get_native_handle() const override;

    [[nodiscard]] void* get_native_display() const override { return nullptr; }

    /**
     * @brief Get the CAMetalLayer backing the content view.
     * @return CAMetalLayer pointer for vkCreateMetalSurfaceEXT, or nullptr after destroy().
     */
    [[nodiscard]] void* get_metal_layer() const;

    void set_title(const std::string& title) override;
    void set_size(uint32_t width, uint32_t height) override;
    void set_position(uint32_t x, uint32_t y) override;
    void set_color(const std::array<float, 4>& color) override;

    Vruta::WindowEventSource& get_event_source() override { return m_event_source; }
    [[nodiscard]] const Vruta::WindowEventSource& get_event_source() const override { return m_event_source; }

    [[nodiscard]] bool is_graphics_registered() const override { return m_graphics_registered.load(); }
    void set_graphics_registered(bool registered) override { m_graphics_registered.store(registered); }

    void register_rendering_buffer(std::shared_ptr<Buffers::VKBuffer> buffer) override;
    void unregister_rendering_buffer(std::shared_ptr<Buffers::VKBuffer> buffer) override;
    void track_frame_command(uint64_t cmd_id) override;
    [[nodiscard]] const std::vector<uint64_t>& get_frame_commands() const override;
    void clear_frame_commands() override;
    [[nodiscard]] std::vector<std::shared_ptr<Buffers::VKBuffer>> get_rendering_buffers() const override;

    [[nodiscard]] bool is_capture_enabled() const override { return m_capture_enabled.load(std::memory_order_acquire); }
    void set_capture_enabled(bool enabled) override { m_capture_enabled.store(enabled, std::memory_order_release); }

private:
    struct Native;

    std::shared_ptr<Native> m_native;

    std::atomic<bool> m_should_close { false };
    std::atomic<bool> m_graphics_registered { false };
    std::atomic<bool> m_capture_enabled { false };

    static constexpr size_t EVENT_QUEUE_CAPACITY = 256;

    Memory::LockFreeQueue<WindowEvent, EVENT_QUEUE_CAPACITY> m_event_queue;

    WindowCreateInfo m_create_info;
    WindowState m_state;
    InputConfig m_input_config;
    WindowEventCallback m_event_callback;

    Vruta::WindowEventSource m_event_source;

    std::vector<std::weak_ptr<Buffers::VKBuffer>> m_rendering_buffers;
    std::vector<uint64_t> m_frame_commands;
    mutable std::mutex m_render_tracking_mutex;

    void push_event(WindowEvent ev);
};

} // namespace MayaFlux::Core

#endif // MAYAFLUX_PLATFORM_MACOS
