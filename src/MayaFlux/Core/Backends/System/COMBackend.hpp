#pragma once

#ifdef MAYAFLUX_PLATFORM_WINDOWS

#include "SystemBackend.hpp"

namespace MayaFlux::Core {

/**
 * @class COMBackend
 * @brief Windows SystemBackend via IFileOpenDialog / IFileSaveDialog (Shell COM).
 *
 * Uses the modern Vista-era Shell COM interfaces from shobjidl_core.h.
 * CoInitializeEx is called once per call on the dispatch thread; the
 * dialog is modal and blocks until the user completes or dismisses it.
 * The result is delivered via callback before the thread exits.
 *
 * initialize() / shutdown() manage nothing persistent - COM is initialised
 * per-call on the dispatch thread. They exist to satisfy SystemBackend.
 */
class MAYAFLUX_API COMBackend final : public SystemBackend {
public:
    COMBackend() = default;
    ~COMBackend() override = default;

    COMBackend(const COMBackend&) = delete;
    COMBackend& operator=(const COMBackend&) = delete;
    COMBackend(COMBackend&&) noexcept = default;
    COMBackend& operator=(COMBackend&&) noexcept = default;

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

    void invoke_dialog(
        bool is_save,
        FileDialogCallback callback,
        const std::vector<SystemFileFilter>& filters,
        const std::filesystem::path& start_dir,
        const std::string& suggested_name);
};

} // namespace MayaFlux::Core

#endif // MAYAFLUX_PLATFORM_WINDOWS
