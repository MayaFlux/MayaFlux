#pragma once

#include "InputBackend.hpp"
#include "MayaFlux/Core/Input/SerialDevice.hpp"
#include "MayaFlux/Transitive/Platform/SerialPorts.hpp"

#include "MayaFlux/Transitive/Memory/FlatMap.hpp"
#include "MayaFlux/Transitive/Memory/SnapshotCell.hpp"

namespace MayaFlux::Registry::Service {
struct NetworkService;
}

namespace MayaFlux::Core {

/**
 * @class SerialInputBackend
 * @brief The serial devices of the input system, one SerialDevice per open port.
 *
 * Devices come from the configured ports and, with enumerate_ports, from the ports the
 * system exposes. A device keeps its id for as long as the backend lives, found again by
 * the path it was configured with or the path the enumerator gave it, which is the stable
 * /dev/serial/by-id link where the system has one, so a replug does not change it. Every value a device parses is delivered to the input callback, from
 * the network io thread, once the backend is started.
 *
 * Ports are opened through the NetworkService, which must have the serial transport
 * enabled in GlobalNetworkConfig, and that endpoint does the reconnecting. The device
 * callback reports a device as connected while its port is open and as disconnected when
 * it leaves OPEN. A device uses the default spec, one line per message read as numbers,
 * unless set_device_spec gave it another before it was opened.
 *
 * write() sends the bytes as they are. A caller that wants requests and replies takes the
 * device from get_device().
 */
class MAYAFLUX_API SerialInputBackend : public IInputBackend {
public:
    struct Config {
        std::vector<SerialPortConfig> ports; ///< Ports to list, with the line setup of each
        bool enumerate_ports { false }; ///< Also list the ports the system exposes
        SerialLineSettings detected_line; ///< Line setup of enumerated ports
        SerialProtocolSettings detected_protocol; ///< Framing and parsing of enumerated ports
        Registry::Service::NetworkService* network { nullptr }; ///< Not owned, must outlive the backend
        std::function<std::vector<Platform::SerialPortEntry>()> enumerator; ///< Lists the system's ports, Platform::list_serial_ports when empty
    };

    explicit SerialInputBackend(Config config);
    ~SerialInputBackend() override;

    SerialInputBackend(const SerialInputBackend&) = delete;
    SerialInputBackend& operator=(const SerialInputBackend&) = delete;
    SerialInputBackend(SerialInputBackend&&) = delete;
    SerialInputBackend& operator=(SerialInputBackend&&) = delete;

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

    int write(uint32_t device_id, std::span<const uint8_t> data) override;

    void set_input_callback(InputCallback callback) override;
    void set_device_callback(DeviceCallback callback) override;

    [[nodiscard]] InputType get_type() const override { return InputType::SERIAL; }
    [[nodiscard]] std::string get_name() const override { return "Serial"; }
    [[nodiscard]] std::string get_version() const override { return "1.0"; }

    /**
     * @brief List a port that was not configured, or update one that was while it is closed.
     * @return The device id, or 0 before initialize().
     *
     * A port already listed under the same path keeps its id and takes the new line setup
     * and protocol unless it is open.
     */
    uint32_t add_port(const SerialPortConfig& port);

    /**
     * @brief Give a device its spec, before it is opened.
     * @return false for an unknown device or one that is open.
     */
    bool set_device_spec(uint32_t device_id, SerialSpec spec);

    /**
     * @brief List a second way of reading a device, as a device of its own.
     * @param device_id The device to read, never a reading itself.
     * @param spec Framing and parsing of this reading. Its line setup is not used.
     * @return The id of the reading, or 0 for an unknown device or a reading. Bind nodes to it.
     *
     * The reading sees the bytes of the device it reads and cuts and parses them on its own,
     * so one port can be read as lines by one node and as raw bytes by another. Opening it
     * opens the device, closing it only stops this reading, and its values carry its own
     * id. Writes to its id go to the device.
     */
    uint32_t add_view(uint32_t device_id, SerialSpec spec);

    using ReaderCallback = std::function<void(const InputValue&)>;

    /**
     * @brief Read a device's bytes a second way and hand the values to @p callback.
     * @return A token for remove_reader(), or 0 for an unknown device or a reading.
     *
     * Unlike a view, it is not listed as a device and does not go to the input callback. It
     * stays attached when the device is closed and opened again, and runs on the link's
     * thread. Values carry the id of the device.
     */
    uint64_t add_reader(uint32_t device_id, SerialSpec spec, ReaderCallback callback);

    /** @brief Stop a reader. No-op for an unknown token. */
    void remove_reader(uint64_t token);

    /** @brief The open device, or null. A reading resolves to the device it reads. For requests and replies. */
    [[nodiscard]] std::shared_ptr<SerialDevice> get_device(uint32_t device_id) const;

private:
    struct Entry {
        InputDeviceInfo info;
        SerialSpec spec;
        std::shared_ptr<SerialDevice> device;
        bool configured {};
        uint32_t parent {};
        uint64_t view_reader {};
    };

    struct ReaderRecord {
        uint64_t token {};
        uint32_t device_id {};
        uint32_t stamp {};
        SerialSpec spec;
        ReaderCallback callback;
        std::weak_ptr<SerialDevice> attached_to;
        SerialDevice::ReaderToken device_token {};
    };

    struct Roster {
        Memory::FlatMap<uint32_t, Entry> entries;
        std::map<std::string, uint32_t> ids;
        uint32_t next_id { 1 };
    };

    using DeviceEvent = std::pair<InputDeviceInfo, bool>;

    static uint32_t id_for(Roster& roster, const std::string& key);
    static Entry make_entry(uint32_t id, const std::string& path, const std::string& name,
        const SerialLineSettings& line, const SerialProtocolSettings& protocol, bool configured);

    uint64_t add_record(uint32_t device_id, uint32_t stamp, SerialSpec spec, ReaderCallback callback);
    void attach_readers(uint32_t device_id, const std::shared_ptr<SerialDevice>& device);
    void detach_view(uint32_t view_id);

    void on_link_state(uint32_t device_id, EndpointState previous, EndpointState current);
    void notify_input(const InputValue& value) const;
    void notify_device(const InputDeviceInfo& info, bool connected) const;

    Config m_config;
    std::atomic<bool> m_initialized { false };
    std::atomic<bool> m_running { false };

    Memory::SnapshotCell<Roster> m_roster;
    std::mutex m_reader_mutex;
    std::vector<ReaderRecord> m_records;
    std::atomic<uint64_t> m_next_reader { 1 };
    Memory::SnapshotCell<InputCallback> m_input_callback;
    Memory::SnapshotCell<DeviceCallback> m_device_callback;
};

} // namespace MayaFlux::Core
