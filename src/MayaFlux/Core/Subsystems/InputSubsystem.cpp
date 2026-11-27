#include "InputSubsystem.hpp"

#include "MayaFlux/Registry/BackendRegistry.hpp"
#include "MayaFlux/Registry/Service/InputService.hpp"
#include "MayaFlux/Registry/Service/NetworkService.hpp"

#include "MayaFlux/Core/Backends/Input/HIDBackend.hpp"
#include "MayaFlux/Core/Backends/Input/HIDSession.hpp"
#include "MayaFlux/Core/Backends/Input/MIDIBackend.hpp"
#include "MayaFlux/Core/Backends/Input/SerialInputBackend.hpp"
#include "MayaFlux/Core/Backends/Input/TabletBackend.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Core {

InputSubsystem::InputSubsystem(GlobalInputConfig& config)
    : m_config(config)
    , m_tokens {
        .Buffer = Buffers::ProcessingToken::INPUT_BACKEND,
        .Node = Nodes::ProcessingToken::EVENT_RATE,
        .Task = Vruta::ProcessingToken::EVENT_DRIVEN
    }
{
}

InputSubsystem::~InputSubsystem()
{
    shutdown();
}

void InputSubsystem::register_callbacks()
{
    // Input subsystem doesn't register timing callbacks like audio/graphics.
    // Backends push to InputManager's queue, which has its own thread.
}

void InputSubsystem::initialize(SubsystemProcessingHandle& handle)
{
    MF_INFO(Journal::Component::Core, Journal::Context::InputSubsystem,
        "Initializing Input Subsystem...");

    m_handle = &handle;

    if (m_config.hid.enabled || m_config.tablet.enabled) {
        m_hid_session = std::make_shared<HIDSession>();
    }

    if (m_config.hid.enabled) {
        initialize_hid_backend();
    }
    if (m_config.midi.enabled) {
        initialize_midi_backend();
    }
    if (m_config.osc.enabled) {
        m_handle->inputs.setup_osc_bridge(m_config.osc);
    }
    if (m_config.serial.enabled) {
        initialize_serial_backend();
    }

    if (m_config.tablet.enabled) {
        initialize_tablet_backend();
    }

    register_backend_service();

    m_ready.store(true);

    MF_INFO(Journal::Component::Core, Journal::Context::InputSubsystem,
        "Input Subsystem initialized with {} backend(s)", m_backends.size());
}

void InputSubsystem::register_backend_service()
{
    auto& registry = Registry::BackendRegistry::instance();

    auto input_service = std::make_shared<Registry::Service::InputService>();

    input_service->get_all_devices = [this]() {
        return get_all_devices();
    };

    input_service->open_device = [this](InputType type, uint32_t id) {
        return open_device(type, id);
    };

    input_service->close_device = [this](InputType type, uint32_t id) {
        close_device(type, id);
    };

    input_service->write = [this](InputType type, uint32_t id, std::span<const uint8_t> data) {
        return write(type, id, data);
    };

    input_service->send_feature_report = [this](InputType type, uint32_t id, std::span<const uint8_t> data) {
        return send_feature_report(type, id, data);
    };

    input_service->add_serial_reader = [this](uint32_t id, const SerialSpec& spec,
                                           std::function<void(const InputValue&)> callback) -> uint64_t {
        std::shared_lock lock(m_backends_mutex);
        const auto it = m_backends.find(InputType::SERIAL);
        auto* serial = it != m_backends.end() ? dynamic_cast<SerialInputBackend*>(it->second.get()) : nullptr;
        return serial ? serial->add_reader(id, spec, std::move(callback)) : 0;
    };

    input_service->remove_serial_reader = [this](uint64_t token) {
        std::shared_lock lock(m_backends_mutex);
        const auto it = m_backends.find(InputType::SERIAL);
        if (auto* serial = it != m_backends.end() ? dynamic_cast<SerialInputBackend*>(it->second.get()) : nullptr) {
            serial->remove_reader(token);
        }
    };

    m_input_service = input_service;

    registry.register_service<Registry::Service::InputService>(
        [input_service]() -> void* {
            return input_service.get();
        });
}

