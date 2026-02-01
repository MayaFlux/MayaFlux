#include "InputManager.hpp"

#include "MayaFlux/Registry/BackendRegistry.hpp"
#include "MayaFlux/Registry/Service/InputService.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Nodes/Input/InputNode.hpp"

namespace MayaFlux::Core {

InputManager::InputManager()
#ifdef MAYAFLUX_PLATFORM_MACOS
    : m_registrations(new RegistrationList())
#else
    : m_registrations(std::make_shared<const RegistrationList>())
#endif
{
#ifdef MAYAFLUX_PLATFORM_MACOS
    for (auto& hp : m_hazard_ptrs) {
        hp.store(nullptr);
    }
#endif

    MF_DEBUG(Journal::Component::Core, Journal::Context::Init,
        "InputManager created");
}

InputManager::~InputManager()
{
    if (m_running.load()) {
        stop();
    }

#ifdef MAYAFLUX_PLATFORM_MACOS
    delete m_registrations.load();
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
// Lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void InputManager::start()
{
    if (m_running.load()) {
        MF_WARN(Journal::Component::Core, Journal::Context::Init,
            "InputManager already running");
        return;
    }

    if (!m_input_service) {
        m_input_service = Registry::BackendRegistry::instance()
                              .get_service<Registry::Service::InputService>();

        if (!m_input_service) {
            MF_WARN(Journal::Component::Core, Journal::Context::InputManagement,
                "InputManager requires InputService but service not registered");
            return;
        }
    }

    m_stop_requested.store(false);
    m_running.store(true);
    m_processing_thread = std::thread(&InputManager::processing_loop, this);

    MF_INFO(Journal::Component::Core, Journal::Context::Init,
        "InputManager started");
}

void InputManager::stop()
{
    if (!m_running.load()) {
        return;
    }

    m_stop_requested.store(true);

    m_queue_notify.store(true);
    m_queue_notify.notify_one();

    if (m_processing_thread.joinable()) {
        m_processing_thread.join();
    }

    m_running.store(false);

    MF_INFO(Journal::Component::Core, Journal::Context::Init,
        "InputManager stopped (processed {} events)", m_events_processed.load());
}

// ─────────────────────────────────────────────────────────────────────────────
// Input Enqueueing
// ─────────────────────────────────────────────────────────────────────────────

void InputManager::enqueue(const InputValue& value)
{

    if (!m_queue.push(value)) {
        MF_WARN(Journal::Component::Core, Journal::Context::InputManagement,
            "Input queue full, dropping oldest event");
    }

    m_queue_notify.store(true);
    m_queue_notify.notify_one();
}

void InputManager::enqueue_batch(const std::vector<InputValue>& values)
{
    if (values.empty())
        return;

    bool any_pushed = false;
    for (const auto& value : values) {
        if (m_queue.push(value)) {
            any_pushed = true;
        } else {
            MF_WARN(Journal::Component::Core, Journal::Context::InputManagement,
                "Input queue full during batch, dropping oldest events");
        }
    }

    if (any_pushed) {
        m_queue_notify.store(true);
        m_queue_notify.notify_one();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Node Registration
// ─────────────────────────────────────────────────────────────────────────────

void InputManager::register_node(
    const std::shared_ptr<Nodes::Input::InputNode>& node,
    InputBinding binding)
{
    if (!node)
        return;

    if (binding.hid_vendor_id || binding.hid_product_id) {
        if (!m_input_service) {
            MF_WARN(Journal::Component::Core, Journal::Context::InputManagement,
                "VID/PID binding requires InputService but service not registered");
            return;
        }

        auto devices = m_input_service->get_all_devices();
        auto resolved = resolve_vid_pid(binding, devices);
        if (resolved) {
            binding = *resolved;
        } else {
            MF_WARN(Journal::Component::Core, Journal::Context::InputManagement,
                "No device found for VID/PID");
            return;
        }
    }

    if (binding.device_id != 0) {
        if (!m_input_service) {
            MF_WARN(Journal::Component::Core, Journal::Context::InputManagement,
                "Device ID binding requires InputService but service not registered");
            return;
        }

        m_input_service->open_device(binding.backend, binding.device_id);
    }

    {
        std::lock_guard lock(m_registry_mutex);
#ifdef MAYAFLUX_PLATFORM_MACOS
        auto* old_list = m_registrations.load();
        auto* new_list = new RegistrationList(*old_list);
        new_list->push_back({ .node = node, .binding = binding });
        m_registrations.store(new_list);
        retire_list(old_list);
#else
        auto current_list = m_registrations.load();
        auto new_list = std::make_shared<RegistrationList>(*current_list);
        new_list->push_back({ .node = node, .binding = binding });
        m_registrations.store(new_list);
#endif
        m_tracked_nodes.push_back(node);
    }

    MF_DEBUG(Journal::Component::Core, Journal::Context::InputManagement,
        "Registered InputNode for backend {} device {}",
        static_cast<int>(binding.backend), binding.device_id);
}

std::optional<InputBinding> InputManager::resolve_vid_pid(
    const InputBinding& binding,
    const std::vector<InputDeviceInfo>& devices) const
{
    for (const auto& dev : devices) {
        if (dev.backend_type != binding.backend)
            continue;

        bool vid_match = !binding.hid_vendor_id || (*binding.hid_vendor_id == dev.vendor_id);
        bool pid_match = !binding.hid_product_id || (*binding.hid_product_id == dev.product_id);

        if (vid_match && pid_match) {
            InputBinding resolved = binding;
            resolved.device_id = dev.id;
            resolved.hid_vendor_id.reset();
            resolved.hid_product_id.reset();
            return resolved;
        }
    }

    return std::nullopt;
}

void InputManager::unregister_node(const std::shared_ptr<Nodes::Input::InputNode>& node)
{
    if (!node)
        return;

    {
        std::lock_guard lock(m_registry_mutex);

#ifdef MAYAFLUX_PLATFORM_MACOS
        auto* old_list = m_registrations.load();
        auto* new_list = new RegistrationList(*old_list);

        new_list->erase(
            std::remove_if(new_list->begin(), new_list->end(),
                [&node](const NodeRegistration& reg) {
                    auto locked = reg.node.lock();
                    return !locked || locked == node;
                }),
            new_list->end());

        m_registrations.store(new_list);
        retire_list(old_list);
#else
        auto current_list = m_registrations.load();
        auto new_list = std::make_shared<RegistrationList>(*current_list);

        new_list->erase(
            std::remove_if(new_list->begin(), new_list->end(),
                [&node](const NodeRegistration& reg) {
                    auto locked = reg.node.lock();
                    return !locked || locked == node;
                }),
            new_list->end());

        m_registrations.store(new_list);
#endif
        m_tracked_nodes.erase(
            std::remove(m_tracked_nodes.begin(), m_tracked_nodes.end(), node),
            m_tracked_nodes.end());
    }

    MF_DEBUG(Journal::Component::Core, Journal::Context::InputManagement,
        "Unregistered InputNode");
}

void InputManager::unregister_all_nodes()
{
    std::lock_guard lock(m_registry_mutex);

#ifdef MAYAFLUX_PLATFORM_MACOS
    auto* old_list = m_registrations.load();
    m_registrations.store(new RegistrationList());
    retire_list(old_list);
#else
    m_registrations.store(std::make_shared<const RegistrationList>());
#endif

    m_tracked_nodes.clear();

    MF_DEBUG(Journal::Component::Core, Journal::Context::InputManagement,
        "Unregistered all InputNodes (Registry swapped to empty)");
}

size_t InputManager::get_registered_node_count() const
{
#ifdef MAYAFLUX_PLATFORM_MACOS
    // Brief hazard pointer acquisition for count
    size_t slot = m_hazard_counter.fetch_add(1) % MAX_READERS;
    const RegistrationList* current;
    do {
        current = m_registrations.load();
        m_hazard_ptrs[slot].store(current);
    } while (current != m_registrations.load());

    size_t count = current->size();
    m_hazard_ptrs[slot].store(nullptr);
    return count;
#else
    return m_registrations.load()->size();
#endif
}

size_t InputManager::get_queue_depth() const
{
    return m_queue.snapshot().size();
}

// ─────────────────────────────────────────────────────────────────────────────
// Processing Thread
// ─────────────────────────────────────────────────────────────────────────────

void InputManager::processing_loop()
{
    MF_DEBUG(Journal::Component::Core, Journal::Context::AsyncIO,
        "Processing thread started");

    while (true) {
        InputValue value;

        while (m_queue.pop(value)) {
            dispatch_to_nodes(value);
            m_events_processed.fetch_add(1, std::memory_order_relaxed);
        }

        if (m_stop_requested.load()) {
            break;
        }

        m_queue_notify.wait(false);
        m_queue_notify.store(false);
    }

    MF_DEBUG(Journal::Component::Core, Journal::Context::AsyncIO,
        "Processing thread exiting");
}

void InputManager::dispatch_to_nodes(const InputValue& value)
{
#ifdef MAYAFLUX_PLATFORM_MACOS
    // Acquire hazard pointer slot
    size_t slot = m_hazard_counter.fetch_add(1) % MAX_READERS;

    const RegistrationList* current_regs;
    do {
        current_regs = m_registrations.load();
        m_hazard_ptrs[slot].store(current_regs);
    } while (current_regs != m_registrations.load());

    // Safe to use current_regs now
    for (const auto& reg : *current_regs) {
        auto node = reg.node.lock();
        if (!node)
            continue;

        if (matches_binding(value, reg.binding)) {
            node->process_input(value);
        }
    }

    // Release hazard pointer
    m_hazard_ptrs[slot].store(nullptr);
#else
    auto current_regs = m_registrations.load();

    for (const auto& reg : *current_regs) {
        auto node = reg.node.lock();
        if (!node)
            continue;

        if (matches_binding(value, reg.binding)) {
            node->process_input(value);
        }
    }
#endif
}

bool InputManager::matches_binding(const InputValue& value, const InputBinding& binding) const
{
    if (binding.backend != value.source_type) {
        return false;
    }

    if (binding.device_id != 0 && binding.device_id != value.device_id) {
        return false;
    }

    switch (binding.backend) {
    case InputType::MIDI:
        if (value.type == InputValue::Type::MIDI) {
            const auto& midi = value.as_midi();

            if (binding.midi_channel && *binding.midi_channel != midi.channel()) {
                return false;
            }

            if (binding.midi_message_type && *binding.midi_message_type != midi.type()) {
                return false;
            }
            if (binding.midi_cc_number && midi.type() == 0xB0) {
                if (*binding.midi_cc_number != midi.data1) {
                    return false;
                }
            }
        }
        break;

    case InputType::OSC:
        if (value.type == InputValue::Type::OSC && binding.osc_address_pattern) {
            const auto& osc = value.as_osc();
            if (!osc.address.starts_with(*binding.osc_address_pattern)) {
                return false;
            }
        }
        break;

    default:
        // HID, Serial: no additional filters beyond device_id
        break;
    }

    return true;
}

#ifdef MAYAFLUX_PLATFORM_MACOS
void InputManager::retire_list(const RegistrationList* list)
{
    // Check if any reader is using this list
    for (const auto& hp : m_hazard_ptrs) {
        if (hp.load() == list) {
            // Still in use - in production you'd add to retirement queue
            // For now, leak it (registrations are infrequent)
            // TODO: Implement proper deferred reclamation queue
            return;
        }
    }
    delete list;
}
#endif

} // namespace MayaFlux::Core
