#pragma once

#ifdef MAYAFLUX_PLATFORM_LINUX

#include "SystemBackend.hpp"

struct DBusConnection;

namespace MayaFlux::Core {

/**
 * @class DBusBackend
 * @brief Linux SystemBackend via the XDG Desktop Portal over D-Bus.
 *
 * Uses org.freedesktop.portal.FileChooser for file open and save dialogs.
 * The portal works correctly inside Flatpak sandboxes and across all
 * desktop environments that ship a portal implementation (GNOME, KDE, etc.).
 *
 * A private session bus connection is opened during initialize() and held
 * for the lifetime of the backend. Each dialog operation spawns a short-lived
 * thread that blocks on dbus_connection_read_write() until the portal's
 * Request object signals its Response, then fires the callback and exits.
 */
class MAYAFLUX_API DBusBackend final : public SystemBackend {
public:
    DBusBackend() = default;
    ~DBusBackend() override;

    DBusBackend(const DBusBackend&) = delete;
    DBusBackend& operator=(const DBusBackend&) = delete;
    DBusBackend(DBusBackend&&) noexcept = default;
    DBusBackend& operator=(DBusBackend&&) noexcept = default;

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
    DBusConnection* m_conn { nullptr };
    bool m_initialized { false };

    void invoke_portal(
        const char* method,
        FileDialogCallback callback,
        const std::vector<SystemFileFilter>& filters,
        const std::filesystem::path& start_dir,
        const std::string& suggested) const;
};

} // namespace MayaFlux::Core

#endif // MAYAFLUX_PLATFORM_LINUX