void InputSubsystem::start()
{
    if (!m_ready.load()) {
        MF_ERROR(Journal::Component::Core, Journal::Context::InputSubsystem,
            "Cannot start InputSubsystem: not initialized");
        return;
    }

    if (m_running.load()) {
        return;
    }

    m_handle->inputs.start();

    {
        std::shared_lock lock(m_backends_mutex);
        for (auto& [type, backend] : m_backends) {
            backend->start();
        }
    }

    m_running.store(true);

    MF_INFO(Journal::Component::Core, Journal::Context::InputSubsystem,
        "Input Subsystem started");
}

void InputSubsystem::pause()
{
    if (!m_running.load())
        return;

    for (auto* backend : get_backends()) {
        backend->stop();
    }

    m_running.store(false);
}

void InputSubsystem::resume()
{
    if (m_running.load())
        return;

    {
        std::shared_lock lock(m_backends_mutex);
        for (auto& [type, backend] : m_backends) {
            backend->start();
        }
    }

    m_running.store(true);
}

void InputSubsystem::stop()
{
    if (!m_running.load())
        return;

    for (auto* backend : get_backends()) {
        backend->stop();
    }

    m_handle->inputs.stop();

    m_running.store(false);

    MF_INFO(Journal::Component::Core, Journal::Context::InputSubsystem,
        "Input Subsystem stopped");
}

void InputSubsystem::shutdown()
{
    if (!m_ready.load())
        return;

    stop();

    {
        std::unique_lock lock(m_backends_mutex);
        for (auto& [type, backend] : m_backends) {
            backend->shutdown();
        }
        m_backends.clear();
        m_hid_auto_open_suppressed.clear();
    }
    m_hid_session.reset();

    m_handle->inputs.unregister();

    auto& registry = Registry::BackendRegistry::instance();
    registry.unregister_service<Registry::Service::InputService>();
    m_input_service.reset();

    m_ready.store(false);

    MF_INFO(Journal::Component::Core, Journal::Context::InputSubsystem,
        "Input Subsystem shutdown complete");
}

void InputSubsystem::wait_until_running()
{
    while (!m_running.load(std::memory_order_acquire))
        std::this_thread::yield();
}

// ─────────────────────────────────────────────────────────────────────────────
// Backend Management
// ─────────────────────────────────────────────────────────────────────────────

bool InputSubsystem::add_backend(std::unique_ptr<IInputBackend> backend)
{
    if (!backend)
        return false;

    InputType type = backend->get_type();

    std::unique_lock lock(m_backends_mutex);

    if (m_backends.find(type) != m_backends.end()) {
        MF_WARN(Journal::Component::Core, Journal::Context::InputSubsystem,
            "Backend type {} already registered", static_cast<int>(type));
        return false;
    }

    if (!backend->initialize()) {
        MF_ERROR(Journal::Component::Core, Journal::Context::InputSubsystem,
            "Failed to initialize backend: {}", backend->get_name());
        return false;
    }

    wire_backend_to_manager(backend.get());

    m_backends[type] = std::move(backend);

    MF_INFO(Journal::Component::Core, Journal::Context::InputSubsystem,
        "Added input backend: {}", m_backends[type]->get_name());

    return true;
}

IInputBackend* InputSubsystem::get_backend(InputType type) const
{
    std::shared_lock lock(m_backends_mutex);
    auto it = m_backends.find(type);
    return (it != m_backends.end()) ? it->second.get() : nullptr;
}

