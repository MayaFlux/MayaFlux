#pragma once
#include "AudioBackend.hpp"

namespace MayaFlux::Core {

// TODO:: Need to see if this is truly necessary or if DeviceInfo can be more accommodating
static DeviceInfo convert_device_info(
    const RtAudio::DeviceInfo& rtInfo,
    unsigned int id,
    unsigned int defaultOutputDevice,
    unsigned int defaultInputDevice)
{
    DeviceInfo info;
    info.name = rtInfo.name;
    info.input_channels = rtInfo.inputChannels;
    info.output_channels = rtInfo.outputChannels;
    info.duplex_channels = rtInfo.duplexChannels;
    info.preferred_sample_rate = rtInfo.preferredSampleRate;
    info.supported_samplerates = rtInfo.sampleRates;
    info.is_default_output = (id == defaultOutputDevice);
    info.is_default_input = (id == defaultInputDevice);
    return info;
}

class RtAudioBackend : public IAudioBackend {
public:
    RtAudioBackend();
    ~RtAudioBackend() override;

    std::unique_ptr<AudioDevice> create_device_manager();
    std::unique_ptr<AudioStream> create_stream(unsigned int deviceId, const GlobalStreamInfo& stream_info, void* user_data) override;

    std::string get_version_string() const override;
    int get_api_type() const override;

    RtAudio* getRawHandle() { return m_context.get(); }

private:
    std::unique_ptr<RtAudio> m_context;
};

class RtAudioDevice : public AudioDevice {
public:
    RtAudioDevice(RtAudio* context);

    std::vector<DeviceInfo> get_output_devices() const override;
    std::vector<DeviceInfo> get_input_devices() const override;
    unsigned int get_default_output_device() const override;
    unsigned int get_default_input_device() const override;

private:
    RtAudio* m_context;
    std::vector<DeviceInfo> m_output_devices;
    std::vector<DeviceInfo> m_input_devices;
    unsigned int m_defaultOutputDevice;
    unsigned int m_defaultInputDevice;
};

class RtAudioStream : public AudioStream {
public:
    RtAudioStream(
        RtAudio* context,
        unsigned int deviceId,
        const GlobalStreamInfo& streamInfo,
        void* userData);

    ~RtAudioStream() override;

    void open() override;
    void start() override;
    void stop() override;
    void close() override;

    bool is_running() const override;
    bool is_open() const override;

    void set_process_callback(
        std::function<int(void*, void*, unsigned int)> processCallback) override;

private:
    static int rtAudioCallback(
        void* output_buffer,
        void* input_buffer,
        unsigned int num_frames,
        double stream_time,
        RtAudioStreamStatus status,
        void* user_data);

    RtAudio* m_context;
    RtAudio::StreamParameters m_parameters;
    RtAudio::StreamOptions m_options;
    GlobalStreamInfo stream_info;
    void* m_userData;

    bool m_isOpen;
    bool m_isRunning;

    std::function<int(void*, void*, unsigned int)> m_process_callback;

    GlobalStreamInfo m_stream_info;

    void configure_stream_options();
};

} // namespace MayaFlux::Core::Audio
