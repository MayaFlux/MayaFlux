#include "Depot.hpp"
#include "MayaFlux/API/Config.hpp"

#include "MayaFlux/API/Core.hpp"
#include "MayaFlux/Core/Engine.hpp"
#include "MayaFlux/IO/IOManager.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace fs = std::filesystem;

namespace MayaFlux {

bool is_image(const fs::path& filepath)
{
    if (!fs::exists(filepath) || !fs::is_regular_file(filepath)) {
        return false;
    }

    auto ext = filepath.extension().string();
    std::ranges::transform(ext, ext.begin(),
        [](unsigned char c) { return std::tolower(c); });

    static const std::unordered_set<std::string> image_extensions = {
        ".png", ".jpg", ".jpeg", ".bmp", ".tga",
        ".psd", ".gif", ".hdr", ".pic", ".pnm"
    };

    return image_extensions.contains(ext);
}

bool is_audio(const fs::path& filepath)
{
    if (!fs::exists(filepath) || !fs::is_regular_file(filepath)) {
        return false;
    }

    auto ext = filepath.extension().string();
    std::ranges::transform(ext, ext.begin(),
        [](unsigned char c) { return std::tolower(c); });

    static const std::unordered_set<std::string> audio_extensions = {
        ".wav", ".aiff", ".aif", ".flac", ".ogg",
        ".mp3", ".m4a", ".wma"
    };

    return audio_extensions.contains(ext);
}

std::shared_ptr<IO::IOManager> get_io_manager()
{
    if (is_engine_initialized()) {
        return get_context().get_io_manager();
    }

    MF_ERROR(Journal::Component::API, Journal::Context::Runtime,
        "Attempted to access IOManager before engine initialization");
    return nullptr;
}

}
