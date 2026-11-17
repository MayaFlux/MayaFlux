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
    size_t len = 0;
    if (getenv_s(&len, nullptr, 0, var) != 0 || len <= 1) {
        return "";
    }
    std::string result(len, '\0');
    if (getenv_s(&len, result.data(), result.size(), var) != 0 || len == 0) {
        return "";
    }
    result.resize(len - 1);
    return result;
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
