#include "Input.hpp"

#include <charconv>

#include "Core.hpp"
#include "MayaFlux/Core/Engine.hpp"
#include "MayaFlux/Core/Input/InputManager.hpp"
#include "MayaFlux/Core/Backends/Input/SerialInputBackend.hpp"
#include "MayaFlux/Core/Subsystems/InputSubsystem.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Portal/Forma/Primitives/Picker.hpp"
#include "MayaFlux/Transitive/Parallel/Dispatch.hpp"
#include "MayaFlux/Transitive/Platform/SerialPorts.hpp"

#include "Prompt.hpp"

namespace MayaFlux {

namespace {

    std::string describe_device(const Core::InputDeviceInfo& device)
    {
        std::string text = device.name;
        if (device.vendor_id != 0 || device.product_id != 0) {
            text += std::format("  {:04x}:{:04x}", device.vendor_id, device.product_id);
        }
        return text + std::format("  #{}", device.id);
    }

    Core::SerialInputBackend* serial_backend()
    {
        return dynamic_cast<Core::SerialInputBackend*>(
            get_input_subsystem().get_backend(Core::InputType::SERIAL));
    }

    Core::SerialInputBackend* require_serial_backend(std::string_view caller)
    {
        auto* backend = serial_backend();
        if (!backend) {
            MF_ERROR(Journal::Component::API, Journal::Context::Runtime,
                "{}: serial input is not enabled in GlobalInputConfig", caller);
        }
        return backend;
    }

    std::string unescape(std::string_view text)
    {
        std::string out;
        for (size_t i = 0; i < text.size(); ++i) {
            if (text[i] != '\\' || i + 1 == text.size()) {
                out += text[i];
                continue;
            }
            switch (text[++i]) {
            case 'n':
                out += '\n';
                break;
            case 'r':
                out += '\r';
                break;
            case 't':
                out += '\t';
                break;
            default:
                out += text[i];
                break;
            }
        }
        return out;
    }

    bool is_baud_rate(std::string_view text)
    {
        uint32_t rate = 0;
        const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), rate);
        return error == std::errc {} && end == text.data() + text.size() && rate > 0 && rate <= 10'000'000;
    }

    bool is_device_path(std::string_view text)
    {
        return !text.empty()
            && std::ranges::none_of(text, [](char c) { return static_cast<unsigned char>(c) < ' ' || c == 0x7F; });
    }

}

const Core::InputSubsystem& get_input_subsystem()
{
    auto subsystem = get_context().get_subsystem(Core::SubsystemType::INPUT);
    return *std::dynamic_pointer_cast<Core::InputSubsystem>(subsystem);
}

Core::InputManager& get_input_manager()
{
    return *get_context().get_input_manager();
}

void register_input_node(const std::shared_ptr<Nodes::Input::InputNode>& node, const Core::InputBinding& binding)
{
    get_input_manager().register_node(node, binding);
}

void unregister_input_node(const std::shared_ptr<Nodes::Input::InputNode>& node)
{
    get_input_manager().unregister_node(node);
}

std::vector<Core::InputDeviceInfo> get_hid_devices()
{
    return get_input_subsystem().get_hid_devices();
}

std::vector<Core::InputDeviceInfo> get_midi_devices()
{
    return get_input_subsystem().get_midi_devices();
}

std::vector<Core::InputDeviceInfo> get_serial_devices()
{
    return get_input_subsystem().get_serial_devices();
}

std::vector<Core::InputDeviceInfo> get_tablet_devices()
{
    return get_input_subsystem().get_tablet_devices();
}

std::vector<Core::InputDeviceInfo> get_all_input_devices()
{
    return get_input_subsystem().get_all_devices();
}

std::optional<Core::InputDeviceInfo> find_hid_device(
    uint16_t vendor_id,
    uint16_t product_id)
{
    return get_input_subsystem().find_hid_device(vendor_id, product_id);
}

std::optional<Core::InputDeviceInfo> choose_input_device(Core::InputType type)
{
    std::vector<Core::InputDeviceInfo> devices;
    for (auto& device : get_all_input_devices()) {
        if (device.backend_type == type) {
            devices.push_back(std::move(device));
        }
    }

    if (devices.empty()) {
        MF_ERROR(Journal::Component::API, Journal::Context::Runtime,
            "choose_input_device: no devices found");
        return std::nullopt;
    }

    auto promise = std::make_shared<std::promise<std::optional<Core::InputDeviceInfo>>>();
    auto future = promise->get_future();

    Portal::Forma::pick("Choose a device", std::move(devices), describe_device,
        [promise](std::optional<Core::InputDeviceInfo> device) {
            promise->set_value(std::move(device));
        });

    return Parallel::await_future(std::move(future));
}

