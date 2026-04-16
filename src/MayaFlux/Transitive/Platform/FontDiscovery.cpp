#include "FontDiscovery.hpp"

#include "HostEnvironment.hpp"

#if defined(MAYAFLUX_PLATFORM_LINUX)
#include <fontconfig/fontconfig.h>
#endif

namespace MayaFlux::Platform {

namespace {
    void log_warn(std::string_view msg)
    {
        std::println(std::cerr, "[Platform::FontDiscovery] WARN  {:.{}}",
            msg.data(), static_cast<int>(msg.size()));
    }

    void log_debug(std::string_view msg)
    {
#if !defined(NDEBUG)
        std::println(std::cerr, "[Platform::FontDiscovery] DEBUG {:.{}}",
            msg.data(), static_cast<int>(msg.size()));
#else
        (void)msg;
#endif
    }
}

#if defined(MAYAFLUX_PLATFORM_LINUX)

std::optional<std::string> find_font(std::string_view family, std::string_view style)
{
    FcConfig* cfg = FcInitLoadConfigAndFonts();
    if (!cfg) {
        log_warn("FcInitLoadConfigAndFonts failed");
        return std::nullopt;
    }

    FcPattern* pat = FcNameParse(
        reinterpret_cast<const FcChar8*>(std::string(family).c_str()));
    if (!style.empty()) {
        FcPatternAddString(pat, FC_STYLE,
            reinterpret_cast<const FcChar8*>(std::string(style).c_str()));
    }
    FcConfigSubstitute(cfg, pat, FcMatchPattern);
    FcDefaultSubstitute(pat);

    FcResult result {};
    FcPattern* match = FcFontMatch(cfg, pat, &result);

    std::optional<std::string> out;
    if (match) {
        FcChar8* file_path {};
        if (FcPatternGetString(match, FC_FILE, 0, &file_path) == FcResultMatch) {
            out = reinterpret_cast<const char*>(file_path);
            log_debug(std::string(family) + " -> " + *out);
        }
        FcPatternDestroy(match);
    } else {
        log_warn("no fontconfig match for '" + std::string(family) + "'");
    }

    FcPatternDestroy(pat);
    FcConfigDestroy(cfg);
    return out;
}

#elif defined(MAYAFLUX_PLATFORM_MACOS)

namespace fs = std::filesystem;

std::optional<std::string> find_font(std::string_view family, std::string_view style)
{
    std::string query = std::string(family);
    if (!style.empty()) {
        query += ":style=" + std::string(style);
    }

    const std::string cmd = "fc-match --format='%{file}' '" + query + "' 2>/dev/null";
    std::string result = SystemConfig::exec_command(cmd.c_str());
    SystemConfig::trim_output(result);

    if (!result.empty() && fs::exists(result)) {
        log_debug(query + " -> " + result);
        return result;
    }

    const std::vector<fs::path> search_dirs = {
        fs::path(safe_getenv("HOME")) / "Library" / "Fonts",
        "/Library/Fonts",
        "/System/Library/Fonts",
        "/System/Library/Fonts/Supplemental"
    };

    std::string lower_family(family);
    std::ranges::transform(lower_family,
        lower_family.begin(), ::tolower);

    for (const auto& dir : search_dirs) {
        if (!fs::exists(dir)) {
            continue;
        }
        for (const auto& entry : fs::directory_iterator(dir)) {
            const std::string ext = entry.path().extension().string();
            if (ext != ".ttf" && ext != ".otf") {
                continue;
            }
            std::string lower_name = entry.path().filename().string();
            std::ranges::transform(lower_name,
                lower_name.begin(), ::tolower);
            if (lower_name.find(lower_family) != std::string::npos) {
                log_debug(std::string(family) + " -> " + entry.path().string());
                return entry.path().string();
            }
        }
    }

    log_warn("no match for '" + std::string(family) + "'");
    return std::nullopt;
}

#elif defined(MAYAFLUX_PLATFORM_WINDOWS)

#include <windows.h>
#ifdef ERROR
#undef ERROR
#endif // ERROR

namespace fs = std::filesystem;

std::optional<std::string> find_font(std::string_view family, std::string_view /*style*/)
{
    char windir[MAX_PATH] {};
    GetWindowsDirectoryA(windir, MAX_PATH);
    const fs::path fonts_dir = fs::path(windir) / "Fonts";

    if (!fs::exists(fonts_dir)) {
        log_warn("fonts directory not found at " + fonts_dir.string());
        return std::nullopt;
    }

    std::string lower_family(family);
    std::ranges::transform(lower_family, lower_family.begin(),
        [](unsigned char c) { return std::tolower(c); });
    std::erase_if(lower_family,
        [](unsigned char c) { return std::isspace(c); });

    for (const auto& entry : fs::directory_iterator(fonts_dir)) {
        const std::string ext = entry.path().extension().string();
        if (ext != ".ttf" && ext != ".otf") {
            continue;
        }
        std::string lower_name = entry.path().filename().string();
        std::ranges::transform(lower_name, lower_name.begin(),
            [](unsigned char c) { return std::tolower(c); });
        if (lower_name.find(lower_family) != std::string::npos) {
            log_debug(std::string(family) + " -> " + entry.path().string());
            return entry.path().string();
        }
    }

    log_warn("no match for '" + std::string(family) + "'");
    return std::nullopt;
}

#endif

} // namespace MayaFlux::Platform
