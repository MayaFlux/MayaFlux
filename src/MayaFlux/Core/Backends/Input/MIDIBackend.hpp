#pragma once

#include "InputBackend.hpp"

class RtMidiIn;

namespace MayaFlux::Core {

/**
 * @class MIDIBackend
 * @brief RtMidi-based MIDI input backend
 *
 * Provides MIDI input functionality using RtMidi library. Supports:
 * - Multiple simultaneous MIDI input ports
 * - Port filtering by name
 * - Virtual MIDI port creation
 * - Automatic port detection
 *
 * Threading model:
 * - RtMidi callbacks fire on RtMidi's internal threads
 * - Callbacks push to InputManager's queue (thread-safe)
 * - No separate polling thread needed (RtMidi is callback-driven)
 */
class MAYAFLUX_API MIDIBackend : public IInputBackend {
public:
    struct Config {
        std::vector<std::string> input_port_filters;
        std::vector<std::string> output_port_filters;
        bool auto_open_inputs { true };
        bool auto_open_outputs { false };
        bool enable_virtual_port { false };
        std::string virtual_port_name { "MayaFlux" };
    };

    MIDIBackend();
    explicit MIDIBackend(Config config);
    ~MIDIBackend() override;

    MIDIBackend(const MIDIBackend&) = delete;
    MIDIBackend& operator=(const MIDIBackend&) = delete;
    MIDIBackend(MIDIBackend&&) = delete;
    MIDIBackend& operator=(MIDIBackend&&) = delete;

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
    [[nodiscard]] std::string get_name() const override { return "MIDI (RtMidi)"; }
    [[nodiscard]] std::string get_version() const override;

private:
    struct MIDIPortInfo : InputDeviceInfo {
        unsigned int rtmidi_port_number;
    };

    struct MIDIPortState {
        std::unique_ptr<RtMidiIn> midi_in;
        MIDIPortInfo info;
        uint32_t device_id;
        std::atomic<bool> active { false };

        std::function<void(const InputValue&)> input_callback;
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

    bool port_matches_filter(const std::string& port_name) const;
    uint32_t find_or_assign_device_id(unsigned int rtmidi_port);
    void create_virtual_port_if_enabled();

    static void rtmidi_callback(double timestamp, std::vector<unsigned char>* message, void* user_data);

    void notify_device_change(const InputDeviceInfo& info, bool connected);

    InputValue parse_midi_message(uint32_t device_id, const std::vector<unsigned char>& message) const;
};

} // namespace MayaFlux::Core
