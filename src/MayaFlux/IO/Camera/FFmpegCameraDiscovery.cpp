#include "FFmpegCameraReader.hpp"

extern "C" {
#include <libavdevice/avdevice.h>
#include <libavformat/avformat.h>
}

#ifdef MAYAFLUX_PLATFORM_LINUX
#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

namespace MayaFlux::IO {

#ifdef MAYAFLUX_PLATFORM_MACOS
std::vector<CameraConfig> enumerate_macos_cameras();
#endif

#ifndef MAYAFLUX_PLATFORM_MACOS
namespace {

    bool provides_video(const AVDeviceInfo& info)
    {
        if (info.nb_media_types == 0)
            return true;
        return std::ranges::find(
                   std::span(info.media_types, static_cast<size_t>(info.nb_media_types)),
                   AVMEDIA_TYPE_VIDEO)
            != std::span(info.media_types, static_cast<size_t>(info.nb_media_types)).end();
    }

#ifdef MAYAFLUX_PLATFORM_LINUX
    bool can_capture(const std::string& path)
    {
        const int fd = ::open(path.c_str(), O_RDONLY | O_NONBLOCK);
        if (fd < 0)
            return false;

        v4l2_capability capability {};
        const bool queried = ::ioctl(fd, VIDIOC_QUERYCAP, &capability) == 0;
        ::close(fd);
        if (!queried)
            return false;

        const uint32_t caps = (capability.capabilities & V4L2_CAP_DEVICE_CAPS)
            ? capability.device_caps
            : capability.capabilities;
        return (caps & V4L2_CAP_VIDEO_CAPTURE) != 0;
    }
#endif

}
#endif

std::vector<CameraConfig> FFmpegCameraReader::enumerate_configs() const
{
#ifdef MAYAFLUX_PLATFORM_MACOS
    return enumerate_macos_cameras();
#else
    std::vector<CameraConfig> configs;

    avdevice_register_all();
    const AVInputFormat* format = av_find_input_format(std::string(CAMERA_FORMAT).c_str());
    AVDeviceInfoList* list = nullptr;
    if (!format || avdevice_list_input_sources(format, nullptr, nullptr, &list) < 0)
        return configs;

    for (int i = 0; i < list->nb_devices; ++i) {
        const AVDeviceInfo& info = *list->devices[i];
        if (!provides_video(info))
            continue;

        CameraConfig config;
        config.device_name = info.device_name;
        config.display_name = info.device_description ? info.device_description : "";
#ifdef MAYAFLUX_PLATFORM_WINDOWS
        config.device_name.insert(0, "video=");
#endif
#ifdef MAYAFLUX_PLATFORM_LINUX
        if (!can_capture(config.device_name))
            continue;
#endif
        configs.push_back(std::move(config));
    }
    avdevice_free_list_devices(&list);

    std::ranges::sort(configs, {}, &CameraConfig::device_name);
    return configs;
#endif
}

}
