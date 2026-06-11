#include "Depot.hpp"
#include "MayaFlux/API/Config.hpp"

#include "MayaFlux/API/Core.hpp"
#include "MayaFlux/Core/Engine.hpp"
#include "MayaFlux/IO/IOManager.hpp"

#include "MayaFlux/Buffers/Container/SoundContainerBuffer.hpp"
#include "MayaFlux/Kakshya/Source/SoundFileContainer.hpp"
#include "MayaFlux/Kakshya/Source/SoundStreamContainer.hpp"

#include "MayaFlux/Buffers/Geometry/MeshBuffer.hpp"
#include "MayaFlux/Nodes/Network/MeshNetwork.hpp"

#include "MayaFlux/Portal/System/Dialog/Chooser.hpp"
#include "MayaFlux/Portal/System/System.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace fs = std::filesystem;

namespace MayaFlux {

namespace {

    bool require_portal(const char* caller)
    {
        if (!Portal::System::is_initialized()) {
            MF_ERROR(Journal::Component::API, Journal::Context::Runtime,
                "{}: Portal::System not initialized", caller);
            return false;
        }
        return true;
    }

    const std::vector<Portal::System::Dialog::ChooserFilter> k_audio_open_filters {
        { .name = "Audio", .extensions = { "wav", "aiff", "aif", "flac", "ogg", "mp3", "m4a", "wma" } },
        { .name = "All Files", .extensions = { "*" } }
    };

    const std::vector<Portal::System::Dialog::ChooserFilter> k_audio_save_filters {
        { .name = "Audio", .extensions = { "wav", "flac", "ogg", "mp3" } },
        { .name = "All Files", .extensions = { "*" } }
    };

    const std::vector<Portal::System::Dialog::ChooserFilter> k_video_filters {
        { .name = "Video", .extensions = { "mp4", "mkv", "mov", "webm", "avi" } },
        { .name = "All Files", .extensions = { "*" } }
    };

    const std::vector<Portal::System::Dialog::ChooserFilter> k_image_filters {
        { .name = "Image", .extensions = { "png", "jpg", "jpeg", "bmp", "tga", "psd", "gif", "hdr", "pic", "pnm", "exr" } },
        { .name = "All Files", .extensions = { "*" } }
    };

    const std::vector<Portal::System::Dialog::ChooserFilter> k_mesh_filters {
        { .name = "3D Model", .extensions = { "glb", "gltf", "fbx", "obj", "ply", "stl", "dae" } },
        { .name = "All Files", .extensions = { "*" } }
    };

    const std::vector<Portal::System::Dialog::ChooserFilter> k_image_save_filters {
        { .name = "Image", .extensions = { "png", "jpg", "jpeg", "bmp", "tga", "exr", "hdr" } },
        { .name = "All Files", .extensions = { "*" } }
    };

