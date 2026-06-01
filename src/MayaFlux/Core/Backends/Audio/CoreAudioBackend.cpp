#include "CoreAudioBackend.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

#ifdef COREAUDIO_BACKEND

namespace {

constexpr auto C = MayaFlux::Journal::Component::Core;
constexpr auto X = MayaFlux::Journal::Context::AudioBackend;

bool property_exists(
    AudioObjectID object,
    const AudioObjectPropertyAddress& address)
{
    return AudioObjectHasProperty(object, &address);
}

} // namespace

namespace MayaFlux::Core {

// ============================================================================
// CoreAudioBackend
// ============================================================================

CoreAudioBackend::CoreAudioBackend()
{
    MF_INFO(C, X, "CoreAudio backend initialised");
}

CoreAudioBackend::~CoreAudioBackend()
{
    cleanup();
}

std::unique_ptr<AudioDevice> CoreAudioBackend::create_device_manager()
{
    return std::make_unique<CoreAudioDevice>();
}

std::unique_ptr<AudioStream> CoreAudioBackend::create_stream(
    unsigned int output_device_id,
    unsigned int input_device_id,
    GlobalStreamInfo& stream_info,
    void* user_data)
{
    CoreAudioDevice devices;

    return std::make_unique<CoreAudioStream>(
        devices.resolve_output_device(output_device_id),
        devices.resolve_input_device(input_device_id),
        stream_info,
        user_data);
}

std::string CoreAudioBackend::get_version_string() const
{
    return "CoreAudio";
}

int CoreAudioBackend::get_api_type() const
{
    return static_cast<int>(GlobalStreamInfo::AudioApi::CORE);
}

void CoreAudioBackend::cleanup()
{
}

// ============================================================================
// CoreAudioDevice
// ============================================================================

CoreAudioDevice::CoreAudioDevice()
{
    enumerate_devices();
}

std::vector<DeviceInfo> CoreAudioDevice::get_output_devices() const
{
    std::vector<DeviceInfo> result;
    result.reserve(m_outputs.size());

    for (const auto& device : m_outputs)
        result.push_back(device.info);

    return result;
}

std::vector<DeviceInfo> CoreAudioDevice::get_input_devices() const
{
    std::vector<DeviceInfo> result;
    result.reserve(m_inputs.size());

    for (const auto& device : m_inputs)
        result.push_back(device.info);

    return result;
}

unsigned int CoreAudioDevice::get_default_output_device() const
{
    return m_default_output;
}

unsigned int CoreAudioDevice::get_default_input_device() const
{
    return m_default_input;
}

AudioDeviceID CoreAudioDevice::resolve_output_device(
    unsigned int index) const
{
    if (index >= m_outputs.size())
        return m_default_output_id;

    return m_outputs[index].id;
}

AudioDeviceID CoreAudioDevice::resolve_input_device(
    unsigned int index) const
{
    if (index >= m_inputs.size())
        return m_default_input_id;

    return m_inputs[index].id;
}

std::string CoreAudioDevice::get_device_name(AudioDeviceID device_id)
{
    AudioObjectPropertyAddress address {
        .mSelector = kAudioObjectPropertyName,
        .mScope = kAudioObjectPropertyScopeGlobal,
        .mElement = kAudioObjectPropertyElementMain
    };

    CFStringRef name_ref = nullptr;
    UInt32 size = sizeof(CFStringRef);

    auto status = AudioObjectGetPropertyData(
        device_id,
        &address,
        0,
        nullptr,
        &size,
        static_cast<void*>(&name_ref));

    if (status != noErr || !name_ref)
        return "Unknown Device";

    char buffer[512] {};

    CFStringGetCString(
        name_ref,
        buffer,
        sizeof(buffer),
        kCFStringEncodingUTF8);

    CFRelease(name_ref);

    return buffer;
}

uint32_t CoreAudioDevice::get_channel_count(
    AudioDeviceID device_id,
    AudioObjectPropertyScope scope)
{
    AudioObjectPropertyAddress address {
        .mSelector = kAudioDevicePropertyStreamConfiguration,
        .mScope = scope,
        .mElement = kAudioObjectPropertyElementMain
    };

    UInt32 size = 0;

    auto status = AudioObjectGetPropertyDataSize(
        device_id,
        &address,
        0,
        nullptr,
        &size);

    if (status != noErr || size == 0)
        return 0;

    auto buffer = std::make_unique<uint8_t[]>(size);

    auto* config = reinterpret_cast<AudioBufferList*>(buffer.get());

    status = AudioObjectGetPropertyData(
        device_id,
        &address,
        0,
        nullptr,
        &size,
        config);

    if (status != noErr)
        return 0;

    uint32_t channels = 0;

    for (UInt32 i = 0; i < config->mNumberBuffers; ++i)
        channels += config->mBuffers[i].mNumberChannels;

    return channels;
}

double CoreAudioDevice::get_nominal_sample_rate(
    AudioDeviceID device_id)
{
    AudioObjectPropertyAddress address {
        .mSelector = kAudioDevicePropertyNominalSampleRate,
        .mScope = kAudioObjectPropertyScopeGlobal,
        .mElement = kAudioObjectPropertyElementMain
    };

    Float64 rate = 48000.0;
    UInt32 size = sizeof(rate);

    auto status = AudioObjectGetPropertyData(
        device_id,
        &address,
        0,
        nullptr,
        &size,
        &rate);

    if (status != noErr)
        return 48000.0;

    return static_cast<double>(rate);
}

void CoreAudioDevice::enumerate_devices()
{
    AudioObjectPropertyAddress devices_address {
        .mSelector = kAudioHardwarePropertyDevices,
        .mScope = kAudioObjectPropertyScopeGlobal,
        .mElement = kAudioObjectPropertyElementMain
    };

    UInt32 size = 0;

    auto status = AudioObjectGetPropertyDataSize(
        kAudioObjectSystemObject,
        &devices_address,
        0,
        nullptr,
        &size);

    if (status != noErr) {
        error<std::runtime_error>(
            C,
            X,
            std::source_location::current(),
            "Failed to query CoreAudio device list");
    }

    std::vector<AudioDeviceID> device_ids(
        size / sizeof(AudioDeviceID));

    status = AudioObjectGetPropertyData(
        kAudioObjectSystemObject,
        &devices_address,
        0,
        nullptr,
        &size,
        device_ids.data());

    if (status != noErr) {
        error<std::runtime_error>(
            C,
            X,
            std::source_location::current(),
            "Failed to retrieve CoreAudio device list");
    }

    {
        AudioObjectPropertyAddress address {
            .mSelector = kAudioHardwarePropertyDefaultOutputDevice,
            .mScope = kAudioObjectPropertyScopeGlobal,
            .mElement = kAudioObjectPropertyElementMain
        };

        UInt32 property_size = sizeof(AudioDeviceID);

        AudioObjectGetPropertyData(
            kAudioObjectSystemObject,
            &address,
            0,
            nullptr,
            &property_size,
            &m_default_output_id);
    }

    {
        AudioObjectPropertyAddress address {
            .mSelector = kAudioHardwarePropertyDefaultInputDevice,
            .mScope = kAudioObjectPropertyScopeGlobal,
            .mElement = kAudioObjectPropertyElementMain
        };

        UInt32 property_size = sizeof(AudioDeviceID);

        AudioObjectGetPropertyData(
            kAudioObjectSystemObject,
            &address,
            0,
            nullptr,
            &property_size,
            &m_default_input_id);
    }

    for (auto device_id : device_ids) {

        const auto name = get_device_name(device_id);

        const auto output_channels = get_channel_count(
            device_id,
            kAudioDevicePropertyScopeOutput);

        const auto input_channels = get_channel_count(
            device_id,
            kAudioDevicePropertyScopeInput);

        const auto sample_rate = get_nominal_sample_rate(device_id);

        if (output_channels > 0) {

            DeviceInfo info;
            info.name = name;
            info.output_channels = output_channels;
            info.input_channels = 0;
            info.preferred_sample_rate = static_cast<uint32_t>(sample_rate);

            info.is_default_output = (device_id == m_default_output_id);

            if (info.is_default_output)
                m_default_output = static_cast<unsigned int>(m_outputs.size());

            m_outputs.push_back({ device_id,
                std::move(info) });
        }

        if (input_channels > 0) {

            DeviceInfo info;
            info.name = name;
            info.output_channels = 0;
            info.input_channels = input_channels;
            info.preferred_sample_rate = static_cast<uint32_t>(sample_rate);

            info.is_default_input = (device_id == m_default_input_id);

            if (info.is_default_input)
                m_default_input = static_cast<unsigned int>(m_inputs.size());

            m_inputs.push_back({ device_id,
                std::move(info) });
        }
    }

    MF_INFO(
        C,
        X,
        "CoreAudio enumeration: {} output device(s), {} input device(s)",
        m_outputs.size(),
        m_inputs.size());
}

// ============================================================================
// CoreAudioStream
// ============================================================================

CoreAudioStream::CoreAudioStream(
    AudioDeviceID output_device_id,
    AudioDeviceID input_device_id,
    GlobalStreamInfo& stream_info,
    void* user_data)
    : m_output_device_id(output_device_id)
    , m_input_device_id(input_device_id)
    , m_stream_info(stream_info)
    , m_user_data(user_data)
{
}

CoreAudioStream::~CoreAudioStream()
{
    if (is_running())
        stop();

    if (is_open())
        close();
}

void CoreAudioStream::open()
{
    if (m_is_open.load())
        return;

    if (!configure_output_unit()) {
        error<std::runtime_error>(C, X, std::source_location::current(),
            "Failed to configure CoreAudio output unit");
    }

    if (!configure_output_device()) {
        error<std::runtime_error>(C, X, std::source_location::current(),
            "Failed to configure CoreAudio output device");
    }

    if (!configure_output_format()) {
        error<std::runtime_error>(C, X, std::source_location::current(),
            "Failed to configure CoreAudio output format");
    }

    auto max_frames = static_cast<UInt32>(m_stream_info.buffer_size);

    AudioUnitSetProperty(
        m_output_unit,
        kAudioUnitProperty_MaximumFramesPerSlice,
        kAudioUnitScope_Global,
        0,
        &max_frames,
        sizeof(max_frames));

    auto status = AudioUnitInitialize(m_output_unit);

    if (status != noErr) {
        error<std::runtime_error>(C, X, std::source_location::current(),
            "AudioUnitInitialize failed for output ({})", static_cast<int>(status));
    }

    m_input_enabled.store(
        m_stream_info.input.enabled && m_stream_info.input.channels > 0,
        std::memory_order_release);

    if (m_input_enabled.load(std::memory_order_acquire)) {

        if (!configure_input_unit()) {
            error<std::runtime_error>(C, X, std::source_location::current(),
                "Failed to configure CoreAudio input unit");
        }

        if (!configure_input_device()) {
            error<std::runtime_error>(C, X, std::source_location::current(),
                "Failed to configure CoreAudio input device");
        }

        if (!configure_input_format()) {
            error<std::runtime_error>(C, X, std::source_location::current(),
                "Failed to configure CoreAudio input format");
        }

        AudioUnitSetProperty(
            m_input_unit,
            kAudioUnitProperty_MaximumFramesPerSlice,
            kAudioUnitScope_Global,
            0,
            &max_frames,
            sizeof(max_frames));

        status = AudioUnitInitialize(m_input_unit);

        if (status != noErr) {
            error<std::runtime_error>(C, X, std::source_location::current(),
                "AudioUnitInitialize failed for input ({})", static_cast<int>(status));
        }
    }

    m_is_open.store(true);
}

void CoreAudioStream::start()
{
    if (!m_is_open.load()) {
        error<std::runtime_error>(C, X, std::source_location::current(),
            "Cannot start stream before open()");
    }

    if (m_is_running.load())
        return;

    if (m_input_enabled.load(std::memory_order_acquire)) {
        auto status = AudioOutputUnitStart(m_input_unit);
        if (status != noErr) {
            error<std::runtime_error>(C, X, std::source_location::current(),
                "AudioOutputUnitStart failed for input ({})", static_cast<int>(status));
        }
    }

    auto status = AudioOutputUnitStart(m_output_unit);

    if (status != noErr) {
        error<std::runtime_error>(C, X, std::source_location::current(),
            "AudioOutputUnitStart failed for output ({})", static_cast<int>(status));
    }

    m_is_running.store(true);
}

void CoreAudioStream::stop()
{
    if (!m_is_running.load())
        return;

    AudioOutputUnitStop(m_output_unit);

    if (m_input_enabled.load(std::memory_order_acquire))
        AudioOutputUnitStop(m_input_unit);

    m_is_running.store(false);
}

void CoreAudioStream::pause()
{
    stop();
    m_is_paused.store(true, std::memory_order_release);
}

void CoreAudioStream::resume()
{
    if (!m_is_paused.load(std::memory_order_acquire))
        return;

    start();
    m_is_paused.store(false, std::memory_order_release);
}

void CoreAudioStream::close()
{
    if (!m_is_open.load())
        return;

    if (m_is_running.load())
        stop();

    if (m_input_buffer_list) {
        std::free(m_input_buffer_list->mBuffers[0].mData);
        std::free(m_input_buffer_list);
        m_input_buffer_list = nullptr;
    }

    if (m_input_unit) {
        AudioUnitUninitialize(m_input_unit);
        AudioComponentInstanceDispose(m_input_unit);
        m_input_unit = nullptr;
    }

    AudioUnitUninitialize(m_output_unit);
    AudioComponentInstanceDispose(m_output_unit);
    m_output_unit = nullptr;

    m_is_open.store(false);
}

bool CoreAudioStream::is_running() const
{
    return m_is_running.load(std::memory_order_acquire);
}

bool CoreAudioStream::is_open() const
{
    return m_is_open.load(std::memory_order_acquire);
}

void CoreAudioStream::set_process_callback(
    std::function<int(void*, void*, unsigned int)> callback)
{
    m_process_callback = std::move(callback);
}

bool CoreAudioStream::configure_output_unit()
{
    AudioComponentDescription desc {};
    desc.componentType = kAudioUnitType_Output;
    desc.componentSubType = kAudioUnitSubType_HALOutput;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;

    AudioComponent component = AudioComponentFindNext(nullptr, &desc);

    if (!component) {
        MF_ERROR(C, X, "Failed to find HALOutput AudioUnit");
        return false;
    }

    auto status = AudioComponentInstanceNew(component, &m_output_unit);

    if (status != noErr || !m_output_unit) {
        MF_ERROR(C, X, "AudioComponentInstanceNew failed for output ({})",
            static_cast<int>(status));
        return false;
    }

    UInt32 enable = 1;

    status = AudioUnitSetProperty(
        m_output_unit,
        kAudioOutputUnitProperty_EnableIO,
        kAudioUnitScope_Output,
        0,
        &enable,
        sizeof(enable));

    if (status != noErr) {
        MF_ERROR(C, X, "Failed to enable output bus ({})", static_cast<int>(status));
        return false;
    }

    UInt32 disable = 0;

    status = AudioUnitSetProperty(
        m_output_unit,
        kAudioOutputUnitProperty_EnableIO,
        kAudioUnitScope_Input,
        1,
        &disable,
        sizeof(disable));

    if (status != noErr) {
        MF_ERROR(C, X, "Failed to disable input bus on output unit ({})",
            static_cast<int>(status));
        return false;
    }

    AURenderCallbackStruct callback {};
    callback.inputProc = &CoreAudioStream::output_callback;
    callback.inputProcRefCon = this;

    status = AudioUnitSetProperty(
        m_output_unit,
        kAudioUnitProperty_SetRenderCallback,
        kAudioUnitScope_Input,
        0,
        &callback,
        sizeof(callback));

    if (status != noErr) {
        MF_ERROR(C, X, "Failed to register render callback ({})", static_cast<int>(status));
        return false;
    }

    return true;
}

bool CoreAudioStream::configure_input_unit()
{
    AudioComponentDescription desc {};
    desc.componentType = kAudioUnitType_Output;
    desc.componentSubType = kAudioUnitSubType_HALOutput;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;

    AudioComponent component = AudioComponentFindNext(nullptr, &desc);

    if (!component) {
        MF_ERROR(C, X, "Failed to find HALOutput AudioUnit for input");
        return false;
    }

    auto status = AudioComponentInstanceNew(component, &m_input_unit);

    if (status != noErr || !m_input_unit) {
        MF_ERROR(C, X, "AudioComponentInstanceNew failed for input ({})",
            static_cast<int>(status));
        return false;
    }

    UInt32 disable = 0;

    status = AudioUnitSetProperty(
        m_input_unit,
        kAudioOutputUnitProperty_EnableIO,
        kAudioUnitScope_Output,
        0,
        &disable,
        sizeof(disable));

    if (status != noErr) {
        MF_ERROR(C, X, "Failed to disable output bus on input unit ({})",
            static_cast<int>(status));
        return false;
    }

    UInt32 enable = 1;

    status = AudioUnitSetProperty(
        m_input_unit,
        kAudioOutputUnitProperty_EnableIO,
        kAudioUnitScope_Input,
        1,
        &enable,
        sizeof(enable));

    if (status != noErr) {
        MF_ERROR(C, X, "Failed to enable input bus ({})", static_cast<int>(status));
        return false;
    }

    AURenderCallbackStruct callback {};
    callback.inputProc = &CoreAudioStream::input_callback;
    callback.inputProcRefCon = this;

    status = AudioUnitSetProperty(
        m_input_unit,
        kAudioOutputUnitProperty_SetInputCallback,
        kAudioUnitScope_Global,
        0,
        &callback,
        sizeof(callback));

    if (status != noErr) {
        MF_ERROR(C, X, "Failed to register input callback ({})", static_cast<int>(status));
        return false;
    }

    return true;
}

bool CoreAudioStream::configure_output_device()
{
    auto status = AudioUnitSetProperty(
        m_output_unit,
        kAudioOutputUnitProperty_CurrentDevice,
        kAudioUnitScope_Global,
        0,
        &m_output_device_id,
        sizeof(m_output_device_id));

    if (status != noErr) {
        MF_ERROR(C, X, "Failed to set output device {} ({})",
            static_cast<unsigned>(m_output_device_id), static_cast<int>(status));
        return false;
    }

    AudioObjectPropertyAddress buffer_addr {
        .mSelector = kAudioDevicePropertyBufferFrameSize,
        .mScope = kAudioObjectPropertyScopeGlobal,
        .mElement = kAudioObjectPropertyElementMain
    };

    auto requested = static_cast<UInt32>(m_stream_info.buffer_size);

    AudioObjectSetPropertyData(
        m_output_device_id,
        &buffer_addr,
        0,
        nullptr,
        sizeof(requested),
        &requested);

    UInt32 actual = 0;
    UInt32 actual_size = sizeof(actual);

    if (AudioObjectGetPropertyData(m_output_device_id, &buffer_addr,
            0, nullptr, &actual_size, &actual)
        == noErr) {
        MF_INFO(C, X, "Output buffer size: requested={} actual={}", requested, actual);
    }

    return true;
}

bool CoreAudioStream::configure_input_device()
{
    auto status = AudioUnitSetProperty(
        m_input_unit,
        kAudioOutputUnitProperty_CurrentDevice,
        kAudioUnitScope_Global,
        0,
        &m_input_device_id,
        sizeof(m_input_device_id));

    if (status != noErr) {
        MF_ERROR(C, X, "Failed to set input device {} ({})",
            static_cast<unsigned>(m_input_device_id), static_cast<int>(status));
        return false;
    }

    AudioObjectPropertyAddress buffer_addr {
        .mSelector = kAudioDevicePropertyBufferFrameSize,
        .mScope = kAudioObjectPropertyScopeGlobal,
        .mElement = kAudioObjectPropertyElementMain
    };

    auto requested = static_cast<UInt32>(m_stream_info.buffer_size);

    AudioObjectSetPropertyData(
        m_input_device_id,
        &buffer_addr,
        0,
        nullptr,
        sizeof(requested),
        &requested);

    UInt32 actual = 0;
    UInt32 actual_size = sizeof(actual);

    if (AudioObjectGetPropertyData(m_input_device_id, &buffer_addr,
            0, nullptr, &actual_size, &actual)
        == noErr) {
        MF_INFO(C, X, "Input buffer size: requested={} actual={}", requested, actual);
    }

    return true;
}

bool CoreAudioStream::configure_output_format()
{
    AudioStreamBasicDescription format {};
    format.mSampleRate = m_stream_info.sample_rate;
    format.mFormatID = kAudioFormatLinearPCM;
    format.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
    format.mBitsPerChannel = 64;
    format.mChannelsPerFrame = m_stream_info.output.channels;
    format.mFramesPerPacket = 1;
    format.mBytesPerFrame = sizeof(double) * format.mChannelsPerFrame;
    format.mBytesPerPacket = format.mBytesPerFrame;

    auto status = AudioUnitSetProperty(
        m_output_unit,
        kAudioUnitProperty_StreamFormat,
        kAudioUnitScope_Input,
        0,
        &format,
        sizeof(format));

    if (status != noErr) {
        MF_ERROR(C, X, "Failed to set output stream format ({})", static_cast<int>(status));
        return false;
    }

    MF_LOG(C, X, "Output format: {} channels {} Hz {} bits",
        format.mChannelsPerFrame, format.mSampleRate, format.mBitsPerChannel);

    return true;
}

bool CoreAudioStream::configure_input_format()
{
    AudioStreamBasicDescription format {};
    format.mSampleRate = m_stream_info.sample_rate;
    format.mFormatID = kAudioFormatLinearPCM;
    format.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
    format.mBitsPerChannel = 64;
    format.mChannelsPerFrame = m_stream_info.input.channels;
    format.mFramesPerPacket = 1;
    format.mBytesPerFrame = sizeof(double) * format.mChannelsPerFrame;
    format.mBytesPerPacket = format.mBytesPerFrame;

    auto status = AudioUnitSetProperty(
        m_input_unit,
        kAudioUnitProperty_StreamFormat,
        kAudioUnitScope_Output,
        1,
        &format,
        sizeof(format));

    if (status != noErr) {
        MF_ERROR(C, X, "Failed to set input stream format ({})", static_cast<int>(status));
        return false;
    }

    const auto bytes = static_cast<UInt32>(
        sizeof(double) * m_stream_info.input.channels * m_stream_info.buffer_size);

    m_input_buffer_list = static_cast<AudioBufferList*>(
        std::calloc(1, sizeof(AudioBufferList) + sizeof(AudioBuffer)));

    m_input_buffer_list->mNumberBuffers = 1;
    m_input_buffer_list->mBuffers[0].mNumberChannels = m_stream_info.input.channels;
    m_input_buffer_list->mBuffers[0].mDataByteSize = bytes;
    m_input_buffer_list->mBuffers[0].mData = std::calloc(1, bytes);

    MF_LOG(C, X, "Input format: {} channels {} Hz {} bits",
        format.mChannelsPerFrame, format.mSampleRate, format.mBitsPerChannel);

    return true;
}

OSStatus CoreAudioStream::output_callback(
    void* ref_con,
    AudioUnitRenderActionFlags*,
    const AudioTimeStamp*,
    UInt32,
    UInt32 num_frames,
    AudioBufferList* io_data)
{
    auto* stream = static_cast<CoreAudioStream*>(ref_con);

    if (!stream)
        return noErr;

    if (!stream->m_process_callback) {
        for (UInt32 b = 0; b < io_data->mNumberBuffers; ++b)
            std::memset(io_data->mBuffers[b].mData, 0, io_data->mBuffers[b].mDataByteSize);
        return noErr;
    }

    auto* output = static_cast<double*>(io_data->mBuffers[0].mData);

    void* input = stream->m_input_enabled.load(std::memory_order_acquire)
        ? stream->m_input_buffer_list->mBuffers[0].mData
        : nullptr;

    stream->m_process_callback(output, input, num_frames);

    return noErr;
}

OSStatus CoreAudioStream::input_callback(
    void* ref_con,
    AudioUnitRenderActionFlags* action_flags,
    const AudioTimeStamp* time_stamp,
    UInt32 bus_number,
    UInt32 num_frames,
    AudioBufferList*)
{
    auto* stream = static_cast<CoreAudioStream*>(ref_con);

    if (!stream || !stream->m_input_buffer_list)
        return noErr;

    stream->m_input_buffer_list->mBuffers[0].mDataByteSize = static_cast<UInt32>(
        sizeof(double) * stream->m_stream_info.input.channels * num_frames);

    AudioUnitRender(
        stream->m_input_unit,
        action_flags,
        time_stamp,
        bus_number,
        num_frames,
        stream->m_input_buffer_list);

    return noErr;
}

} // namespace MayaFlux::Core

#endif // COREAUDIO_BACKEND
