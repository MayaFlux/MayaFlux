#pragma once

#ifdef MAYAFLUX_PLATFORM_WINDOWS

#include "MayaFlux/Core/Backends/Windowing/Window.hpp"
#include "MayaFlux/Vruta/WindowEventSource.hpp"

#include "MayaFlux/Transitive/Memory/RingBuffer.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace MayaFlux::Core {

/**
 * @class Win32Window
 * @brief Native Win32 window backend, no GLFW dependency.
 *
 * The Win32 message loop runs on a dedicated UI thread spawned in the
 * constructor. WndProc enqueues WindowEvents under m_event_mutex; poll()
 * drains the queue on the graphics thread and signals m_event_source.
 * WindowManager::process() calls poll() instead of glfwPollEvents() when
 * the NATIVE windowing backend is selected.
 */
class MAYAFLUX_API Win32Window : public Window {
public:
    Win32Window(const WindowCreateInfo& create_info,
        const GlobalGraphicsConfig& graphics_config);

    ~Win32Window() override;

    Win32Window(const Win32Window&) = delete;
    Win32Window& operator=(const Win32Window&) = delete;
    Win32Window(Win32Window&&) = delete;
    Win32Window& operator=(Win32Window&&) = delete;

    void show() override;
    void hide() override;
    void destroy() override;
    void poll() override;

    [[nodiscard]] bool should_close() const override;

    [[nodiscard]] const WindowState& get_state() const override { return m_state; }
    [[nodiscard]] const WindowCreateInfo& get_create_info() const override { return m_create_info; }

    void set_input_config(const InputConfig& config) override { m_input_config = config; }
    [[nodiscard]] const InputConfig& get_input_config() const override { return m_input_config; }

    void set_event_callback(WindowEventCallback callback) override;

    [[nodiscard]] void* get_native_handle() const override { return m_hwnd; }
    [[nodiscard]] void* get_native_display() const override { return nullptr; }

    void set_title(const std::string& title) override;
    void set_size(uint32_t width, uint32_t height) override;
    void set_position(uint32_t x, uint32_t y) override;
    void set_color(const std::array<float, 4>& color) override;

    static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

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

private:
    HWND m_hwnd { nullptr };
    HINSTANCE m_hinstance { nullptr };

    std::thread m_ui_thread;
    std::atomic<bool> m_hwnd_ready { false };
    std::atomic<bool> m_should_close { false };

    static constexpr size_t EVENT_QUEUE_CAPACITY = 256;

    Memory::LockFreeQueue<WindowEvent, EVENT_QUEUE_CAPACITY> m_event_queue;

    KeyRepeatConfig m_key_repeat_config;
    WindowCreateInfo m_create_info;
    WindowState m_state;
    InputConfig m_input_config;
    WindowEventCallback m_event_callback;

    std::atomic<bool> m_graphics_registered { false };
    Vruta::WindowEventSource m_event_source;
    std::atomic<bool> m_keys_dirty { false };
    std::unordered_map<int16_t, WindowEvent::KeyData> m_held_keys_ui;
    std::unordered_map<int16_t, WindowEvent::KeyData> m_held_keys;
    ULONGLONG m_repeat_next_tick { 0 };

    std::vector<std::weak_ptr<Buffers::VKBuffer>> m_rendering_buffers;
    std::vector<uint64_t> m_frame_commands;
    mutable std::mutex m_render_tracking_mutex;

    void ui_thread_main();
    void push_event(WindowEvent ev);
};

} // namespace MayaFlux::Core

#endif // MAYAFLUX_PLATFORM_WINDOWS
