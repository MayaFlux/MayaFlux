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

        if (!result.empty() && result.find("clang") != std::string::npos) {
            return result;
        }

        return "";
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

        // Try vswhere.exe (VS2017+)
        const char* vswhere_cmd = "\"C:\\Program Files (x86)\\Microsoft Visual Studio\\Installer\\vswhere.exe\" "
                                  "-latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 "
                                  "-property installationPath 2>nul";

        std::string vs_path = exec_command(vswhere_cmd);
        if (!vs_path.empty()) {
            vs_path.erase(std::remove(vs_path.begin(), vs_path.end(), '\r'), vs_path.end());
            vs_path.erase(std::remove(vs_path.begin(), vs_path.end(), '\n'), vs_path.end());

            std::string msvc_base = vs_path + "\\VC\\Tools\\MSVC";
            WIN32_FIND_DATAA fd;
            HANDLE hFind = FindFirstFileA((msvc_base + "\\*").c_str(), &fd);
            std::string latest;
            if (hFind != INVALID_HANDLE_VALUE) {
                do {
                    if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && fd.cFileName[0] >= '0' && fd.cFileName[0] <= '9') {
                        if (fd.cFileName > latest)
                            latest = fd.cFileName;
                    }
                } while (FindNextFileA(hFind, &fd));
                FindClose(hFind);
            }

            if (!latest.empty()) {
                includes.push_back(msvc_base + "\\" + latest + "\\include");
                std::string atlmfc = msvc_base + "\\" + latest + "\\atlmfc\\include";
                if (GetFileAttributesA(atlmfc.c_str()) != INVALID_FILE_ATTRIBUTES)
                    includes.push_back(atlmfc);
            }
        }

        // Fallback to environment
        if (includes.empty()) {
            char* dir = nullptr;
            size_t len = 0;
            if (_dupenv_s(&dir, &len, "VCInstallDir") == 0 && dir) {
                includes.push_back(std::string(dir) + "\\include");
                free(dir);
            }
        }

        return includes;
    }

    static std::vector<std::string> get_windows_sdk_includes()
    {
        std::vector<std::string> includes;
        char *sdk_dir = nullptr, *sdk_ver = nullptr;
        size_t len = 0;

        if (_dupenv_s(&sdk_dir, &len, "WindowsSdkDir") == 0 && sdk_dir) {
            std::string base = sdk_dir;
            free(sdk_dir);
            if (_dupenv_s(&sdk_ver, &len, "WindowsSDKVersion") == 0 && sdk_ver) {
                std::string ver = sdk_ver;
                free(sdk_ver);
                if (!ver.empty() && ver.back() == '\\')
                    ver.pop_back();
                includes = {
                    base + "Include\\" + ver + "\\ucrt",
                    base + "Include\\" + ver + "\\shared",
                    base + "Include\\" + ver + "\\um",
                    base + "Include\\" + ver + "\\winrt",
                    base + "Include\\" + ver + "\\cppwinrt"
                };
            }
        }

        return includes;
    }
#endif
};

} // namespace MayaFlux::Platform
