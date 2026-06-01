#include "CoreMidiBackend.hpp"

#ifdef MAYAFLUX_PLATFORM_MACOS

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Core {

namespace {
    std::string cfstring_to_string(CFStringRef str)
    {
        if (!str)
            return {};

        char buffer[512];

        if (CFStringGetCString(
                str,
                buffer,
                sizeof(buffer),
                kCFStringEncodingUTF8)) {
            return buffer;
        }

        return {};
    }

    constexpr auto C = Journal::Component::Core;
    constexpr auto X = Journal::Context::InputBackend;
}

CoreMidiBackend::CoreMidiBackend()
    : CoreMidiBackend(Config {})
{
}

CoreMidiBackend::CoreMidiBackend(Config config)
    : m_config(std::move(config))
{
}

CoreMidiBackend::~CoreMidiBackend()
{
    if (m_initialized.load()) {
        shutdown();
    }
}

bool CoreMidiBackend::initialize()
{
    if (m_initialized.load()) {
        MF_WARN(C, X,
            "CoreMidiBackend already initialized");
        return true;
    }

    OSStatus status = MIDIClientCreate(
        CFSTR("MayaFlux"),
        &CoreMidiBackend::midi_notify_callback,
        this,
        &m_client);

    if (status != noErr) {
        MF_ERROR(C, X,
            "Failed to create CoreMIDI client (status={})",
            static_cast<int>(status));
        return false;
    }

    m_initialized.store(true);

    refresh_devices();

    create_virtual_port_if_enabled();

    MF_INFO(C, X,
        "CoreMidiBackend initialized with {} port(s)",
        m_enumerated_devices.size());

    return true;
}

void CoreMidiBackend::start()
{
    if (!m_initialized.load()) {
        MF_ERROR(C, X,
            "Cannot start CoreMidiBackend: not initialized");
        return;
    }

    if (m_running.load()) {
        MF_WARN(C, X,
            "CoreMidiBackend already running");
        return;
    }

    if (m_config.auto_open_inputs) {
        std::vector<uint32_t> to_open;

        {
            std::lock_guard lock(m_devices_mutex);

            for (const auto& [id, info] : m_enumerated_devices) {
                to_open.push_back(id);
            }
        }

        for (uint32_t id : to_open) {
            open_device(id);
        }
    }

    m_running.store(true);

    MF_INFO(C, X,
        "CoreMidiBackend started with {} open port(s)",
        get_open_devices().size());
}

void CoreMidiBackend::stop()
{
    if (!m_running.load())
        return;

    std::vector<uint32_t> to_close;

    {
        std::lock_guard lock(m_devices_mutex);

        for (const auto& [id, state] : m_open_devices) {
            to_close.push_back(id);
        }
    }

    for (uint32_t id : to_close) {
        close_device(id);
    }

    m_running.store(false);

    MF_INFO(C, X, "CoreMidiBackend stopped");
}

void CoreMidiBackend::shutdown()
{
    if (!m_initialized.load())
        return;

    stop();

    if (m_virtual_destination) {
        MIDIEndpointDispose(m_virtual_destination);
        m_virtual_destination = 0;
    }

    {
        std::lock_guard lock(m_devices_mutex);
        m_open_devices.clear();
        m_enumerated_devices.clear();
    }

    if (m_client) {
        MIDIClientDispose(m_client);
        m_client = 0;
    }

    m_initialized.store(false);

    MF_INFO(C, X, "CoreMidiBackend shutdown complete");
}

std::vector<InputDeviceInfo> CoreMidiBackend::get_devices() const
{
    std::lock_guard lock(m_devices_mutex);

    std::vector<InputDeviceInfo> result;
    result.reserve(m_enumerated_devices.size());

    for (const auto& [id, info] : m_enumerated_devices) {
        result.push_back(info);
    }

    return result;
}

size_t CoreMidiBackend::refresh_devices()
{
    std::vector<InputDeviceInfo> newly_added;
    std::vector<InputDeviceInfo> removed_devices;

    const ItemCount count = MIDIGetNumberOfSources();

    std::unordered_set<MIDIUniqueID> seen;

    {
        std::lock_guard lock(m_devices_mutex);

        for (ItemCount i = 0; i < count; ++i) {
            MIDIEndpointRef endpoint = MIDIGetSource(i);
            if (!endpoint)
                continue;

            MIDIUniqueID unique_id = 0;
            if (MIDIObjectGetIntegerProperty(endpoint, kMIDIPropertyUniqueID, &unique_id) != noErr)
                continue;

            seen.insert(unique_id);

            CFStringRef name_ref = nullptr;
            MIDIObjectGetStringProperty(endpoint, kMIDIPropertyName, &name_ref);
            std::string cname = cfstring_to_string(name_ref);

            if (name_ref)
                CFRelease(name_ref);

            if (!port_matches_filter(cname))
                continue;

            uint32_t dev_id = find_or_assign_device_id(unique_id);
            bool is_new = m_enumerated_devices.find(dev_id) == m_enumerated_devices.end();

            MIDIPortInfo info {};
            info.id = dev_id;
            info.name = cname;
            info.unique_id = unique_id;
            info.endpoint = endpoint;
            info.backend_type = InputType::MIDI;
            info.is_connected = true;
            info.is_input = true;
            info.is_output = false;
            info.port_number = static_cast<uint8_t>(i);

            m_enumerated_devices[dev_id] = info;
            if (is_new)
                newly_added.push_back(info);
        }

        for (auto it = m_enumerated_devices.begin(); it != m_enumerated_devices.end();) {
            if (!seen.contains(it->second.unique_id)) {
                removed_devices.push_back(it->second);
                uint32_t removed_id = it->first;
                ++it;

                close_device(removed_id);

                it = m_enumerated_devices.erase(
                    m_enumerated_devices.find(removed_id));
                it = m_enumerated_devices.erase(it);
            } else {
                ++it;
            }
        }
    }

    for (const auto& info : newly_added) {
        MF_INFO(C, X, "MIDI port found: '{}'", info.name);
        notify_device_change(info, true);
    }

    for (const auto& info : removed_devices) {
        MF_INFO(C, X, "MIDI port removed: '{}'", info.name);
        notify_device_change(info, false);
    }

    size_t device_count {};
    {
        std::lock_guard lock(m_devices_mutex);
        device_count = m_enumerated_devices.size();
    }

    return device_count;
}

bool CoreMidiBackend::open_device(uint32_t device_id)
{
    std::lock_guard lock(m_devices_mutex);

    if (m_open_devices.find(device_id) != m_open_devices.end()) {
        return true;
    }

    auto it = m_enumerated_devices.find(device_id);

    if (it == m_enumerated_devices.end()) {
        return false;
    }

    auto state = std::make_shared<MIDIPortState>();

    state->info = it->second;
    state->device_id = device_id;

    {
        std::lock_guard cb_lock(m_callback_mutex);
        state->input_callback = m_input_callback;
    }

    OSStatus status = MIDIInputPortCreate(
        m_client,
        CFSTR("MayaFlux Input"),
        &CoreMidiBackend::midi_read_callback,
        state.get(),
        &state->input_port);

    if (status != noErr) {
        return false;
    }

    status = MIDIPortConnectSource(
        state->input_port,
        state->info.endpoint,
        state.get());

    if (status != noErr) {
        MIDIPortDispose(state->input_port);
        state->input_port = 0;
        return false;
    }

    state->active.store(true);

    m_open_devices.insert_or_assign(device_id, state);

    MF_INFO(C, X,
        "Opened MIDI port {}: '{}'",
        device_id,
        state->info.name);

    return true;
}

void CoreMidiBackend::close_device(uint32_t device_id)
{
    std::shared_ptr<MIDIPortState> state;

    {
        std::lock_guard lock(m_devices_mutex);

        auto it = m_open_devices.find(device_id);

        if (it == m_open_devices.end()) {
            return;
        }

        state = std::move(it->second);

        m_open_devices.erase(it);
    }

    state->active.store(false);

    if (state->input_port) {

        MIDIPortDisconnectSource(
            state->input_port,
            state->info.endpoint);

        MIDIPortDispose(state->input_port);

        state->input_port = 0;
    }

    MF_INFO(C, X,
        "Closed MIDI port {}: '{}'",
        device_id,
        state->info.name);
}

bool CoreMidiBackend::is_device_open(uint32_t device_id) const
{
    std::lock_guard lock(m_devices_mutex);

    return m_open_devices.find(device_id)
        != m_open_devices.end();
}

std::vector<uint32_t> CoreMidiBackend::get_open_devices() const
{
    std::lock_guard lock(m_devices_mutex);

    std::vector<uint32_t> result;
    result.reserve(m_open_devices.size());

    for (const auto& [id, state] : m_open_devices) {
        result.push_back(id);
    }

    return result;
}

void CoreMidiBackend::set_input_callback(InputCallback callback)
{
    std::lock_guard lock(m_callback_mutex);

    m_input_callback = std::move(callback);
}

void CoreMidiBackend::set_device_callback(DeviceCallback callback)
{
    std::lock_guard lock(m_callback_mutex);
    m_device_callback = std::move(callback);
}

bool CoreMidiBackend::port_matches_filter(const std::string& port_name) const
{
    if (m_config.input_port_filters.empty())
        return true;

    return std::ranges::any_of(
        m_config.input_port_filters,
        [&port_name](const std::string& filter) {
            return port_name.find(filter) != std::string::npos;
        });
}

uint32_t CoreMidiBackend::find_or_assign_device_id(
    MIDIUniqueID unique_id)
{
    for (const auto& [id, info] : m_enumerated_devices) {
        if (info.unique_id == unique_id)
            return id;
    }

    return m_next_device_id++;
}

void CoreMidiBackend::create_virtual_port_if_enabled()
{
    if (!m_config.enable_virtual_port)
        return;

    if (m_virtual_destination)
        return;

    CFStringRef name = CFStringCreateWithCString(
        nullptr,
        m_config.virtual_port_name.c_str(),
        kCFStringEncodingUTF8);

    if (!name) {
        MF_ERROR(C, X,
            "Failed to create CoreFoundation string for virtual MIDI destination");
        return;
    }

    OSStatus status = MIDIDestinationCreate(
        m_client,
        name,
        &CoreMidiBackend::virtual_destination_callback,
        this,
        &m_virtual_destination);

    CFRelease(name);

    if (status != noErr) {
        MF_ERROR(C, X,
            "Failed to create virtual MIDI destination '{}' (status={})",
            m_config.virtual_port_name,
            static_cast<int>(status));
        return;
    }

    MF_INFO(C, X,
        "Created virtual MIDI destination '{}'",
        m_config.virtual_port_name);
}

void CoreMidiBackend::midi_read_callback(
    const MIDIPacketList* packet_list,
    void* read_proc_ref_con,
    void* /*src_conn_ref_con*/)
{
    auto* state = static_cast<MIDIPortState*>(read_proc_ref_con);

    if (!state)
        return;

    if (!state->active.load())
        return;

    if (!state->input_callback)
        return;

    const MIDIPacket* packet = &packet_list->packet[0];

    for (UInt32 i = 0; i < packet_list->numPackets; ++i) {
        if (packet->length == 0) {
            packet = MIDIPacketNext(packet);
            continue;
        }

        if (packet->length > 3) {

            std::vector<uint8_t> bytes(
                packet->data,
                packet->data + packet->length);

            state->input_callback(
                InputValue::make_bytes(
                    std::move(bytes),
                    state->device_id,
                    InputType::MIDI));
        } else {

            uint8_t status = packet->data[0];
            uint8_t d1 = packet->length > 1 ? packet->data[1] : 0;
            uint8_t d2 = packet->length > 2 ? packet->data[2] : 0;

            state->input_callback(
                InputValue::make_midi(
                    status,
                    d1,
                    d2,
                    state->device_id));
        }

        packet = MIDIPacketNext(packet);
    }
}

void CoreMidiBackend::notify_device_change(
    const InputDeviceInfo& info,
    bool connected)
{
    std::lock_guard lock(m_callback_mutex);

    if (m_device_callback) {
        m_device_callback(info, connected);
    }
}

void CoreMidiBackend::midi_notify_callback(
    const MIDINotification* notification,
    void* ref_con)
{
    auto* self = static_cast<CoreMidiBackend*>(ref_con);

    if (!self || !notification) {
        return;
    }

    switch (notification->messageID) {

    case kMIDIMsgObjectAdded: {
        const auto* msg = reinterpret_cast<const MIDIObjectAddRemoveNotification*>(notification);

        MF_INFO(
            Journal::Component::Core,
            Journal::Context::InputBackend,
            "CoreMIDI object added (type={})",
            static_cast<int>(msg->childType));

        self->refresh_devices();
        break;
    }

    case kMIDIMsgObjectRemoved: {
        const auto* msg = reinterpret_cast<const MIDIObjectAddRemoveNotification*>(notification);

        MF_INFO(
            Journal::Component::Core,
            Journal::Context::InputBackend,
            "CoreMIDI object removed (type={})",
            static_cast<int>(msg->childType));

        self->refresh_devices();
        break;
    }

    case kMIDIMsgPropertyChanged: {
        const auto* msg = reinterpret_cast<const MIDIObjectPropertyChangeNotification*>(notification);

        std::string property_name;

        if (msg->propertyName) {
            property_name = cfstring_to_string(msg->propertyName);
        }

        MF_DEBUG(
            Journal::Component::Core,
            Journal::Context::InputBackend,
            "CoreMIDI property changed: {}",
            property_name.empty() ? "<unknown>" : property_name);

        self->refresh_devices();
        break;
    }

    case kMIDIMsgSetupChanged:
        MF_INFO(
            Journal::Component::Core,
            Journal::Context::InputBackend,
            "CoreMIDI setup changed");

        self->refresh_devices();
        break;

    case kMIDIMsgIOError: {
        const auto* msg = reinterpret_cast<const MIDIIOErrorNotification*>(notification);

        MF_WARN(
            Journal::Component::Core,
            Journal::Context::InputBackend,
            "CoreMIDI I/O error on device {} (error={})",
            msg->driverDevice,
            msg->errorCode);

        break;
    }

    default:
        MF_DEBUG(
            Journal::Component::Core,
            Journal::Context::InputBackend,
            "Unhandled CoreMIDI notification {}",
            static_cast<int>(notification->messageID));
        break;
    }
}

void CoreMidiBackend::virtual_destination_callback(
    const MIDIPacketList* packet_list,
    void* read_proc_ref_con,
    void* /*src_conn_ref_con*/)
{
    auto* self = static_cast<CoreMidiBackend*>(read_proc_ref_con);

    if (!self)
        return;

    InputCallback callback;

    {
        std::lock_guard lock(self->m_callback_mutex);
        callback = self->m_input_callback;
    }

    if (!callback)
        return;

    const MIDIPacket* packet = &packet_list->packet[0];

    for (UInt32 i = 0; i < packet_list->numPackets; ++i) {

        if (packet->length == 0) {
            packet = MIDIPacketNext(packet);
            continue;
        }

        if (packet->length > 3 && self->m_config.enable_sysex) {

            std::vector<uint8_t> bytes(
                packet->data,
                packet->data + packet->length);

            callback(
                InputValue::make_bytes(
                    std::move(bytes),
                    0,
                    InputType::MIDI));
        } else {

            uint8_t status = packet->data[0];
            uint8_t d1 = packet->length > 1 ? packet->data[1] : 0;
            uint8_t d2 = packet->length > 2 ? packet->data[2] : 0;

            callback(
                InputValue::make_midi(
                    status,
                    d1,
                    d2,
                    0));
        }

        packet = MIDIPacketNext(packet);
    }
}

std::string CoreMidiBackend::get_version() const
{
    return "CoreMIDI";
}

} // namespace MayaFlux::Core

#endif // MAYAFLUX_PLATFORM_MACOS