Core::InputBinding bind_hid(uint32_t device_id)
{
    return Core::InputBinding::hid(device_id);
}

Core::InputBinding bind_hid(uint16_t vid, uint16_t pid)
{
    return Core::InputBinding::hid_by_vid_pid(vid, pid);
}

Core::InputBinding bind_midi(uint32_t device_id, std::optional<uint8_t> channel)
{
    return Core::InputBinding::midi(device_id, channel);
}

Core::InputBinding bind_midi_cc(
    std::optional<uint8_t> cc_number,
    std::optional<uint8_t> channel,
    uint32_t device_id)
{
    return Core::InputBinding::midi_cc(cc_number, channel, device_id);
}

Core::InputBinding bind_midi_note_on(
    std::optional<uint8_t> channel,
    uint32_t device_id)
{
    return Core::InputBinding::midi_note_on(channel, device_id);
}

Core::InputBinding bind_midi_note_off(
    std::optional<uint8_t> channel,
    uint32_t device_id)
{
    return Core::InputBinding::midi_note_off(channel, device_id);
}

Core::InputBinding bind_midi_pitch_bend(
    std::optional<uint8_t> channel,
    uint32_t device_id)
{
    return Core::InputBinding::midi_pitch_bend(channel, device_id);
}

Core::InputBinding bind_osc(const std::string& pattern)
{
    return Core::InputBinding::osc(pattern);
}

Core::InputBinding bind_serial(uint32_t device_id)
{
    return Core::InputBinding::serial(device_id);
}

Core::InputBinding bind_tablet(uint32_t device_id)
{
    return Core::InputBinding::tablet(device_id);
}


std::optional<Core::InputDeviceInfo> open_serial_port(const Core::SerialPortConfig& port)
{
    auto* backend = require_serial_backend("open_serial_port");
    if (!backend) {
        return std::nullopt;
    }

    const uint32_t id = backend->add_port(port);
    if (id == 0 || !backend->open_device(id)) {
        return std::nullopt;
    }

    for (auto& device : backend->get_devices()) {
        if (device.id == id) {
            return device;
        }
    }
    return std::nullopt;
}

