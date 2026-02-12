#pragma once

namespace MayaFlux::Platform {

#ifdef MAYAFLUX_PLATFORM_WINDOWS
constexpr char PathSeparator = '\\';
#else
constexpr char PathSeparator = '/';
#endif

static std::string safe_getenv(const char* var);

namespace fs = std::filesystem;
class MAYAFLUX_API SystemConfig {
public:
    static const std::string& get_clang_resource_dir();

    static const std::vector<std::string>& get_system_includes();

    static const std::vector<std::string>& get_system_libraries();

    static const std::string& find_library(const std::string& library_name);

#ifdef MAYAFLUX_PLATFORM_MACOS
    static std::string get_macos_sdk_path();
#endif // MAYAFLUX_PLATFORM_MACOS

private:
    static std::string exec_command(const char* cmd);

    static void trim_output(std::string& str);

    static std::string format_library_name(const std::string& library_name);

    static std::vector<std::string> get_clang_includes();

    static std::vector<std::string> parse_clang_search_paths(const std::string& output);

    static std::string find_latest_sdk_version(const fs::path& base);

#ifdef MAYAFLUX_PLATFORM_WINDOWS

    static std::string find_latest_vs_installation();

    static std::string find_latest_msvc_version(const fs::path& msvc_base);

    static std::vector<std::string> get_msvc_includes();

    static std::vector<std::string> get_msvc_libraries();

    static std::vector<std::string> get_windows_sdk_includes();

    static std::vector<std::string> get_windows_sdk_libraries();

    static std::vector<std::string> probe_sdk_paths(const std::string& subpath,
        const std::vector<std::string>& subdirs,
        const std::string& arch = "");

#else
    static std::vector<std::string> get_unix_library_paths();
#ifdef MAYAFLUX_PLATFORM_MACOS
    static std::string get_xcode_system_includes();
#endif // MAYAFLUX_PLATFORM_MACOS

#endif // MAYAFLUX_PLATFORM_WINDOWS
};

} // namespace Platform
