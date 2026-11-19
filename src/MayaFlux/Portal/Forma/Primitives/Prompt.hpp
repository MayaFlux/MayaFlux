#pragma once

namespace MayaFlux::Portal::Forma {

class Surface;
class FormaTarget;

/**
 * @brief Receives the accepted text, or nullopt when the prompt was cancelled or closed.
 */
using AskCallback = std::function<void(std::optional<std::string>)>;

/**
 * @brief Decides whether a text may be accepted.
 */
using AskValidator = std::function<bool(std::string_view)>;

/**
 * @brief Open a one line text prompt and report what the user typed.
 *
 * Requires Portal::Forma to be initialized and the engine running. The window
 * holds one text field with the focus, started on @p initial with the cursor
 * at its end. Enter accepts the text, Escape cancels, and closing the window
 * cancels. The callback fires exactly once, on the event thread, so it must
 * not block, and the window closes as it fires.
 *
 * When a @p validator is given it runs on every edit. Text it refuses is drawn
 * in a warning colour and Enter does nothing until the text is accepted, so the
 * callback only ever receives text the validator took.
 *
 * @param title     Window title, with a note that Enter accepts and Escape cancels.
 * @param initial   Starting text.
 * @param validator Accepts or refuses the text. Empty accepts everything.
 * @param callback  Invoked once with the text or nullopt.
 */
MAYAFLUX_API void ask(
    std::string title,
    std::string initial,
    AskValidator validator,
    AskCallback callback);

/**
 * @brief ask() with no validator.
 */
MAYAFLUX_API void ask(
    std::string title,
    std::string initial,
    AskCallback callback);

namespace internal {

    /**
     * @brief The prompt itself, placed on a surface that already exists.
     *
     * ask() builds a window and a surface and calls this. It places the field
     * in the whole surface, gives it focus, and requests the surface's window
     * to close when the prompt finishes. Separate so the behaviour can be
     * driven against a scripted window.
     *
     * @param surface   Surface to place the field on.
     * @param target    Where the field's text is drawn.
     * @param row_px    Height of the field in pixels, for the text layout.
     * @param initial   Starting text.
     * @param validator Accepts or refuses the text. Empty accepts everything.
     * @param callback  Invoked once with the text or nullopt.
     * @return Callable that finishes the prompt as cancelled, for a window
     *         that closes without the field's own Escape.
     */
    MAYAFLUX_API std::function<void()> ask_in(
        Surface& surface,
        FormaTarget target,
        uint32_t row_px,
        std::string initial,
        AskValidator validator,
        AskCallback callback);

} // namespace internal

} // namespace MayaFlux::Portal::Forma
