#ifdef MAYAFLUX_PLATFORM_WINDOWS

#include "COMBackend.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

#include <shobjidl_core.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#ifdef ERROR
#undef ERROR
#endif // ERROR

namespace MayaFlux::Core {

namespace {

    /**
     * @brief Convert a UTF-8 std::string to a UTF-16 std::wstring.
     */
    std::wstring to_wide(const std::string& s)
    {
        if (s.empty()) {
            return {};
        }
        const int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
        std::wstring result(len - 1, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, result.data(), len);
        return result;
    }

    /**
     * @brief Convert a UTF-16 std::wstring to a UTF-8 std::string.
     */
    std::string to_narrow(const std::wstring& ws)
    {
        if (ws.empty()) {
            return {};
        }
        const int len = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, nullptr, 0, nullptr, nullptr);
        std::string result(len - 1, '\0');
        WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, result.data(), len, nullptr, nullptr);
        return result;
    }

    /**
     * @brief Build a COMDLG_FILTERSPEC array from SystemFileFilter entries.
     *
     * Both the name and pattern wstrings must outlive the COMDLG_FILTERSPEC array,
     * so they are returned as parallel vectors that the caller keeps alive.
     */
    void build_filter_specs(
        const std::vector<SystemFileFilter>& filters,
        std::vector<std::wstring>& names,
        std::vector<std::wstring>& patterns,
        std::vector<COMDLG_FILTERSPEC>& specs)
    {
        names.reserve(filters.size());
        patterns.reserve(filters.size());
        specs.reserve(filters.size());

        for (const auto& f : filters) {
            names.push_back(to_wide(f.name));

            std::wstring pattern;
            for (size_t i = 0; i < f.extensions.size(); ++i) {
                if (i > 0) {
                    pattern += L';';
                }
                pattern += (f.extensions[i] == "*") ? L"*.*" : L"*." + to_wide(f.extensions[i]);
            }
            patterns.push_back(std::move(pattern));

            specs.push_back({ names.back().c_str(), patterns.back().c_str() });
        }
    }

    /**
     * @brief Run one COM file dialog on the calling thread.
     *
     * CoInitializeEx / CoUninitialize are called here so the dispatch
     * thread owns its own COM apartment. Fires @p callback before returning.
     */
    void run_dialog(
        bool is_save,
        FileDialogCallback callback,
        const std::vector<SystemFileFilter>& filters,
        const std::filesystem::path& start_dir,
        const std::string& suggested_name)
    {
        HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        if (FAILED(hr)) {
            MF_ERROR(Journal::Component::Core, Journal::Context::API,
                "COMBackend: CoInitializeEx failed: 0x{:08X}", static_cast<uint32_t>(hr));
            callback(std::unexpected(SystemDialogError::BackendError));
            return;
        }

        IFileDialog* dialog = nullptr;
        const CLSID clsid = is_save ? CLSID_FileSaveDialog : CLSID_FileOpenDialog;
        const IID iid = is_save ? IID_IFileSaveDialog : IID_IFileOpenDialog;

        hr = CoCreateInstance(clsid, nullptr, CLSCTX_ALL, iid,
            reinterpret_cast<void**>(&dialog));

        if (FAILED(hr) || !dialog) {
            MF_ERROR(Journal::Component::Core, Journal::Context::API,
                "COMBackend: CoCreateInstance failed: 0x{:08X}", static_cast<uint32_t>(hr));
            CoUninitialize();
            callback(std::unexpected(SystemDialogError::BackendError));
            return;
        }

        std::vector<std::wstring> names, pattern_strings;
        std::vector<COMDLG_FILTERSPEC> specs;
        build_filter_specs(filters, names, pattern_strings, specs);
        if (!specs.empty()) {
            dialog->SetFileTypes(static_cast<UINT>(specs.size()), specs.data());
        }

        if (is_save && !suggested_name.empty()) {
            dialog->SetFileName(to_wide(suggested_name).c_str());
        }

        if (!start_dir.empty()) {
            IShellItem* folder = nullptr;
            hr = SHCreateItemFromParsingName(start_dir.wstring().c_str(), nullptr,
                IID_IShellItem, reinterpret_cast<void**>(&folder));
            if (SUCCEEDED(hr) && folder) {
                dialog->SetFolder(folder);
                folder->Release();
            }
        }

        hr = dialog->Show(nullptr);

        if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
            dialog->Release();
            CoUninitialize();
            callback(std::unexpected(SystemDialogError::Cancelled));
            return;
        }

        if (FAILED(hr)) {
            MF_ERROR(Journal::Component::Core, Journal::Context::API,
                "COMBackend: dialog Show failed: 0x{:08X}", static_cast<uint32_t>(hr));
            dialog->Release();
            CoUninitialize();
            callback(std::unexpected(SystemDialogError::BackendError));
            return;
        }

        IShellItem* result = nullptr;
        hr = dialog->GetResult(&result);

        if (FAILED(hr) || !result) {
            dialog->Release();
            CoUninitialize();
            callback(std::unexpected(SystemDialogError::BackendError));
            return;
        }

        PWSTR path_raw = nullptr;
        hr = result->GetDisplayName(SIGDN_FILESYSPATH, &path_raw);
        result->Release();
        dialog->Release();

        if (FAILED(hr) || !path_raw) {
            CoUninitialize();
            callback(std::unexpected(SystemDialogError::BackendError));
            return;
        }

        std::filesystem::path chosen_path(path_raw);
        CoTaskMemFree(path_raw);
        CoUninitialize();

        callback(std::move(chosen_path));
    }

} // namespace

// =============================================================================
// Lifecycle
// =============================================================================

bool COMBackend::initialize()
{
    if (m_initialized) {
        return true;
    }
    m_initialized = true;
    MF_INFO(Journal::Component::Core, Journal::Context::API,
        "COMBackend initialized");
    return true;
}

void COMBackend::shutdown()
{
    if (!m_initialized) {
        return;
    }
    m_initialized = false;
    MF_INFO(Journal::Component::Core, Journal::Context::API,
        "COMBackend shutdown");
}

// =============================================================================
// Dialog operations
// =============================================================================

void COMBackend::open_file(
    FileDialogCallback callback,
    std::vector<SystemFileFilter> filters,
    std::filesystem::path start_dir)
{
    invoke_dialog(false, std::move(callback), filters, start_dir, {});
}

void COMBackend::save_file(
    FileDialogCallback callback,
    std::string suggested_name,
    std::vector<SystemFileFilter> filters,
    std::filesystem::path start_dir)
{
    invoke_dialog(true, std::move(callback), filters, start_dir, suggested_name);
}

void COMBackend::invoke_dialog(
    bool is_save,
    FileDialogCallback callback,
    const std::vector<SystemFileFilter>& filters,
    const std::filesystem::path& start_dir,
    const std::string& suggested_name)
{
    if (!m_initialized) {
        callback(std::unexpected(SystemDialogError::BackendError));
        return;
    }

    std::thread(run_dialog, is_save, std::move(callback), filters, start_dir, suggested_name).detach();
}

} // namespace MayaFlux::Core

#endif // MAYAFLUX_PLATFORM_WINDOWS
