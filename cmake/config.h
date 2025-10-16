#pragma once
#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

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

// #ifdef MAYAFLUX_PLATFORM_WINDOWS
//     #if defined(MAYAFLUX_EXPORTS)
//         #define MAYAFLUX_API __declspec(dllexport)
//     #else
//         #define MAYAFLUX_API __declspec(dllimport)
//     #endif
// #else
//     #if defined(__GNUC__) && (__GNUC__ >= 4)
//         #if defined(MAYAFLUX_EXPORTS)
//             #define MAYAFLUX_API __attribute__((visibility("default")))
//         #else
//             #define MAYAFLUX_API
//         #endif
//     #else
//         #define MAYAFLUX_API
//     #endif
// #endif

#define MAYAFLUX_API

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

        if (!result.empty() && (result.find("clang") != std::string::npos || result.find("lib") != std::string::npos)) {
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

        auto sdk_includes = get_windows_sdk_includes();
        includes.insert(includes.end(), sdk_includes.begin(), sdk_includes.end());
#endif

        const char* cmd =
#ifdef MAYAFLUX_PLATFORM_WINDOWS
            "clang -v -E -x c++ - 2>&1 <nul";
#else
            "clang -v -E -x c++ - 2>&1 < /dev/null";
#endif

        auto clang_output = exec_command(cmd);
        auto clang_includes = parse_clang_includes(clang_output);
        includes.insert(includes.end(), clang_includes.begin(), clang_includes.end());

        return includes;
    }

private:
    static std::string exec_command(const char* cmd)
    {
        std::array<char, 128> buffer;
        std::string result;

#ifdef MAYAFLUX_PLATFORM_WINDOWS
        std::unique_ptr<FILE, decltype(&_pclose)> pipe(_popen(cmd, "r"), _pclose);
#else
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
#endif

        if (!pipe)
            return "";

        while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get())) {
            result += buffer.data();
        }
        return result;
    }

    static std::vector<std::string> parse_clang_includes(const std::string& output)
    {
        std::vector<std::string> includes;
        size_t start_pos = 0;
        bool in_search_paths = false;

        std::istringstream stream(output);
        std::string line;

        while (std::getline(stream, line)) {
            if (line.find("#include <...> search starts here:") != std::string::npos) {
                in_search_paths = true;
                continue;
            }
            if (line.find("End of search list.") != std::string::npos) {
                break;
            }
            if (in_search_paths) {
                // Trim whitespace
                auto start = line.find_first_not_of(" \t");
                if (start != std::string::npos) {
                    auto end = line.find_last_not_of(" \t");
                    std::string path = line.substr(start, end - start + 1);
                    if (!path.empty()) {
                        includes.push_back(path);
                    }
                }
            }
        }
        return includes;
    }

#ifdef MAYAFLUX_PLATFORM_WINDOWS
    static std::vector<std::string> get_msvc_includes()
    {
        std::vector<std::string> includes;
        namespace fs = std::filesystem;

        // Method 1: Use vswhere to find Visual Studio
        const char* vswhere_cmd = "\"C:\\Program Files (x86)\\Microsoft Visual Studio\\Installer\\vswhere.exe\" "
                                  "-latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 "
                                  "-property installationPath 2>nul";

        std::string vs_path = exec_command(vswhere_cmd);
        if (!vs_path.empty()) {
            // Clean path
            vs_path.erase(std::remove(vs_path.begin(), vs_path.end(), '\r'), vs_path.end());
            vs_path.erase(std::remove(vs_path.begin(), vs_path.end(), '\n'), vs_path.end());
            vs_path.erase(vs_path.find_last_not_of(" \t") + 1);

            if (fs::exists(vs_path)) {
                fs::path msvc_base = fs::path(vs_path) / "VC" / "Tools" / "MSVC";

                if (fs::exists(msvc_base)) {
                    std::string latest_version;
                    for (const auto& entry : fs::directory_iterator(msvc_base)) {
                        if (entry.is_directory()) {
                            std::string name = entry.path().filename().string();
                            if (!name.empty() && std::isdigit(name[0])) {
                                if (latest_version.empty() || name > latest_version) {
                                    latest_version = name;
                                }
                            }
                        }
                    }

                    if (!latest_version.empty()) {
                        fs::path include_path = msvc_base / latest_version / "include";
                        if (fs::exists(include_path)) {
                            includes.push_back(include_path.string());
                        }
                    }
                }
            }
        }

        // Method 2: Environment variable fallback
        if (includes.empty()) {
            const char* vc_dir = std::getenv("VCINSTALLDIR");
            if (vc_dir && fs::exists(vc_dir)) {
                fs::path include_path = fs::path(vc_dir) / "include";
                if (fs::exists(include_path)) {
                    includes.push_back(include_path.string());
                }
            }
        }

        return includes;
    }

    static std::vector<std::string> get_windows_sdk_includes()
    {
        std::vector<std::string> includes;
        namespace fs = std::filesystem;

        // Method 1: Environment variables
        const char* sdk_dir = std::getenv("WindowsSdkDir");
        const char* sdk_ver = std::getenv("WindowsSDKVersion");

        if (sdk_dir && sdk_ver && fs::exists(sdk_dir)) {
            fs::path base = fs::path(sdk_dir);
            std::string version = sdk_ver;

            // Remove trailing backslash from version if present
            if (!version.empty() && version.back() == '\\') {
                version.pop_back();
            }

            std::vector<std::string> subdirs = { "ucrt", "shared", "um", "winrt", "cppwinrt" };
            for (const auto& subdir : subdirs) {
                fs::path include_path = base / "Include" / version / subdir;
                if (fs::exists(include_path)) {
                    includes.push_back(include_path.string());
                }
            }
        }

        // Method 2: Common installation paths
        if (includes.empty()) {
            std::vector<fs::path> sdk_paths = {
                "C:\\Program Files (x86)\\Windows Kits\\10\\Include",
                "C:\\Program Files\\Windows Kits\\10\\Include"
            };

            for (const auto& base_path : sdk_paths) {
                if (fs::exists(base_path)) {
                    std::string latest_version;
                    for (const auto& entry : fs::directory_iterator(base_path)) {
                        if (entry.is_directory()) {
                            std::string name = entry.path().filename().string();
                            if (!name.empty() && std::isdigit(name[0])) {
                                if (latest_version.empty() || name > latest_version) {
                                    latest_version = name;
                                }
                            }
                        }
                    }

                    if (!latest_version.empty()) {
                        std::vector<std::string> subdirs = { "ucrt", "shared", "um", "winrt", "cppwinrt" };
                        for (const auto& subdir : subdirs) {
                            fs::path include_path = base_path / latest_version / subdir;
                            if (fs::exists(include_path)) {
                                includes.push_back(include_path.string());
                            }
                        }
                        break;
                    }
                }
            }
        }

        return includes;
    }
#endif
};

} // namespace MayaFlux::Platform
