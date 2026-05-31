#pragma once

#ifdef MAYAFLUX_PLATFORM_MACOS

#include "SystemBackend.hpp"

namespace MayaFlux::Core {

/**
 * @class NSBackend
 * @brief macOS SystemBackend via NSOpenPanel / NSSavePanel (AppKit).
 *
 * AppKit panel calls must run on the main thread. Each dialog operation
 * marshals onto the main dispatch queue via Parallel::dispatch_main_sync
 * and blocks the calling thread until the user completes or dismisses.
 * The result is delivered via callback before returning to the caller.
 *
 * Implementation lives in NSBackend.mm (Objective-C++) to access AppKit.
 */
class MAYAFLUX_API NSBackend final : public SystemBackend {
public:
    NSBackend() = default;
    ~NSBackend() override = default;

    NSBackend(const NSBackend&) = delete;
    NSBackend& operator=(const NSBackend&) = delete;
    NSBackend(NSBackend&&) noexcept = default;
    NSBackend& operator=(NSBackend&&) noexcept = default;

    [[nodiscard]] bool initialize() override;
    void shutdown() override;
    [[nodiscard]] bool is_initialized() const override { return m_initialized; }

    void open_file(
        FileDialogCallback callback,
        std::vector<SystemFileFilter> filters,
        std::filesystem::path start_dir) override;

    void save_file(
        FileDialogCallback callback,
        std::string suggested_name,
        std::vector<SystemFileFilter> filters,
        std::filesystem::path start_dir) override;

private:
    bool m_initialized { false };
};

} // namespace MayaFlux::Core

#endif // MAYAFLUX_PLATFORM_MACOS
