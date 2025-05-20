#pragma once

#include "GlobalStreamInfo.hpp"
#include "MayaFlux/Utils.hpp"

namespace MayaFlux::Core {
class AudioDevice;
class AudioStream;

struct DeviceInfo {
    std::string name;
    u_int32_t input_channels = 0;
    u_int32_t output_channels = 0;
    u_int32_t duplex_channels = 0;
    u_int32_t preferred_sample_rate = 0;
    std::vector<u_int32_t> supported_samplerates;
    bool is_default_output = false;
    bool is_default_input = false;
};

class IAudioBackend {
public:
    virtual ~IAudioBackend() = default;

    virtual std::unique_ptr<AudioDevice> create_device_manager() = 0;

    virtual std::unique_ptr<AudioStream> create_stream(unsigned int deviceId, const GlobalStreamInfo& stream_info, void* userData) = 0;

    virtual std::string get_version_string() const = 0;
    virtual int get_api_type() const = 0;
};

class AudioDevice {
public:
    virtual ~AudioDevice() = default;

    virtual std::vector<DeviceInfo> get_output_devices() const = 0;
    virtual std::vector<DeviceInfo> get_input_devices() const = 0;
    virtual unsigned int get_default_output_device() const = 0;
    virtual unsigned int get_default_input_device() const = 0;
};

class AudioStream {
public:
    virtual ~AudioStream() = default;

    virtual void open() = 0;
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void close() = 0;

    virtual bool is_running() const = 0;
    virtual bool is_open() const = 0;

    virtual void set_process_callback(std::function<int(void*, void*, unsigned int)> processCallback) = 0;
};

class AudioBackendFactory {
public:
    static std::unique_ptr<IAudioBackend> create_backend(Utils::BackendType type);
};
}