    bool check_extension(const fs::path& filepath, const Portal::System::Dialog::ChooserFilter& filter)
    {
        if (!fs::exists(filepath) || !fs::is_regular_file(filepath))
            return false;

        auto ext = filepath.extension().string();
        if (ext.empty())
            return false;

        std::ranges::transform(ext, ext.begin(), [](unsigned char c) { return std::tolower(c); });
        const std::string_view bare { ext.data() + 1, ext.size() - 1 }; // strip leading dot

        return std::ranges::any_of(filter.extensions,
            [&](const std::string& e) { return e == bare; });
    }

} // namespace

// ---------------------------------------------------------------------------

std::vector<std::shared_ptr<Buffers::SoundContainerBuffer>> prepare_audio_buffers(
    const std::shared_ptr<Kakshya::SoundFileContainer>& container)
{
    if (!container) {
        MF_ERROR(Journal::Component::API, Journal::Context::Runtime,
            "prepare_audio_buffers failed: null container");
        return {};
    }

    uint32_t num_channels = container->get_num_channels();
    std::vector<std::shared_ptr<Buffers::SoundContainerBuffer>> created_buffers;

    for (uint32_t ch = 0; ch < num_channels; ++ch) {
        auto buf = std::make_shared<Buffers::SoundContainerBuffer>(
            ch, Config::get_buffer_size(), container, ch);
        buf->initialize();
        created_buffers.push_back(std::move(buf));
    }

    return created_buffers;
}

bool is_image(const fs::path& filepath)
{
    return check_extension(filepath, k_image_filters[0]);
}

bool is_audio(const fs::path& filepath)
{
    return check_extension(filepath, k_audio_open_filters[0]);
}

// ---------------------------------------------------------------------------
// Dialog-backed load
// ---------------------------------------------------------------------------

std::shared_ptr<Kakshya::SoundFileContainer> choose_audio()
{
    if (!require_portal("choose_audio"))
        return nullptr;

    return Portal::System::Dialog::open_file<std::shared_ptr<Kakshya::SoundFileContainer>>(
        [](const fs::path& p) { return get_io_manager()->load_audio(p.string()); },
        [](Core::SystemDialogError) { },
        k_audio_open_filters);
}

std::shared_ptr<Kakshya::VideoFileContainer> choose_video()
{
    if (!require_portal("choose_video"))
        return nullptr;

    return Portal::System::Dialog::open_file<std::shared_ptr<Kakshya::VideoFileContainer>>(
        [](const fs::path& p) { return get_io_manager()->load_video(p.string()); },
        [](Core::SystemDialogError) { },
        k_video_filters);
}

std::shared_ptr<Buffers::TextureBuffer> choose_image()
{
    if (!require_portal("choose_image"))
        return nullptr;

    return Portal::System::Dialog::open_file<std::shared_ptr<Buffers::TextureBuffer>>(
        [](const fs::path& p) { return get_io_manager()->load_image(p.string()); },
        [](Core::SystemDialogError) { },
        k_image_filters);
}

std::vector<std::shared_ptr<Buffers::MeshBuffer>> choose_mesh()
{
    if (!require_portal("choose_mesh"))
        return {};

    return Portal::System::Dialog::open_file<std::vector<std::shared_ptr<Buffers::MeshBuffer>>>(
        [](const fs::path& p) { return get_io_manager()->load_mesh(p.string()); },
        [](Core::SystemDialogError) { },
        k_mesh_filters);
}

std::shared_ptr<Nodes::Network::MeshNetwork> choose_mesh_network(IO::TextureResolver resolver)
{
    if (!require_portal("choose_mesh_network"))
        return nullptr;

    return Portal::System::Dialog::open_file<std::shared_ptr<Nodes::Network::MeshNetwork>>(
        [r = std::move(resolver)](const fs::path& p) mutable {
            return get_io_manager()->load_mesh_network(p.string(), std::move(r));
        },
        [](Core::SystemDialogError) { },
        k_mesh_filters);
}

// ---------------------------------------------------------------------------
// Dialog-backed save
// ---------------------------------------------------------------------------

bool save_audio(
    const std::shared_ptr<Kakshya::SoundStreamContainer>& container,
    const std::string& suggested_name)
{
    if (!require_portal("save_audio"))
        return false;

    auto iom = get_io_manager();
    if (!iom) {
        MF_ERROR(Journal::Component::API, Journal::Context::Runtime,
            "save_audio: IOManager unavailable");
        return false;
    }

    return Portal::System::Dialog::save_file<bool>(
        [&iom, &container](const fs::path& p) {
            iom->write(container, p.string());
            return true;
        },
        [](Core::SystemDialogError) { },
        suggested_name,
        k_audio_save_filters);
}

bool save_image(
    const std::shared_ptr<Buffers::TextureBuffer>& buffer,
    const std::string& suggested_name)
{
    if (!require_portal("save_image"))
        return false;

    auto iom = get_io_manager();
    if (!iom) {
        MF_ERROR(Journal::Component::API, Journal::Context::Runtime,
            "save_image: IOManager unavailable");
        return false;
    }

    return Portal::System::Dialog::save_file<bool>(
        [&iom, &buffer](const fs::path& p) {
            return iom->save_image(buffer, p.string(), IO::ImageWriteOptions {});
        },
        [](Core::SystemDialogError) { },
        suggested_name,
        k_image_save_filters);
}

bool save_image(
    const std::shared_ptr<Buffers::TextureBuffer>& buffer,
    const std::string& suggested_name,
    const IO::ImageWriteOptions& options)
{
    if (!require_portal("save_image"))
        return false;

    auto iom = get_io_manager();
    if (!iom) {
        MF_ERROR(Journal::Component::API, Journal::Context::Runtime,
            "save_image: IOManager unavailable");
        return false;
    }

    return Portal::System::Dialog::save_file<bool>(
        [&iom, &buffer, &options](const fs::path& p) {
            return iom->save_image(buffer, p.string(), options);
        },
        [](Core::SystemDialogError) { },
        suggested_name,
        k_image_save_filters);
}

// ---------------------------------------------------------------------------

std::shared_ptr<IO::IOManager> get_io_manager()
{
    if (is_engine_configured())
        return get_context().get_io_manager();

    MF_ERROR(Journal::Component::API, Journal::Context::Runtime,
        "Attempted to access IOManager before engine initialization");
    return nullptr;
}

} // namespace MayaFlux
