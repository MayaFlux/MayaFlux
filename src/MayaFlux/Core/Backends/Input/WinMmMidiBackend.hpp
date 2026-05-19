#pragma once

#ifdef MAYAFLUX_PLATFORM_WINDOWS

#include "InputBackend.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>

namespace MayaFlux::Core {

/**
 * @class WinMmMidiBackend
 * @brief WinMM-native MIDI input backend for Windows
 *
 * Wraps midiIn* API directly. Zero additional dependencies beyond winmm.lib.
 * Compatible with Windows 10 and later. On Windows 11 24H2+ with Windows MIDI
 * Services installed, WinMM calls are automatically routed through the new
 * service, gaining multi-client support without any code changes.
 *
 * Threading model:
 * - midiInOpen uses CALLBACK_FUNCTION; Windows calls midi_callback on a
 *   dedicated MM thread per open device
 * - Callbacks push InputValue to InputManager's queue (thread-safe)
 * - No polling thread needed
 * - SysEx uses double-buffered MIDIHDR; buffers are re-queued from the callback
 */
class MAYAFLUX_API WinMmMidiBackend : public IInputBackend {
public:
    struct Config {
        std::vector<std::string> input_port_filters;
        bool auto_open_inputs { true };
        bool enable_sysex { true };
        std::string virtual_port_name { "MayaFlux" };
    };

    WinMmMidiBackend();
    explicit WinMmMidiBackend(Config config);
    ~WinMmMidiBackend() override;

    WinMmMidiBackend(const WinMmMidiBackend&) = delete;
    WinMmMidiBackend& operator=(const WinMmMidiBackend&) = delete;
    WinMmMidiBackend(WinMmMidiBackend&&) = delete;
    WinMmMidiBackend& operator=(WinMmMidiBackend&&) = delete;

    bool initialize() override;
    void start() override;
    void stop() override;
    void shutdown() override;

    [[nodiscard]] bool is_initialized() const override { return m_initialized.load(); }
    [[nodiscard]] bool is_running() const override { return m_running.load(); }

    [[nodiscard]] std::vector<InputDeviceInfo> get_devices() const override;
    size_t refresh_devices() override;

    bool open_device(uint32_t device_id) override;
    void close_device(uint32_t device_id) override;
    [[nodiscard]] bool is_device_open(uint32_t device_id) const override;
    [[nodiscard]] std::vector<uint32_t> get_open_devices() const override;

    void set_input_callback(InputCallback callback) override;
    void set_device_callback(DeviceCallback callback) override;

    [[nodiscard]] InputType get_type() const override { return InputType::MIDI; }
    [[nodiscard]] std::string get_name() const override { return "MIDI (WinMM)"; }
    [[nodiscard]] std::string get_version() const override;

private:
    static constexpr size_t k_sysex_buf_size { 256 };
    static constexpr size_t k_sysex_buf_count { 2 };

    struct MIDIPortInfo : InputDeviceInfo {
        UINT winmm_device_id { 0 };
    };

    struct MIDIPortState {
        MIDIPortInfo info;
        uint32_t device_id { 0 };
        HMIDIIN handle { nullptr };
        std::atomic<bool> active { false };
        std::function<void(const InputValue&)> input_callback;
        MIDIHDR sysex_headers[k_sysex_buf_count] {};
        std::array<std::vector<uint8_t>, k_sysex_buf_count> sysex_bufs;
    };

    Config m_config;
    std::atomic<bool> m_initialized { false };
    std::atomic<bool> m_running { false };

    mutable std::mutex m_devices_mutex;
    std::unordered_map<uint32_t, MIDIPortInfo> m_enumerated_devices;
    std::unordered_map<uint32_t, std::shared_ptr<MIDIPortState>> m_open_devices;
    uint32_t m_next_device_id { 1 };

    InputCallback m_input_callback;
    DeviceCallback m_device_callback;
    mutable std::mutex m_callback_mutex;

    bool port_matches_filter(const std::string& name) const;
    uint32_t find_or_assign_device_id(UINT winmm_id);
    void queue_sysex_buffers(MIDIPortState& state);
    void notify_device_change(const InputDeviceInfo& info, bool connected);

    static void CALLBACK midi_callback(HMIDIIN handle, UINT msg,
        DWORD_PTR user_data, DWORD_PTR param1, DWORD_PTR param2);
};

} // namespace MayaFlux::Core

#endif // MAYAFLUX_PLATFORM_WINDOWS
