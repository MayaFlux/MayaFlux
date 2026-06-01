#pragma once

#ifdef MAYAFLUX_PLATFORM_MACOS

#include "InputBackend.hpp"

#include <CoreMIDI/CoreMIDI.h>

namespace MayaFlux::Core {

/**
 * @class CoreMidiBackend
 * @brief CoreMIDI-native MIDI backend for macOS
 *
 * Wraps Apple's CoreMIDI framework directly with no third-party
 * dependencies.
 *
 * Intended feature parity:
 * - Multiple simultaneous MIDI input ports
 * - Port filtering by name
 * - Virtual MIDI port creation
 * - Device hotplug detection
 * - SysEx support
 * - MIDI output support
 *
 * Threading model:
 * - CoreMIDI invokes MIDIReadProc on CoreMIDI-managed threads
 * - Callbacks push InputValue to InputManager's queue (thread-safe)
 * - No polling thread required
 * - Device notifications arrive through MIDINotifyProc
 */
class MAYAFLUX_API CoreMidiBackend : public IInputBackend {
public:
    struct Config {
        std::vector<std::string> input_port_filters;
        std::vector<std::string> output_port_filters;
        bool auto_open_inputs { true };
        bool auto_open_outputs { false };
        bool enable_virtual_port { false };
        bool enable_sysex { true };
        std::string virtual_port_name { "MayaFlux" };
    };

    CoreMidiBackend();
    explicit CoreMidiBackend(Config config);
    ~CoreMidiBackend() override;

    CoreMidiBackend(const CoreMidiBackend&) = delete;
    CoreMidiBackend& operator=(const CoreMidiBackend&) = delete;
    CoreMidiBackend(CoreMidiBackend&&) = delete;
    CoreMidiBackend& operator=(CoreMidiBackend&&) = delete;

    // ===================================================================================
    // IInputBackend Implementation
    // ===================================================================================

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
    [[nodiscard]] std::string get_name() const override { return "MIDI (CoreMIDI)"; }
    [[nodiscard]] std::string get_version() const override;

private:
    struct MIDIPortInfo : InputDeviceInfo {
        MIDIUniqueID unique_id { 0 };
        MIDIEndpointRef endpoint { 0 };
    };

    struct MIDIPortState {
        MIDIPortInfo info;
        uint32_t device_id { 0 };

        MIDIPortRef input_port { 0 };

        std::atomic<bool> active { false };

        std::function<void(const InputValue&)> input_callback;
    };

    Config m_config;

    std::atomic<bool> m_initialized { false };
    std::atomic<bool> m_running { false };

    MIDIClientRef m_client { 0 };
    MIDIEndpointRef m_virtual_destination { 0 };

    mutable std::mutex m_devices_mutex;
    std::unordered_map<uint32_t, MIDIPortInfo> m_enumerated_devices;
    std::unordered_map<uint32_t, std::shared_ptr<MIDIPortState>> m_open_devices;
    uint32_t m_next_device_id { 1 };

    InputCallback m_input_callback;
    DeviceCallback m_device_callback;
    mutable std::mutex m_callback_mutex;

    bool port_matches_filter(const std::string& port_name) const;
    uint32_t find_or_assign_device_id(MIDIUniqueID unique_id);

    void create_virtual_port_if_enabled();
    void notify_device_change(const InputDeviceInfo& info, bool connected);

    static void virtual_destination_callback(
        const MIDIPacketList* packet_list,
        void* read_proc_ref_con,
        void* src_conn_ref_con);

    static void midi_read_callback(
        const MIDIPacketList* packet_list,
        void* read_proc_ref_con,
        void* src_conn_ref_con);

    static void midi_notify_callback(
        const MIDINotification* notification,
        void* ref_con);
};

} // namespace MayaFlux::Core

#endif // MAYAFLUX_PLATFORM_MACOS
