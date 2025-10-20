#include "config.h"

namespace MayaFlux::Platform {

std::string safe_getenv(const char* var)
{
#ifdef MAYAFLUX_PLATFORM_WINDOWS
    char* value = nullptr;
    size_t len = 0;
    if (_dupenv_s(&value, &len, var) == 0 && value != nullptr) {
        std::string result(value);
        free(value);
        return result;
    }
    return "";
#else
    const char* value = std::getenv(var);
    return value ? std::string(value) : "";
#endif
}

std::string SystemConfig::get_clang_resource_dir()
{
    const char* cmd = "clang -print-resource-dir";
    auto result = exec_command(cmd);
    trim_output(result);

    if (!result.empty()
        && (result.find("error:") == std::string::npos)
        && (result.find("clang") != std::string::npos
            || result.find("lib") != std::string::npos
            || fs::exists(result))) {
        return result;
    }

    return "";
}

std::vector<std::string> SystemConfig::get_system_includes()
{
    std::vector<std::string> includes;

#ifdef MAYAFLUX_PLATFORM_WINDOWS
    auto msvc_includes = get_msvc_includes();
    includes.insert(includes.end(), msvc_includes.begin(), msvc_includes.end());

    auto sdk_includes = get_windows_sdk_includes();
    includes.insert(includes.end(), sdk_includes.begin(), sdk_includes.end());
#endif

    auto clang_includes = get_clang_includes();
    includes.insert(includes.end(), clang_includes.begin(), clang_includes.end());

    return includes;
}

std::vector<std::string> SystemConfig::get_system_libraries()
{
    std::vector<std::string> lib_paths;

#ifdef MAYAFLUX_PLATFORM_WINDOWS
    auto msvc_libs = get_msvc_libraries();
    lib_paths.insert(lib_paths.end(), msvc_libs.begin(), msvc_libs.end());

    auto sdk_libs = get_windows_sdk_libraries();
    lib_paths.insert(lib_paths.end(), sdk_libs.begin(), sdk_libs.end());
#else
    auto system_libs = get_unix_library_paths();
    lib_paths.insert(lib_paths.end(), system_libs.begin(), system_libs.end());
#endif

    return lib_paths;
}
std::string SystemConfig::find_library(const std::string& library_name)
{
    auto lib_paths = get_system_libraries();
    std::string search_name = format_library_name(library_name);

    for (const auto& lib_path : lib_paths) {
        fs::path full_path = fs::path(lib_path) / search_name;
        if (fs::exists(full_path)) {
            return full_path.string();
        }
    }

    return "";
}
std::string SystemConfig::exec_command(const char* cmd)
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
void SystemConfig::trim_output(std::string& str)
{
    str.erase(std::remove(str.begin(), str.end(), '\r'), str.end());
    str.erase(std::remove(str.begin(), str.end(), '\n'), str.end());
    str.erase(str.find_last_not_of(" \t") + 1);
}
std::string SystemConfig::format_library_name(const std::string& library_name)
{
#ifdef MAYAFLUX_PLATFORM_WINDOWS
    if (library_name.find(".lib") == std::string::npos) {
        return library_name + ".lib";
    }
#else
    if (library_name.find(".a") == std::string::npos && library_name.find(".so") == std::string::npos) {
        return "lib" + library_name + ".a";
    }
#endif
    return library_name;
}
std::vector<std::string> SystemConfig::get_clang_includes()
{
#ifdef MAYAFLUX_PLATFORM_WINDOWS
    const char* cmd = "echo. | clang -v -E -x c++ - 2>&1";
#else
    const char* cmd = "clang -v -E -x c++ - 2>&1 < /dev/null";
#endif

    auto clang_output = exec_command(cmd);
    return parse_clang_search_paths(clang_output);
}
std::vector<std::string> SystemConfig::parse_clang_search_paths(const std::string& output)
{
    std::vector<std::string> paths;
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
            auto start = line.find_first_not_of(" \t");
            if (start != std::string::npos) {
                auto end = line.find_last_not_of(" \t");
                std::string path = line.substr(start, end - start + 1);
                if (!path.empty()) {
                    paths.push_back(path);
                }
            }
        }
    }
    return paths;
}

std::string SystemConfig::find_latest_sdk_version(const fs::path& base)
{
    if (!fs::exists(base)) {
        return "";
    }

    std::string latest_version;
    for (const auto& entry : fs::directory_iterator(base)) {
        if (entry.is_directory()) {
            std::string name = entry.path().filename().string();
            if (!name.empty() && std::isdigit(name[0])) {
                if (latest_version.empty() || name > latest_version) {
                    latest_version = name;
                }
            }
        }
    }
    return latest_version;
}

#ifdef MAYAFLUX_PLATFORM_WINDOWS

std::string SystemConfig::find_latest_vs_installation()
{
    std::vector<std::string> vswhere_paths = {
        "C:\\Program Files (x86)\\Microsoft Visual Studio\\Installer\\vswhere.exe",
        "C:\\Program Files\\Microsoft Visual Studio\\Installer\\vswhere.exe"
    };

    for (const auto& path : vswhere_paths) {
        if (fs::exists(path)) {
            std::string cmd = "\"" + path + "\" -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath";
            std::string vs_path = exec_command(cmd.c_str());
            if (!vs_path.empty()) {
                trim_output(vs_path);
                if (fs::exists(vs_path)) {
                    return vs_path;
                }
            }
        }
    }

    return "";
}