std::vector<IInputBackend*> InputSubsystem::get_backends() const
{
    std::shared_lock lock(m_backends_mutex);
    std::vector<IInputBackend*> result;
    result.reserve(m_backends.size());
    for (const auto& [type, backend] : m_backends) {
        result.push_back(backend.get());
    }
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// Device Management
// ─────────────────────────────────────────────────────────────────────────────

std::vector<InputDeviceInfo> InputSubsystem::get_all_devices() const
{
    std::shared_lock lock(m_backends_mutex);
    std::vector<InputDeviceInfo> result;
    for (const auto& [type, backend] : m_backends) {
        auto devices = backend->get_devices();
        result.insert(result.end(), devices.begin(), devices.end());
    }
    return result;
}

bool InputSubsystem::open_device(InputType backend_type, uint32_t device_id)
{
    if (backend_type == InputType::HID) {
        std::unique_lock lock(m_backends_mutex);
        const auto it = m_backends.find(backend_type);
        if (it == m_backends.end()) {
            MF_ERROR(Journal::Component::Core, Journal::Context::InputSubsystem,
                "Backend not found for device open request");
            return false;
        }
        const bool opened = it->second->open_device(device_id);
        if (opened) {
            m_hid_auto_open_suppressed.erase(device_id);
        }
        return opened;
    }

    std::shared_lock lock(m_backends_mutex);
    auto it = m_backends.find(backend_type);
    if (it == m_backends.end()) {
        MF_ERROR(Journal::Component::Core, Journal::Context::InputSubsystem,
            "Backend not found for device open request");
        return false;
    }
    return it->second->open_device(device_id);
}

void InputSubsystem::close_device(InputType backend_type, uint32_t device_id)
{
    if (backend_type == InputType::HID) {
        std::unique_lock lock(m_backends_mutex);
        const auto it = m_backends.find(backend_type);
        if (it != m_backends.end()) {
            m_hid_auto_open_suppressed.insert(device_id);
            it->second->close_device(device_id);
        }
        return;
    }

    std::shared_lock lock(m_backends_mutex);
    auto it = m_backends.find(backend_type);
    if (it != m_backends.end()) {
        it->second->close_device(device_id);
    }
}

int InputSubsystem::write(InputType backend_type, uint32_t device_id, std::span<const uint8_t> data)
{
    std::shared_lock lock(m_backends_mutex);
    auto it = m_backends.find(backend_type);
    return it == m_backends.end() ? -1 : it->second->write(device_id, data);
}

int InputSubsystem::send_feature_report(InputType backend_type, uint32_t device_id, std::span<const uint8_t> data)
{
    std::shared_lock lock(m_backends_mutex);
    auto it = m_backends.find(backend_type);
    return it == m_backends.end() ? -1 : it->second->send_feature_report(device_id, data);
}

// ─────────────────────────────────────────────────────────────────────────────
// Private: Backend Initialization
// ─────────────────────────────────────────────────────────────────────────────

void InputSubsystem::wire_backend_to_manager(IInputBackend* backend)
{
    backend->set_input_callback([this](const InputValue& value) {
        m_handle->inputs.enqueue_input(value);
    });

    backend->set_device_callback([this, backend](const InputDeviceInfo& info, bool connected) {
        MF_INFO(Journal::Component::Core, Journal::Context::InputSubsystem,
            "Device {}: {} ({})",
            connected ? "connected" : "disconnected",
            info.name, static_cast<int>(info.backend_type));

        if (connected && backend->get_type() == InputType::HID && m_config.hid.auto_open) {
            std::shared_lock lock(m_backends_mutex);
            if (!m_hid_auto_open_suppressed.contains(info.id)) {
                backend->open_device(info.id);
            }
        }

        m_handle->inputs.on_device_change(info, connected);
    });
}

void InputSubsystem::initialize_hid_backend()
{
    HIDBackend::Config hid_config;
    hid_config.filters = m_config.hid.filters;
    hid_config.read_buffer_size = m_config.hid.read_buffer_size;
    hid_config.poll_timeout_ms = m_config.hid.poll_timeout_ms;
    hid_config.auto_reconnect = m_config.hid.auto_reconnect;
    hid_config.reconnect_interval_ms = m_config.hid.reconnect_interval_ms;
    hid_config.session = m_hid_session;

    auto hid = std::make_unique<HIDBackend>(hid_config);

    if (add_backend(std::move(hid))) {
        if (m_config.hid.auto_open) {
            auto* backend = dynamic_cast<HIDBackend*>(get_backend(InputType::HID));
            for (const auto& dev : backend->get_devices()) {
                backend->open_device(dev.id);
            }
        }
    }
}

void InputSubsystem::initialize_midi_backend()
{
    MIDIBackend::Config midi_config;
    midi_config.input_port_filters = m_config.midi.input_port_filters;
    midi_config.auto_open_inputs = m_config.midi.auto_open_inputs;
    midi_config.virtual_port_name = m_config.midi.virtual_port_name;

#if !defined(MAYAFLUX_PLATFORM_WINDOWS)
    midi_config.output_port_filters = m_config.midi.output_port_filters;
    midi_config.auto_open_outputs = m_config.midi.auto_open_outputs;
    midi_config.enable_virtual_port = m_config.midi.enable_virtual_port;
#endif

    auto midi = std::make_unique<MIDIBackend>(midi_config);

    if (add_backend(std::move(midi))) {
        if (m_config.midi.auto_open_inputs) {
            auto* backend = dynamic_cast<MIDIBackend*>(get_backend(InputType::MIDI));
            for (const auto& dev : backend->get_devices()) {
                if (dev.is_input) {
                    backend->open_device(dev.id);
                }
            }
        }
    }
}
void InputSubsystem::initialize_serial_backend()
{
    auto* network = Registry::BackendRegistry::instance()
                        .get_service<Registry::Service::NetworkService>();

    if (!network) {
        MF_WARN(Journal::Component::Core, Journal::Context::InputSubsystem,
            "Serial input requested but NetworkService not available");
        return;
    }

    SerialInputBackend::Config serial_config;
    serial_config.ports = m_config.serial.ports;
    serial_config.enumerate_ports = m_config.serial.enumerate_ports;
    serial_config.detected_line = m_config.serial.detected_line;
    serial_config.detected_protocol = m_config.serial.detected_protocol;
    serial_config.network = network;

    if (!add_backend(std::make_unique<SerialInputBackend>(std::move(serial_config)))) {
        return;
    }

    auto* backend = dynamic_cast<SerialInputBackend*>(get_backend(InputType::SERIAL));
    const auto devices = backend->get_devices();
    for (const auto& port : m_config.serial.ports) {
        for (const auto& device : devices) {
            if (device.port_name == port.port_name) {
                backend->open_device(device.id);
            }
        }
    }
}

void InputSubsystem::initialize_tablet_backend()
{
    TabletBackend::Config tablet_config;
    tablet_config.read_buffer_size = m_config.tablet.read_buffer_size;
    tablet_config.poll_timeout_ms = m_config.tablet.poll_timeout_ms;
    tablet_config.split_eraser = m_config.tablet.split_eraser;
    tablet_config.auto_reconnect = m_config.tablet.auto_reconnect;
    tablet_config.reconnect_interval_ms = m_config.tablet.reconnect_interval_ms;
    tablet_config.session = m_hid_session;

    add_backend(std::make_unique<TabletBackend>(tablet_config));
}

[[nodiscard]] std::vector<InputDeviceInfo> InputSubsystem::get_hid_devices() const
{
    std::shared_lock lock(m_backends_mutex);
    auto it = m_backends.find(InputType::HID);
    return (it != m_backends.end()) ? it->second->get_devices() : std::vector<InputDeviceInfo> {};
}

[[nodiscard]] std::vector<InputDeviceInfo> InputSubsystem::get_midi_devices() const
{
    std::shared_lock lock(m_backends_mutex);
    auto it = m_backends.find(InputType::MIDI);
    return (it != m_backends.end()) ? it->second->get_devices() : std::vector<InputDeviceInfo> {};
}

[[nodiscard]] std::vector<InputDeviceInfo> InputSubsystem::get_serial_devices() const
{
    std::shared_lock lock(m_backends_mutex);
    auto it = m_backends.find(InputType::SERIAL);
    return (it != m_backends.end()) ? it->second->get_devices() : std::vector<InputDeviceInfo> {};
}

[[nodiscard]] std::vector<InputDeviceInfo> InputSubsystem::get_tablet_devices() const
{
    std::shared_lock lock(m_backends_mutex);
    auto it = m_backends.find(InputType::TABLET);
    return (it != m_backends.end()) ? it->second->get_devices() : std::vector<InputDeviceInfo> {};
}

[[nodiscard]] std::optional<InputDeviceInfo> InputSubsystem::get_device_info(
    InputType backend_type,
    uint32_t device_id) const
{
    std::shared_lock lock(m_backends_mutex);
    auto it = m_backends.find(backend_type);
    if (it == m_backends.end())
        return std::nullopt;

    auto devices = it->second->get_devices();
    for (const auto& dev : devices) {
        if (dev.id == device_id) {
            return dev;
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<InputDeviceInfo> InputSubsystem::find_hid_device(
    uint16_t vendor_id,
    uint16_t product_id) const
{
    auto devices = get_hid_devices();
    for (const auto& dev : devices) {
        if (dev.vendor_id == vendor_id && dev.product_id == product_id) {
            return dev;
        }
    }
    return std::nullopt;
}

} // namespace MayaFlux::Core
