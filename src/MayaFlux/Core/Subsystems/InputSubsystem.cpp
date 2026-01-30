#include "InputSubsystem.hpp"

#include "MayaFlux/Core/Backends/Input/HIDBackend.hpp"
#include "MayaFlux/Core/Input/InputManager.hpp"
#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Core {

InputSubsystem::InputSubsystem(GlobalInputConfig& config)
    : m_config(config)
    , m_tokens {
        .Buffer = Buffers::ProcessingToken::INPUT_BACKEND,
        .Node = Nodes::ProcessingToken::EVENT_RATE,
        .Task = Vruta::ProcessingToken::EVENT_DRIVEN
    }
    , m_input_manager(std::make_unique<InputManager>())
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

    if (m_config.hid.enabled) {
        initialize_hid_backend();
    }
    if (m_config.midi.enabled) {
        initialize_midi_backend();
    }
    if (m_config.osc.enabled) {
        initialize_osc_backend();
    }
    if (m_config.serial.enabled) {
        initialize_serial_backend();
    }

    m_ready.store(true);

    MF_INFO(Journal::Component::Core, Journal::Context::InputSubsystem,
        "Input Subsystem initialized with {} backend(s)", m_backends.size());
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

    m_input_manager->start();

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

    {
        std::shared_lock lock(m_backends_mutex);
        for (auto& [type, backend] : m_backends) {
            backend->stop();
        }
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

    {
        std::shared_lock lock(m_backends_mutex);
        for (auto& [type, backend] : m_backends) {
            backend->stop();
        }
    }

    m_input_manager->stop();

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
    }

    m_input_manager->unregister_all_nodes();

    m_ready.store(false);

    MF_INFO(Journal::Component::Core, Journal::Context::InputSubsystem,
        "Input Subsystem shutdown complete");
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
    std::shared_lock lock(m_backends_mutex);
    auto it = m_backends.find(backend_type);
    if (it != m_backends.end()) {
        it->second->close_device(device_id);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Private: Backend Initialization
// ─────────────────────────────────────────────────────────────────────────────

void InputSubsystem::wire_backend_to_manager(IInputBackend* backend)
{
    backend->set_input_callback([this](const InputValue& value) {
        m_input_manager->enqueue(value);
    });

    backend->set_device_callback([](const InputDeviceInfo& info, bool connected) {
        MF_INFO(Journal::Component::Core, Journal::Context::InputSubsystem,
            "Device {}: {} ({})",
            connected ? "connected" : "disconnected",
            info.name, static_cast<int>(info.backend_type));
    });
}

void InputSubsystem::initialize_hid_backend()
{
#ifdef MAYAFLUX_HID_BACKEND
    HIDBackend::Config hid_config;
    hid_config.filters = m_config.hid.filters;
    hid_config.read_buffer_size = m_config.hid.read_buffer_size;
    hid_config.poll_timeout_ms = m_config.hid.poll_timeout_ms;
    hid_config.auto_reconnect = m_config.hid.auto_reconnect;
    hid_config.reconnect_interval_ms = m_config.hid.reconnect_interval_ms;

    auto hid = std::make_unique<HIDBackend>(hid_config);

    if (add_backend(std::move(hid))) {
        if (m_config.hid.auto_open) {
            auto* backend = dynamic_cast<HIDBackend*>(get_backend(InputType::HID));
            for (const auto& dev : backend->get_devices()) {
                backend->open_device(dev.id);
            }
        }
    }
#else
    MF_WARN(Journal::Component::Core, Journal::Context::InputSubsystem,
        "HID backend requested but HIDAPI not available at build time");
#endif
}

void InputSubsystem::initialize_midi_backend()
{
    MF_WARN(Journal::Component::Core, Journal::Context::InputSubsystem,
        "MIDI backend not yet implemented");
}

void InputSubsystem::initialize_osc_backend()
{
    MF_WARN(Journal::Component::Core, Journal::Context::InputSubsystem,
        "OSC backend not yet implemented");
}

void InputSubsystem::initialize_serial_backend()
{
    MF_WARN(Journal::Component::Core, Journal::Context::InputSubsystem,
        "Serial backend not yet implemented");
}

} // namespace MayaFlux::Core