std::string SystemConfig::find_latest_msvc_version(const fs::path& msvc_base)
{
    if (!fs::exists(msvc_base)) {
        return "";
    }

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
    return latest_version;
}

std::vector<std::string> SystemConfig::get_msvc_includes()
{
    std::vector<std::string> includes;

    std::string vs_path = find_latest_vs_installation();
    if (!vs_path.empty()) {
        fs::path msvc_base = fs::path(vs_path) / "VC" / "Tools" / "MSVC";
        std::string version = find_latest_msvc_version(msvc_base);

        if (!version.empty()) {
            fs::path include_path = msvc_base / version / "include";
            if (fs::exists(include_path)) {
                includes.push_back(include_path.string());
            }
        }
    }

    if (includes.empty()) {
        std::string vc_dir = safe_getenv("VCINSTALLDIR");
        if (!vc_dir.empty() && fs::exists(vc_dir)) {
            fs::path include_path = fs::path(vc_dir) / "include";
            if (fs::exists(include_path)) {
                includes.push_back(include_path.string());
            }
        }
    }

    return includes;
}

std::vector<std::string> SystemConfig::get_msvc_libraries()
{
    std::vector<std::string> lib_paths;

    std::string vs_path = find_latest_vs_installation();
    if (!vs_path.empty()) {
        fs::path msvc_base = fs::path(vs_path) / "VC" / "Tools" / "MSVC";
        std::string version = find_latest_msvc_version(msvc_base);

        if (!version.empty()) {
            fs::path lib_path = msvc_base / version / "lib" / "x64";
            if (fs::exists(lib_path)) {
                lib_paths.push_back(lib_path.string());
            }
        }
    }

    if (lib_paths.empty()) {
        std::string vc_dir = safe_getenv("VCINSTALLDIR");
        if (!vc_dir.empty() && fs::exists(vc_dir)) {
            fs::path lib_path = fs::path(vc_dir) / "lib" / "x64";
            if (fs::exists(lib_path)) {
                lib_paths.push_back(lib_path.string());
            }
        }
    }

    return lib_paths;
}

std::vector<std::string> SystemConfig::get_windows_sdk_includes()
{
    std::vector<std::string> includes;

    std::string sdk_dir = safe_getenv("WindowsSdkDir");
    std::string sdk_ver = safe_getenv("WindowsSDKVersion");

    if (!sdk_dir.empty() && !sdk_ver.empty() && fs::exists(sdk_dir)) {
        fs::path base = fs::path(sdk_dir);
        std::string version = std::string(sdk_ver);
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

    if (includes.empty()) {
        includes = probe_sdk_paths("Include",
            std::vector<std::string> { "ucrt", "shared", "um", "winrt", "cppwinrt" });
    }

    return includes;
}

std::vector<std::string> SystemConfig::get_windows_sdk_libraries()
{
    std::vector<std::string> lib_paths;

    std::string sdk_dir = safe_getenv("WindowsSdkDir");
    std::string sdk_ver = safe_getenv("WindowsSDKVersion");

    if (!sdk_dir.empty() && !sdk_ver.empty() && fs::exists(sdk_dir)) {
        fs::path base = fs::path(sdk_dir);
        std::string version = std::string(sdk_ver);
        if (!version.empty() && version.back() == '\\') {
            version.pop_back();
        }

        std::vector<std::string> subdirs = { "ucrt", "um" };
        for (const auto& subdir : subdirs) {
            fs::path lib_path = base / "Lib" / version / subdir / "x64";
            if (fs::exists(lib_path)) {
                lib_paths.push_back(lib_path.string());
            }
        }
    }

    if (lib_paths.empty()) {
        lib_paths = probe_sdk_paths("Lib",
            std::vector<std::string> { "ucrt", "um" }, "x64");
    }

    return lib_paths;
}

std::vector<std::string> SystemConfig::probe_sdk_paths(const std::string& subpath,
    const std::vector<std::string>& subdirs,
    const std::string& arch)
{
    std::vector<std::string> paths;
    std::vector<fs::path> sdk_base_paths = {
        "C:\\Program Files (x86)\\Windows Kits\\10",
        "C:\\Program Files\\Windows Kits\\10"
    };

    for (const auto& base_path : sdk_base_paths) {
        if (fs::exists(base_path)) {
            fs::path search_dir = fs::path(base_path) / subpath;
            std::string version = find_latest_sdk_version(search_dir);

            if (!version.empty()) {
                for (const auto& subdir : subdirs) {
                    fs::path final_path = fs::path(base_path) / subpath / version / subdir;
                    if (!arch.empty()) {
                        final_path = final_path / arch;
                    }
                    if (fs::exists(final_path)) {
                        paths.push_back(final_path.string());
                    }
                }
                break;
            }
        }
    }

    return paths;
}

#else

std::vector<std::string> SystemConfig::get_unix_library_paths()
{
    std::vector<std::string> lib_paths;

    std::string ld_library_path = safe_getenv("LD_LIBRARY_PATH");
    if (!ld_library_path.empty()) {
        std::istringstream stream(ld_library_path);
        std::string path;
        while (std::getline(stream, path, ':')) {
            if (!path.empty() && fs::exists(path)) {
                lib_paths.push_back(path);
            }
        }
    }

    std::vector<std::string> default_paths = {
        "/usr/local/lib",
        "/usr/lib",
        "/lib",
        "/usr/local/lib64",
        "/usr/lib64",
        "/lib64"
    };

    for (const auto& path : default_paths) {
        if (fs::exists(path)) {
            lib_paths.push_back(path);
        }
    }

    return lib_paths;
}

#endif
}
