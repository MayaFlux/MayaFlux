#pragma once

#include "InputBackend.hpp"

#ifdef PIPEWIRE_BACKEND

#include <alsa/asoundlib.h>
#include <pipewire/pipewire.h>

namespace MayaFlux::Core {

/**
 * @class PipewireMidiBackend
 * @brief PipeWire-native MIDI input backend
 *
 * Enumerates Midi/Source nodes from the PipeWire registry and opens
 * a pw_stream per port. All streams share a single pw_thread_loop,
 * avoiding the overhead of one loop per port.
 *
 * Threading model:
 * - One pw_thread_loop drives all MIDI port streams
 * - The process callback fires on that loop's thread for each port
 * - Callbacks push to InputManager's queue (thread-safe)
 * - Registry enumeration and hotplug run on the same loop thread
 * - m_devices_mutex guards m_enumerated_devices and m_open_devices
 *   across the loop thread and any external caller
 */
class MAYAFLUX_API PipewireMidiBackend : public IInputBackend {
public:
    struct Config {
        std::vector<std::string> input_port_filters;
        std::vector<std::string> output_port_filters;
        bool auto_open_inputs { true };
        bool auto_open_outputs { false };
        bool enable_virtual_port { false };
        std::string virtual_port_name { "MayaFlux" };
    };

    PipewireMidiBackend();
    explicit PipewireMidiBackend(Config config);
    ~PipewireMidiBackend() override;

    PipewireMidiBackend(const PipewireMidiBackend&) = delete;
    PipewireMidiBackend& operator=(const PipewireMidiBackend&) = delete;
    PipewireMidiBackend(PipewireMidiBackend&&) = delete;
    PipewireMidiBackend& operator=(PipewireMidiBackend&&) = delete;

    void register_midi_port(uint32_t pw_id, const std::string& name, const std::string& object_path);

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
    [[nodiscard]] std::string get_name() const override { return "MIDI (PipeWire)"; }
    [[nodiscard]] std::string get_version() const override;

private:
    struct MIDIPortInfo : InputDeviceInfo {
        uint32_t pw_global_id { 0 };
        std::string object_path;
        int alsa_client { -1 };
        int alsa_port { -1 };
    };

    struct MIDIPortState {
        MIDIPortInfo info;
        uint32_t device_id { 0 };
        std::atomic<bool> active { false };
        std::function<void(const InputValue&)> input_callback;
        snd_seq_t* seq_handle { nullptr };
        int seq_port { -1 };
        std::thread poll_thread;
    };

    // ===================================================================================
    // Enumeration state (used during registry roundtrip and persistent hotplug)
    // ===================================================================================

    Config m_config;
    std::atomic<bool> m_initialized { false };
    std::atomic<bool> m_running { false };

    pw_thread_loop* m_thread_loop { nullptr };
    pw_context* m_context { nullptr };
    pw_core* m_core { nullptr };

    pw_registry* m_registry { nullptr };
    spa_hook m_registry_listener {};
    spa_hook m_core_listener {};

    mutable std::mutex m_devices_mutex;
    std::unordered_map<uint32_t, MIDIPortInfo> m_enumerated_devices;
    std::unordered_map<uint32_t, std::shared_ptr<MIDIPortState>> m_open_devices;
    uint32_t m_next_device_id { 1 };

    InputCallback m_input_callback;
    DeviceCallback m_device_callback;
    mutable std::mutex m_callback_mutex;

    // ===================================================================================
    // Private helpers
    // ===================================================================================

    bool port_matches_filter(const std::string& port_name) const;
    uint32_t find_or_assign_device_id(uint32_t pw_global_id);
    void create_virtual_port_if_enabled();
    void notify_device_change(const InputDeviceInfo& info, bool connected);

    // ===================================================================================
    // PipeWire callbacks (static, called on pw_thread_loop thread)
    // ===================================================================================

    static void on_registry_global(
        void* userdata,
        uint32_t id,
        uint32_t permissions,
        const char* type,
        uint32_t version,
        const struct spa_dict* props);

    static void on_registry_global_remove(void* userdata, uint32_t id);

    static void on_core_done(void* userdata, uint32_t id, int seq);
};

} // namespace MayaFlux::Core

#endif // PIPEWIRE_BACKEND
