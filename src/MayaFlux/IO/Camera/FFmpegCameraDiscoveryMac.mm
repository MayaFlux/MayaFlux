#ifdef MAYAFLUX_PLATFORM_MACOS

#include "CameraSource.hpp"

#include <charconv>

#import <AVFoundation/AVFoundation.h>

namespace MayaFlux::IO {

std::vector<CameraConfig> enumerate_macos_cameras()
{
    std::vector<CameraConfig> configs;

    @autoreleasepool {
        for (AVCaptureDevice* device in
                [AVCaptureDevice devicesWithMediaType:AVMediaTypeVideo]) {
            CameraConfig config;
            config.device_name = device.localizedName.UTF8String;
            config.display_name = config.device_name;
            configs.push_back(std::move(config));
        }
    }

    return configs;
}

double macos_camera_max_fps(const std::string& device_name)
{
    double max_fps = 0.0;

    @autoreleasepool {
        NSArray<AVCaptureDevice*>* devices = [AVCaptureDevice devicesWithMediaType:AVMediaTypeVideo];
        NSString* name = [NSString stringWithUTF8String:device_name.c_str()];

        AVCaptureDevice* match = nil;
        for (AVCaptureDevice* device in devices) {
            if ([device.localizedName hasPrefix:name]) {
                match = device;
                break;
            }
        }

        size_t index = 0;
        const auto* end = device_name.data() + device_name.size();
        if (!match && !device_name.empty()
            && std::from_chars(device_name.data(), end, index).ptr == end
            && index < devices.count) {
            match = devices[index];
        }

        for (AVCaptureDeviceFormat* format in match.formats) {
            for (AVFrameRateRange* range in format.videoSupportedFrameRateRanges)
                max_fps = std::max(max_fps, range.maxFrameRate);
        }
    }

    return max_fps;
}

}

#endif