std::optional<Core::SerialPortConfig> choose_serial_port()
{
    struct PortChoice {
        std::string path;
        std::string label;
    };
    struct RateChoice {
        uint32_t rate;
        std::string label;
    };
    struct ProtocolChoice {
        std::string label;
        Core::SerialProtocolSettings settings;
    };
    struct RestartChoice {
        std::string label;
        uint32_t settle_ms;
    };

    std::vector<PortChoice> ports;
    for (const auto& port : Platform::list_serial_ports()) {
        ports.push_back({ port.path.string(), std::format("{}  {}", port.name, port.path.string()) });
    }

    Core::SerialPortConfig config;
    std::string path;
    if (!ports.empty()) {
        ports.push_back({ "", "Another path, such as a virtual port" });
        const auto port = detail::pick_one<PortChoice>("Choose a serial port", ports,
            [](const PortChoice& choice) { return choice.label; });
        if (!port) {
            return std::nullopt;
        }
        path = port->path;
    }
    if (path.empty()) {
        const auto typed = detail::ask_one("Serial port, a device path", "", is_device_path);
        if (!typed) {
            return std::nullopt;
        }
        path = *typed;
    }
    config.port_name = std::move(path);

    const std::vector<RateChoice> rates {
        { 9600, "9600" },
        { 19200, "19200" },
        { 31250, "31250, MIDI" },
        { 38400, "38400" },
        { 57600, "57600" },
        { 115200, "115200" },
        { 230400, "230400" },
        { 250000, "250000, DMX" },
        { 460800, "460800" },
        { 921600, "921600" },
        { 0, "Another rate" },
    };
    const auto rate = detail::pick_one<RateChoice>("Baud rate", rates,
        [](const RateChoice& choice) { return choice.label; });
    if (!rate) {
        return std::nullopt;
    }
    config.line.baud_rate = rate->rate;
    if (rate->rate == 0) {
        const auto typed = detail::ask_one("Baud rate", "115200", is_baud_rate);
        if (!typed) {
            return std::nullopt;
        }
        std::from_chars(typed->data(), typed->data() + typed->size(), config.line.baud_rate);
    }

    Core::SerialProtocolSettings numbers;
    Core::SerialProtocolSettings text;
    text.parse = Core::SerialParse::BYTES;
    Core::SerialProtocolSettings midi;
    midi.framing = Core::SerialFraming::RAW;
    midi.parse = Core::SerialParse::MIDI;
    Core::SerialProtocolSettings cobs;
    cobs.framing = Core::SerialFraming::COBS;
    cobs.parse = Core::SerialParse::BYTES;
    Core::SerialProtocolSettings slip;
    slip.framing = Core::SerialFraming::SLIP;
    slip.parse = Core::SerialParse::BYTES;
    Core::SerialProtocolSettings raw;
    raw.framing = Core::SerialFraming::RAW;
    raw.parse = Core::SerialParse::BYTES;
    Core::SerialProtocolSettings gap;
    gap.framing = Core::SerialFraming::IDLE_GAP;
    gap.parse = Core::SerialParse::BYTES;
    gap.gap_ms = 5;
    Core::SerialProtocolSettings delimited;
    delimited.framing = Core::SerialFraming::DELIMITED;
    delimited.parse = Core::SerialParse::BYTES;

    const std::vector<ProtocolChoice> protocols {
        { "Lines of numbers (Serial.println)", numbers },
        { "Lines of text, as bytes", text },
        { "MIDI byte stream", midi },
        { "COBS frames", cobs },
        { "SLIP frames", slip },
        { "Raw bytes, as they arrive", raw },
        { "A message ends after a pause", gap },
        { "Messages ended by a terminator", delimited },
    };
    const auto protocol = detail::pick_one<ProtocolChoice>("How the port is read", protocols,
        [](const ProtocolChoice& choice) { return choice.label; });
    if (!protocol) {
        return std::nullopt;
    }
    config.protocol = protocol->settings;

    if (config.protocol.framing == Core::SerialFraming::DELIMITED) {
        const auto typed = detail::ask_one("Terminator, with \\r \\n \\t for control characters", "\\r\\n",
            [](std::string_view candidate) { return !unescape(candidate).empty(); });
        if (!typed) {
            return std::nullopt;
        }
        config.protocol.terminator = unescape(*typed);
    }

    const std::vector<RestartChoice> restarts {
        { "Board restarts on open (Arduino): wait 2 s", 2000 },
        { "No restart: read at once", 0 },
    };
    const auto restart = detail::pick_one<RestartChoice>("When the port opens", restarts,
        [](const RestartChoice& choice) { return choice.label; });
    if (!restart) {
        return std::nullopt;
    }
    config.line.settle_ms = restart->settle_ms;

    return config;
}

std::optional<Core::InputDeviceInfo> open_serial_reading(uint32_t device_id, const Core::SerialSpec& spec)
{
    auto* backend = require_serial_backend("open_serial_reading");
    if (!backend) {
        return std::nullopt;
    }

    const uint32_t id = backend->add_view(device_id, spec);
    if (id == 0 || !backend->open_device(id)) {
        return std::nullopt;
    }

    for (auto& device : backend->get_devices()) {
        if (device.id == id) {
            return device;
        }
    }
    return std::nullopt;
}

bool write_serial(uint32_t device_id, std::span<const uint8_t> bytes)
{
    auto* backend = serial_backend();
    return backend && backend->write(device_id, bytes) >= 0;
}

bool write_serial(uint32_t device_id, std::string_view text)
{
    return write_serial(device_id,
        std::span<const uint8_t> { reinterpret_cast<const uint8_t*>(text.data()), text.size() });
}

bool send_serial_message(uint32_t device_id, std::span<const uint8_t> message)
{
    const auto device = get_serial_device(device_id);
    return device && device->send_framed(message);
}

std::shared_ptr<Core::SerialDevice> get_serial_device(uint32_t device_id)
{
    auto* backend = serial_backend();
    return backend ? backend->get_device(device_id) : nullptr;
}

bool set_serial_spec(uint32_t device_id, const Core::SerialSpec& spec)
{
    auto* backend = require_serial_backend("set_serial_spec");
    if (!backend) {
        return false;
    }

    const bool was_open = backend->is_device_open(device_id);
    if (was_open) {
        backend->close_device(device_id);
    }
    const bool set = backend->set_device_spec(device_id, spec);
    if (was_open) {
        backend->open_device(device_id);
    }
    return set;
}

}
