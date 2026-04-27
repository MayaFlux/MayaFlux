#include "Depot.hpp"
#include "MayaFlux/API/Config.hpp"

#include "MayaFlux/API/Core.hpp"
#include "MayaFlux/Core/Engine.hpp"
#include "MayaFlux/IO/IOManager.hpp"

#include "MayaFlux/Buffers/Container/SoundContainerBuffer.hpp"
#include "MayaFlux/Kakshya/Source/SoundFileContainer.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace fs = std::filesystem;

namespace MayaFlux {

std::vector<std::shared_ptr<Buffers::SoundContainerBuffer>> prepare_audio_buffers(const std::shared_ptr<Kakshya::SoundFileContainer>& container)
{
    if (!container) {
        MF_ERROR(Journal::Component::API, Journal::Context::Runtime,
            "prepare_audio_buffers failed: null container");
        return {};
    }

    uint32_t num_channels = container->get_num_channels();
    std::vector<std::shared_ptr<Buffers::SoundContainerBuffer>> created_buffers;

    for (uint32_t channel = 0; channel < num_channels; ++channel) {
        auto container_buffer = std::make_shared<MayaFlux::Buffers::SoundContainerBuffer>(
            channel,
            Config::get_buffer_size(),
            container,
            channel);

        container_buffer->initialize();

        created_buffers.push_back(std::move(container_buffer));
    }

    return created_buffers;
}

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
        ".psd", ".gif", ".hdr", ".pic", ".pnm", ".exr"
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
    if (is_engine_configured()) {
        return get_context().get_io_manager();
    }

    MF_ERROR(Journal::Component::API, Journal::Context::Runtime,
        "Attempted to access IOManager before engine initialization");
    return nullptr;
}

}
