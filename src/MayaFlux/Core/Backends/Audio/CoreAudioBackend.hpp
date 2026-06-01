#pragma once

#include "AudioBackend.hpp"

#ifdef COREAUDIO_BACKEND

#include <AudioUnit/AudioUnit.h>
#include <CoreAudio/CoreAudio.h>

namespace MayaFlux::Core {

/**
 * @class CoreAudioBackend
 * @brief Native macOS CoreAudio backend.
 *
 * Provides device enumeration and stream creation using the CoreAudio HAL.
 * Unlike RtAudio, this backend talks directly to CoreAudio and does not
 * rely on any third-party abstraction layer.
 */
class CoreAudioBackend final : public IAudioBackend {
public:
    CoreAudioBackend();
    ~CoreAudioBackend() override;

    std::unique_ptr<AudioDevice> create_device_manager() override;

    std::unique_ptr<AudioStream> create_stream(
        unsigned int output_device_id,
        unsigned int input_device_id,
        GlobalStreamInfo& stream_info,
        void* user_data) override;

    [[nodiscard]] std::string get_version_string() const override;
    [[nodiscard]] int get_api_type() const override;

    void cleanup() override;
};

/**
 * @class CoreAudioDevice
 * @brief Enumerates CoreAudio devices and default endpoints.
 */
class CoreAudioDevice final : public AudioDevice {
public:
    CoreAudioDevice();
    ~CoreAudioDevice() override = default;

    [[nodiscard]] std::vector<DeviceInfo> get_output_devices() const override;
    [[nodiscard]] std::vector<DeviceInfo> get_input_devices() const override;

    [[nodiscard]] unsigned int get_default_output_device() const override;
    [[nodiscard]] unsigned int get_default_input_device() const override;

private:
    struct DeviceEntry {
        AudioDeviceID id {};
        DeviceInfo info;
    };

    void enumerate_devices();

    std::vector<DeviceEntry> m_outputs;
    std::vector<DeviceEntry> m_inputs;

    unsigned int m_default_output = 0;
    unsigned int m_default_input = 0;
    AudioDeviceID m_default_output_id = kAudioObjectUnknown;
    AudioDeviceID m_default_input_id = kAudioObjectUnknown;

    [[nodiscard]] static std::string get_device_name(AudioDeviceID device_id);
    [[nodiscard]] static uint32_t get_channel_count(
        AudioDeviceID device_id,
        AudioObjectPropertyScope scope);

    [[nodiscard]] static double get_nominal_sample_rate(
        AudioDeviceID device_id);
};

/**
 * @class CoreAudioStream
 * @brief Duplex CoreAudio stream backed by a HAL AudioUnit.
 *
 * CoreAudio drives the render callback from its own realtime thread.
 * Input capture is pulled via AudioUnitRender() from the input bus
 * and presented to the MayaFlux callback using the standard
 * output/input/frame-count abstraction.
 */
class CoreAudioStream final : public AudioStream {
public:
    CoreAudioStream(
        AudioDeviceID output_device_id,
        AudioDeviceID input_device_id,
        GlobalStreamInfo& stream_info,
        void* user_data);

    ~CoreAudioStream() override;

    void open() override;
    void start() override;
    void stop() override;
    void pause() override;
    void resume() override;
    void close() override;

    [[nodiscard]] bool is_running() const override;
    [[nodiscard]] bool is_open() const override;

    void set_process_callback(
        std::function<int(void*, void*, unsigned int)> callback) override;

private:
    /**
     * @brief CoreAudio realtime callback.
     */
    static OSStatus render_callback(
        void* ref_con,
        AudioUnitRenderActionFlags* action_flags,
        const AudioTimeStamp* time_stamp,
        UInt32 bus_number,
        UInt32 num_frames,
        AudioBufferList* io_data);

    bool configure_audio_unit();
    bool configure_devices();
    bool configure_stream_format();

private:
    AudioUnit m_audio_unit = nullptr;

    AudioDeviceID m_output_device_id = kAudioObjectUnknown;
    AudioDeviceID m_input_device_id = kAudioObjectUnknown;

    GlobalStreamInfo& m_stream_info;
    void* m_user_data = nullptr;

    std::function<int(void*, void*, unsigned int)> m_process_callback;

    std::vector<double> m_input_staging;
    std::vector<double> m_output_staging;

    std::atomic<bool> m_is_open { false };
    std::atomic<bool> m_is_running { false };
    std::atomic<bool> m_is_paused { false };
};

} // namespace MayaFlux::Core

#endif // COREAUDIO_BACKEND
