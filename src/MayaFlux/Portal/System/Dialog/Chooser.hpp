#pragma once

#include "MayaFlux/Core/Backends/System/SystemBackend.hpp"

#include <future>

namespace MayaFlux::Portal::System::Dialog {

/**
 * @brief Result type for the primitive callback overload.
 */
using ChooserResult = Core::FileDialogResult;

/**
 * @brief Callback type for the primitive callback overload.
 */
using ChooserCallback = Core::FileDialogCallback;

/**
 * @brief A named group of accepted file extensions.
 *
 * @code
 * ChooserFilter{ "Audio", { "wav", "flac", "aiff" } }
 * ChooserFilter{ "All Files", { "*" } }
 * @endcode
 */
using ChooserFilter = Core::SystemFileFilter;

// =============================================================================
// open_file
// =============================================================================

/**
 * @brief Present a native open-file dialog, delivering the result via @p callback.
 *
 * Primitive overload. The callback receives a ChooserResult and is responsible
 * for checking and unwrapping it. Invoked exactly once on the platform backend
 * thread; callers that need thread affinity must marshal the result themselves.
 *
 * @param callback  Invoked with the chosen path or a SystemDialogError.
 * @param filters   Extension filter groups shown in the dialog.
 * @param start_dir Directory the dialog opens in. Platform default if empty.
 */
MAYAFLUX_API void open_file(
    ChooserCallback callback,
    std::vector<ChooserFilter> filters = {},
    std::filesystem::path start_dir = {});

/**
 * @brief Present a native open-file dialog, returning @p T produced by @p on_success.
 *
 * Blocks until the user completes or dismisses the dialog. @p on_success is
 * called with the chosen path and its return value is returned directly to the
 * caller. @p on_error is called on cancellation or backend failure; in that
 * case the return value is a default-constructed @p T.
 *
 * @code
 * auto tex = open_file<std::shared_ptr<TextureBuffer>>(
 *     [](std::filesystem::path p) {
 *         IO::ImageReader r;
 *         r.open(p.string());
 *         return r.create_texture_buffer();
 *     },
 *     [](Core::SystemDialogError) {}
 * );
 * @endcode
 *
 * @tparam T        Return type of @p on_success.
 * @param on_success Called with the chosen path on success. Must return T.
 * @param on_error   Called with the error on cancellation or failure.
 * @param filters    Extension filter groups shown in the dialog.
 * @param start_dir  Directory the dialog opens in. Platform default if empty.
 * @return           The value returned by @p on_success, or a default T on error.
 *
 * @todo Post-0.4: when C++23 is the minimum, add a non-blocking overload using
 *       std::expected::transform / and_then on a future-returning variant.
 */
template <typename T>
T open_file(
    std::function<T(std::filesystem::path)> on_success,
    std::function<void(Core::SystemDialogError)> on_error,
    std::vector<ChooserFilter> filters = {},
    std::filesystem::path start_dir = {})
{
    std::promise<T> promise;
    auto future = promise.get_future();

    open_file(
        [&promise, on_success = std::move(on_success), on_error = std::move(on_error)](ChooserResult result) {
            if (result) {
                promise.set_value(on_success(std::move(*result)));
            } else {
                on_error(result.error());
                promise.set_value(T {});
            }
        },
        std::move(filters),
        std::move(start_dir));

    return future.get();
}

// =============================================================================
// save_file
// =============================================================================

/**
 * @brief Present a native save-file dialog, delivering the result via @p callback.
 *
 * Primitive overload.
 *
 * @param callback        Invoked with the chosen path or a SystemDialogError.
 * @param suggested_name  Filename pre-filled in the dialog name field.
 * @param filters         Extension filter groups shown in the dialog.
 * @param start_dir       Directory the dialog opens in. Platform default if empty.
 */
MAYAFLUX_API void save_file(
    ChooserCallback callback,
    std::string suggested_name = {},
    std::vector<ChooserFilter> filters = {},
    std::filesystem::path start_dir = {});

/**
 * @brief Present a native save-file dialog, returning @p T produced by @p on_success.
 *
 * Blocks until the user completes or dismisses the dialog.
 *
 * @tparam T          Return type of @p on_success.
 * @param on_success  Called with the chosen path on success. Must return T.
 * @param on_error    Called with the error on cancellation or failure.
 * @param suggested_name Filename pre-filled in the dialog name field.
 * @param filters     Extension filter groups shown in the dialog.
 * @param start_dir   Directory the dialog opens in. Platform default if empty.
 * @return            The value returned by @p on_success, or a default T on error.
 *
 * @todo Post-0.4: when C++23 is the minimum, add a non-blocking overload using
 *       std::expected::transform / and_then on a future-returning variant.
 */
template <typename T>
T save_file(
    std::function<T(std::filesystem::path)> on_success,
    std::function<void(Core::SystemDialogError)> on_error,
    std::string suggested_name = {},
    std::vector<ChooserFilter> filters = {},
    std::filesystem::path start_dir = {})
{
    std::promise<T> promise;
    auto future = promise.get_future();

    save_file(
        [&promise, on_success = std::move(on_success), on_error = std::move(on_error)](ChooserResult result) {
            if (result) {
                promise.set_value(on_success(std::move(*result)));
            } else {
                on_error(result.error());
                promise.set_value(T {});
            }
        },
        std::move(suggested_name),
        std::move(filters),
        std::move(start_dir));

    return future.get();
}

} // namespace MayaFlux::Portal::System::Dialog
