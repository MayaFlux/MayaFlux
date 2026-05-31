#pragma once

#include <expected>

namespace MayaFlux::Core {

/**
 * @enum SystemDialogError
 * @brief Failure modes for OS dialog operations.
 */
enum class SystemDialogError : uint8_t {
    Cancelled, ///< User dismissed the dialog without completing it.
    BackendError, ///< Platform backend failed to open or communicate.
    NotSupported, ///< No implementation available on this platform build.
};

/**
 * @struct SystemFileFilter
 * @brief A named group of accepted file extensions for dialog filters.
 */
struct SystemFileFilter {
    std::string name;
    std::vector<std::string> extensions;
};

/**
 * @brief Result type delivered to all file dialog callbacks.
 */
using FileDialogResult = std::expected<std::filesystem::path, SystemDialogError>;

/**
 * @brief Callback type for all file dialog operations.
 */
using FileDialogCallback = std::function<void(FileDialogResult)>;

/**
 * @class SystemBackend
 * @brief Abstract interface for native OS service backends.
 *
 * Implemented per platform: DBusBackend (Linux), COMBackend (Windows),
 * NSBackend (macOS). Portal::System owns a single instance and routes
 * all OS surface calls through it.
 *
 * Lifecycle is minimal: initialize() acquires any persistent platform
 * resource (e.g. a D-Bus session connection); shutdown() releases it.
 * There is no start()/stop() because no continuous thread is driven
 * at this level - each operation is self-contained.
 *
 * All dialog operations are non-blocking at the interface level.
 * The callback is invoked exactly once per call, on whichever thread
 * the platform backend uses to deliver its response. Callers that need
 * thread affinity must marshal the result themselves.
 */
class MAYAFLUX_API SystemBackend {
public:
    virtual ~SystemBackend() = default;

    SystemBackend() = default;
    SystemBackend(const SystemBackend&) = delete;
    SystemBackend& operator=(const SystemBackend&) = delete;
    SystemBackend(SystemBackend&&) noexcept = default;
    SystemBackend& operator=(SystemBackend&&) noexcept = default;

    /**
     * @brief Acquire any persistent platform resources required by this backend.
     * @return True if the backend is ready for use.
     */
    [[nodiscard]] virtual bool initialize() = 0;

    /**
     * @brief Release all platform resources held by this backend.
     */
    virtual void shutdown() = 0;

    /**
     * @brief Return true if initialize() has completed successfully.
     */
    [[nodiscard]] virtual bool is_initialized() const = 0;

    /**
     * @brief Present a native open-file dialog.
     *
     * @param callback    Invoked with the chosen path or a SystemDialogError.
     * @param filters     Extension filter groups shown in the dialog.
     * @param start_dir   Directory the dialog opens in. Platform default if empty.
     */
    virtual void open_file(
        FileDialogCallback callback,
        std::vector<SystemFileFilter> filters,
        std::filesystem::path start_dir) = 0;

    /**
     * @brief Present a native save-file dialog.
     *
     * @param callback        Invoked with the chosen path or a SystemDialogError.
     * @param suggested_name  Filename pre-filled in the dialog name field.
     * @param filters         Extension filter groups shown in the dialog.
     * @param start_dir       Directory the dialog opens in. Platform default if empty.
     */
    virtual void save_file(
        FileDialogCallback callback,
        std::string suggested_name,
        std::vector<SystemFileFilter> filters,
        std::filesystem::path start_dir) = 0;
};

} // namespace MayaFlux::Core
