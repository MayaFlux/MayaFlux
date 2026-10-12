#pragma once

#ifdef MAYAFLUX_PLATFORM_MACOS

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

/**
 * @namespace MayaFlux::Test::CocoaInput
 * @brief Delivers synthetic AppKit input and notifications to a CocoaWindow's native window.
 *
 * Every function takes the NSWindow returned by CocoaWindow::get_native_handle()
 * and must be called on the main thread. Events are handed straight to the
 * window's content view or delegate, so delivery does not depend on the
 * application being active or the window being key. Positions are top-left
 * view coordinates in points.
 */
namespace MayaFlux::Test::CocoaInput {

/**
 * @brief Modifier flags accepted by the event helpers.
 */
enum class Modifier : uint8_t {
    None = 0,
    Shift = 1U << 0,
    Control = 1U << 1,
    Option = 1U << 2,
    Command = 1U << 3,
    CapsLock = 1U << 4,
};

[[nodiscard]] constexpr Modifier operator|(Modifier a, Modifier b) noexcept
{
    return static_cast<Modifier>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

/**
 * @brief True when a window server session with an active display exists.
 */
[[nodiscard]] bool display_available();

/**
 * @brief Run the main run loop briefly so queued main-queue blocks execute.
 */
void pump_main_queue();

/**
 * @brief Keep an NSWindow alive independently of its CocoaWindow.
 * @param window NSWindow handle.
 * @return Owning reference released when the pointer is destroyed.
 */
[[nodiscard]] std::shared_ptr<void> retain(void* window);

void key_down(void* window, uint16_t key_code, Modifier mods = Modifier::None, bool is_repeat = false);
void key_up(void* window, uint16_t key_code, Modifier mods = Modifier::None);
void flags_changed(void* window, uint16_t key_code, Modifier flags);

void mouse_move(void* window, double x, double y);
void mouse_button(void* window, int button, bool down, Modifier mods = Modifier::None);
void mouse_entered(void* window);
void mouse_exited(void* window);

/**
 * @brief Deliver a scroll event with continuous (trackpad style) pixel deltas.
 * @param delta_x Raw AppKit horizontal delta.
 * @param delta_y Raw AppKit vertical delta.
 */
void scroll_precise(void* window, int32_t delta_x, int32_t delta_y);

/**
 * @brief Deliver a scroll event with discrete line deltas (wheel style).
 */
void scroll_lines(void* window, int32_t delta_x, int32_t delta_y);

/**
 * @brief Commit text through the view's NSTextInputClient interface.
 */
void insert_text(void* window, std::u16string_view text);

void focus_gained(void* window);
void focus_lost(void* window);
void minimized(void* window);
void restored(void* window);
void live_resize_began(void* window);
void live_resize_ended(void* window);

/**
 * @brief Ask the delegate whether the window should close, as the close button does.
 * @return The delegate's answer.
 */
bool close_requested(void* window);

[[nodiscard]] std::string title(void* window);
[[nodiscard]] bool is_visible(void* window);
[[nodiscard]] bool has_delegate(void* window);
[[nodiscard]] std::pair<double, double> content_size(void* window);

/**
 * @brief Content area origin in top-left coordinates of the primary display.
 */
[[nodiscard]] std::pair<double, double> content_origin(void* window);

[[nodiscard]] double backing_scale(void* window);
[[nodiscard]] bool is_metal_layer(void* layer);
[[nodiscard]] double layer_scale(void* layer);

}

#endif
