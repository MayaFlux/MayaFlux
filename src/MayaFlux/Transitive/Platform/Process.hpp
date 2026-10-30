#pragma once

namespace MayaFlux::Platform {

#ifdef MAYAFLUX_PLATFORM_WINDOWS
constexpr char PathSeparator = '\\';
#else
constexpr char PathSeparator = '/';
#endif

inline std::string safe_getenv(const char* var)
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

// NOLINTNEXTLINE(cert-env33-c)
inline std::string exec_command(const char* cmd)
{
    std::array<char, 128> buffer {};
    std::string result;

    using PipeCloser = int (*)(FILE*);
#ifdef MAYAFLUX_PLATFORM_WINDOWS
    std::unique_ptr<FILE, PipeCloser> pipe(_popen(cmd, "r"), _pclose);
#else
    std::unique_ptr<FILE, PipeCloser> pipe(popen(cmd, "r"), pclose);
#endif

    if (!pipe)
        return "";

    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get())) {
        result += buffer.data();
    }
    return result;
}

inline void trim_output(std::string& str)
{
    str.erase(std::remove(str.begin(), str.end(), '\r'), str.end());
    str.erase(std::remove(str.begin(), str.end(), '\n'), str.end());
    str.erase(str.find_last_not_of(" \t") + 1);
}

} // namespace MayaFlux::Platform
