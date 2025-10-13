#pragma once

// Cross-platform definitions
#ifdef MAYAFLUX_PLATFORM_WINDOWS
using u_int = unsigned int;
using u_int8_t = uint8_t;
using u_int16_t = uint16_t;
using u_int32_t = uint32_t;
using u_int64_t = uint64_t;

#ifdef ERROR
#undef ERROR
#endif // ERROR
#define MAYAFLUX_FORCEINLINE __forceinline
#else
#define MAYAFLUX_FORCEINLINE __attribute__((always_inline))
#endif

// Unified export macro
#if defined(_WIN32) || defined(_WIN64)
#if defined(MAYAFLUX_STATIC)
#define MAYAFLUX_API
#elif defined(MAYAFLUX_EXPORTS)
#define MAYAFLUX_API __declspec(dllexport)
#elif defined(MAYAFLUX_IMPORTS)
#define MAYAFLUX_API __declspec(dllimport)
#else
#define MAYAFLUX_API
#endif
#else
#if defined(__GNUC__) && (__GNUC__ >= 4)
#if defined(MAYAFLUX_EXPORTS)
#define MAYAFLUX_API __attribute__((visibility("default")))
#else
#define MAYAFLUX_API
#endif
#else
#define MAYAFLUX_API
#endif
#endif

// C++20 std::format availability detection
// On macOS, we use fmt library due to runtime availability issues with macOS 14
#if defined(__APPLE__) && defined(__MACH__)
#define MAYAFLUX_USE_FMT 1
#define MAYAFLUX_USE_STD_FORMAT 0
#else
// Check if std::format is available on other platforms
#if __has_include(<format>) && defined(__cpp_lib_format)
#define MAYAFLUX_USE_STD_FORMAT 1
#define MAYAFLUX_USE_FMT 0
#else
#define MAYAFLUX_USE_FMT 1
#define MAYAFLUX_USE_STD_FORMAT 0
#endif
#endif

namespace MayaFlux::Platform {

#ifdef MAYAFLUX_PLATFORM_WINDOWS
constexpr char PathSeparator = '\\';
#else
constexpr char PathSeparator = '/';
#endif

class SystemConfig {
public:
    static std::string get_clang_resource_dir()
    {
#ifdef MAYAFLUX_PLATFORM_WINDOWS
        const char* cmd = "clang -print-resource-dir 2>nul";
#else
        const char* cmd = "clang -print-resource-dir 2>/dev/null";
#endif

        auto result = exec_command(cmd);
        if (!result.empty() && result.back() == '\n') {
            result.pop_back();
        }
        return result;
    }

    static std::vector<std::string> get_system_includes()
    {
        std::vector<std::string> includes;

#ifdef MAYAFLUX_PLATFORM_WINDOWS
        auto msvc_includes = get_msvc_includes();
        includes.insert(includes.end(), msvc_includes.begin(), msvc_includes.end());

        // Get Windows SDK includes
        auto sdk_includes = get_windows_sdk_includes();
        includes.insert(includes.end(), sdk_includes.begin(), sdk_includes.end());

        // Get Clang system includes
        const char* cmd = "clang -v -E -x c++ - 2>&1 <nul | findstr \"^ \\\\\"";
#else
        const char* cmd = "clang -v -E -x c++ - 2>&1 < /dev/null | grep \"^ /\"";
#endif

        auto clang_includes = exec_command_lines(cmd);
        includes.insert(includes.end(), clang_includes.begin(), clang_includes.end());

        return includes;
    }

private:
    static std::string exec_command(const char* cmd)
    {
        std::array<char, 128> buffer;
        std::string result;
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);

        if (!pipe)
            return "";

        while (fgets(buffer.data(), buffer.size(), pipe.get())) {
            result += buffer.data();
        }
        return result;
    }

    static std::vector<std::string> exec_command_lines(const char* cmd)
    {
        std::vector<std::string> lines;
        std::array<char, 128> buffer;

        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
        if (!pipe)
            return lines;

        while (fgets(buffer.data(), buffer.size(), pipe.get())) {
            std::string line(buffer.data());
            line.erase(0, line.find_first_not_of(" \t"));
            line.erase(line.find_last_not_of(" \t\n\r") + 1);

            if (!line.empty()) {
                lines.push_back(line);
            }
        }
        return lines;
    }

#ifdef MAYAFLUX_PLATFORM_WINDOWS
    static std::vector<std::string> get_msvc_includes()
    {
        std::vector<std::string> includes;

        // Try environment variables first
        char* vc_install_dir = nullptr;
        size_t len = 0;
        if (_dupenv_s(&vc_install_dir, &len, "VCInstallDir") == 0 && vc_install_dir) {
            includes.push_back(std::string(vc_install_dir) + "include");
            free(vc_install_dir);
        }

        // Fallback to registry query
        HKEY hKey;
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                "SOFTWARE\\Microsoft\\VisualStudio\\SxS\\VS7",
                0, KEY_READ, &hKey)
            == ERROR_SUCCESS) {
            char install_dir[1024];
            DWORD size = sizeof(install_dir);

            // Try to get latest version
            if (RegQueryValueExA(hKey, "17.0", nullptr, nullptr,
                    (LPBYTE)install_dir, &size)
                == ERROR_SUCCESS) {
                includes.push_back(std::string(install_dir) + "VC\\Tools\\MSVC\\14.\\include");
            }
            RegCloseKey(hKey);
        }

        return includes;
    }

    static std::vector<std::string> get_windows_sdk_includes()
    {
        std::vector<std::string> includes;

        char* windows_sdk_dir = nullptr;
        size_t len = 0;
        if (_dupenv_s(&windows_sdk_dir, &len, "WindowsSdkDir") == 0 && windows_sdk_dir) {
            includes.push_back(std::string(windows_sdk_dir) + "Include");
            free(windows_sdk_dir);
        }

        return includes;
    }
#endif
};

} // namespace MayaFlux::Platform
