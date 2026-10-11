#include "gtest/gtest.h"

#if defined(MAYAFLUX_PLATFORM_MACOS)

#include <CoreFoundation/CoreFoundation.h>
#include <CoreMIDI/CoreMIDI.h>
#include <mach/mach_time.h>

#include <chrono>
#include <cstddef>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "MayaFlux/Core/Backends/Input/CoreMidiBackend.hpp"

namespace MayaFlux::Test {
namespace {

    std::string unique_name(const char* prefix)
    {
        return std::string(prefix)
            + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    }

    CFStringRef make_cfstring(const std::string& value)
    {
        return CFStringCreateWithCString(
            kCFAllocatorDefault,
            value.c_str(),
            kCFStringEncodingUTF8);
    }

    std::string endpoint_name(MIDIObjectRef endpoint)
    {
        CFStringRef name_ref = nullptr;
        if (MIDIObjectGetStringProperty(endpoint, kMIDIPropertyName, &name_ref) != noErr
            || !name_ref) {
            return {};
        }

        char buffer[512] {};
        const bool converted = CFStringGetCString(
            name_ref,
            buffer,
            sizeof(buffer),
            kCFStringEncodingUTF8);
        CFRelease(name_ref);
        return converted ? std::string(buffer) : std::string {};
    }

    MIDIEndpointRef find_destination(const std::string& name)
    {
        const ItemCount count = MIDIGetNumberOfDestinations();
        for (ItemCount index = 0; index < count; ++index) {
            const auto endpoint = MIDIGetDestination(index);
            if (endpoint && endpoint_name(endpoint) == name)
                return endpoint;
        }
        return 0;
    }

    class TestMidiClient {
    public:
        TestMidiClient() = default;
        TestMidiClient(const TestMidiClient&) = delete;
        TestMidiClient& operator=(const TestMidiClient&) = delete;
        TestMidiClient(TestMidiClient&&) = delete;
        TestMidiClient& operator=(TestMidiClient&&) = delete;

        ~TestMidiClient()
        {
            if (m_source)
                MIDIEndpointDispose(m_source);
            if (m_output_port)
                MIDIPortDispose(m_output_port);
            if (m_client)
                MIDIClientDispose(m_client);
        }

        bool create_client(const std::string& name)
        {
            CFStringRef cf_name = make_cfstring(name);
            if (!cf_name)
                return false;

            const auto status = MIDIClientCreate(cf_name, nullptr, nullptr, &m_client);
            CFRelease(cf_name);
            return status == noErr;
        }

        MIDIEndpointRef create_source(const std::string& name)
        {
            CFStringRef cf_name = make_cfstring(name);
            if (!cf_name)
                return 0;

            const auto status = MIDISourceCreate(m_client, cf_name, &m_source);
            CFRelease(cf_name);
            return status == noErr ? m_source : 0;
        }

        bool create_output_port()
        {
            return MIDIOutputPortCreate(
                       m_client,
                       CFSTR("MayaFlux CoreMIDI Test Output"),
                       &m_output_port)
                == noErr;
        }

        OSStatus send(MIDIEndpointRef destination, const std::vector<uint8_t>& data)
        {
            MIDIPacketList packet_list {};
            auto* current_packet = MIDIPacketListInit(&packet_list);
            current_packet = MIDIPacketListAdd(
                &packet_list,
                sizeof(packet_list),
                current_packet,
                0,
                static_cast<ByteCount>(data.size()),
                data.data());
            if (!current_packet)
                return -1;
            return MIDISend(m_output_port, destination, &packet_list);
        }

        OSStatus send_to_source(const std::vector<uint8_t>& data)
        {
            MIDIPacketList packet_list {};
            auto* current_packet = MIDIPacketListInit(&packet_list);
            current_packet = MIDIPacketListAdd(
                &packet_list,
                sizeof(packet_list),
                current_packet,
                mach_absolute_time(),
                static_cast<ByteCount>(data.size()),
                data.data());
            if (!current_packet)
                return -1;
            return MIDIReceived(m_source, &packet_list);
        }

    private:
        MIDIClientRef m_client {};
        MIDIEndpointRef m_source {};
        MIDIPortRef m_output_port {};
    };

    class ReceivedValues {
    public:
        void push(const Core::InputValue& value)
        {
            {
                std::lock_guard lock(m_mutex);
                m_values.push_back(value);
            }
            m_changed.notify_all();
        }

        bool wait_for_size(std::size_t size)
        {
            std::unique_lock lock(m_mutex);
            return m_changed.wait_for(lock, std::chrono::seconds(2), [&] {
                return m_values.size() >= size;
            });
        }

        std::vector<Core::InputValue> values() const
        {
            std::lock_guard lock(m_mutex);
            return m_values;
        }

    private:
        mutable std::mutex m_mutex;
        std::condition_variable m_changed;
        std::vector<Core::InputValue> m_values;
    };

    TEST(CoreMidiBackendTest, InitializesAndSupportsRepeatedLifecycleCalls)
    {
        Core::CoreMidiBackend::Config config;
        config.auto_open_inputs = false;
        Core::CoreMidiBackend backend(config);

        EXPECT_FALSE(backend.is_initialized());
        EXPECT_FALSE(backend.is_running());
        EXPECT_EQ(backend.get_type(), Core::InputType::MIDI);
        EXPECT_EQ(backend.get_name(), "MIDI (CoreMIDI)");
        EXPECT_EQ(backend.get_version(), "CoreMIDI");

        ASSERT_TRUE(backend.initialize());
        EXPECT_TRUE(backend.is_initialized());
        EXPECT_TRUE(backend.initialize());
        EXPECT_FALSE(backend.is_running());

        backend.start();
        EXPECT_TRUE(backend.is_running());
        backend.start();
        EXPECT_TRUE(backend.is_running());

        backend.stop();
        EXPECT_FALSE(backend.is_running());
        backend.stop();
        EXPECT_FALSE(backend.is_running());

        backend.shutdown();
        EXPECT_FALSE(backend.is_initialized());
        backend.shutdown();
        EXPECT_FALSE(backend.is_running());

        ASSERT_TRUE(backend.initialize());
        EXPECT_TRUE(backend.is_initialized());
        backend.shutdown();
    }

    TEST(CoreMidiBackendTest, RefreshesFilteredInputDevices)
    {
        TestMidiClient source_client;
        ASSERT_TRUE(source_client.create_client(unique_name("MayaFluxCoreMidiFilterClient")));
        const auto source_name = unique_name("MayaFluxCoreMidiFilterSource");
        ASSERT_NE(source_client.create_source(source_name), 0U);

        Core::CoreMidiBackend::Config config;
        config.auto_open_inputs = false;
        config.input_port_filters = { source_name };
        Core::CoreMidiBackend backend(config);
        ASSERT_TRUE(backend.initialize());

        const auto devices = backend.get_devices();
        ASSERT_EQ(devices.size(), 1U);
        EXPECT_EQ(devices.front().name, source_name);
        EXPECT_GT(devices.front().id, 0U);
        EXPECT_EQ(devices.front().backend_type, Core::InputType::MIDI);
        EXPECT_TRUE(devices.front().is_connected);
        EXPECT_TRUE(devices.front().is_input);
        EXPECT_FALSE(devices.front().is_output);
        EXPECT_EQ(backend.refresh_devices(), 1U);
        EXPECT_EQ(backend.get_devices().front().id, devices.front().id);

        backend.shutdown();
    }

    TEST(CoreMidiBackendTest, OpensAndClosesVirtualInputAndRejectsUnknownIds)
    {
        TestMidiClient source_client;
        ASSERT_TRUE(source_client.create_client(unique_name("MayaFluxCoreMidiOpenClient")));
        const auto source_name = unique_name("MayaFluxCoreMidiOpenSource");
        ASSERT_NE(source_client.create_source(source_name), 0U);

        Core::CoreMidiBackend::Config config;
        config.auto_open_inputs = false;
        config.input_port_filters = { source_name };
        Core::CoreMidiBackend backend(config);
        ASSERT_TRUE(backend.initialize());
        const auto devices = backend.get_devices();
        ASSERT_EQ(devices.size(), 1U);
        const auto device_id = devices.front().id;

        EXPECT_FALSE(backend.open_device(UINT32_MAX));
        EXPECT_FALSE(backend.is_device_open(device_id));
        EXPECT_TRUE(backend.get_open_devices().empty());

        ASSERT_TRUE(backend.open_device(device_id));
        EXPECT_TRUE(backend.is_device_open(device_id));
        EXPECT_EQ(backend.get_open_devices(), std::vector<uint32_t> { device_id });
        EXPECT_TRUE(backend.open_device(device_id));
        EXPECT_EQ(backend.get_open_devices().size(), 1U);

        backend.close_device(device_id);
        EXPECT_FALSE(backend.is_device_open(device_id));
        EXPECT_TRUE(backend.get_open_devices().empty());
        backend.close_device(device_id);
        backend.close_device(UINT32_MAX);

        backend.shutdown();
    }

    TEST(CoreMidiBackendTest, OpensConfiguredInputsOnStart)
    {
        TestMidiClient source_client;
        ASSERT_TRUE(source_client.create_client(unique_name("MayaFluxCoreMidiAutoOpenClient")));
        const auto source_name = unique_name("MayaFluxCoreMidiAutoOpenSource");
        ASSERT_NE(source_client.create_source(source_name), 0U);

        Core::CoreMidiBackend::Config config;
        config.input_port_filters = { source_name };
        Core::CoreMidiBackend backend(config);
        ASSERT_TRUE(backend.initialize());
        const auto devices = backend.get_devices();
        ASSERT_EQ(devices.size(), 1U);
        EXPECT_TRUE(backend.get_open_devices().empty());

        backend.start();
        EXPECT_TRUE(backend.is_running());
        ASSERT_TRUE(backend.is_device_open(devices.front().id));
        EXPECT_EQ(backend.get_open_devices(), std::vector<uint32_t> { devices.front().id });

        backend.stop();
        EXPECT_FALSE(backend.is_running());
        EXPECT_FALSE(backend.is_device_open(devices.front().id));
        backend.shutdown();
    }

    TEST(CoreMidiBackendTest, DeliversShortAndSystemExclusiveInputMessages)
    {
        TestMidiClient source_client;
        ASSERT_TRUE(source_client.create_client(unique_name("MayaFluxCoreMidiInputClient")));
        const auto source_name = unique_name("MayaFluxCoreMidiInputSource");
        ASSERT_NE(source_client.create_source(source_name), 0U);

        Core::CoreMidiBackend::Config config;
        config.auto_open_inputs = false;
        config.input_port_filters = { source_name };
        Core::CoreMidiBackend backend(config);
        auto received = std::make_shared<ReceivedValues>();
        backend.set_input_callback([received](const Core::InputValue& value) {
            received->push(value);
        });
        ASSERT_TRUE(backend.initialize());

        const auto devices = backend.get_devices();
        ASSERT_EQ(devices.size(), 1U);
        const auto device_id = devices.front().id;
        ASSERT_TRUE(backend.open_device(device_id));
        backend.start();
        ASSERT_TRUE(backend.is_running());

        ASSERT_EQ(source_client.send_to_source({ 0x92, 64, 101 }), noErr);
        ASSERT_TRUE(received->wait_for_size(1));
        auto values = received->values();
        ASSERT_EQ(values.size(), 1U);
        EXPECT_EQ(values[0].type, Core::InputValue::Type::MIDI);
        EXPECT_EQ(values[0].source_type, Core::InputType::MIDI);
        EXPECT_EQ(values[0].device_id, device_id);
        EXPECT_EQ(values[0].as_midi().status, 0x92);
        EXPECT_EQ(values[0].as_midi().data1, 64);
        EXPECT_EQ(values[0].as_midi().data2, 101);

        const std::vector<uint8_t> sysex { 0xF0, 0x7D, 0x01, 0x02, 0x03, 0xF7 };
        ASSERT_EQ(source_client.send_to_source(sysex), noErr);
        ASSERT_TRUE(received->wait_for_size(2));
        values = received->values();
        ASSERT_EQ(values.size(), 2U);
        EXPECT_EQ(values[1].type, Core::InputValue::Type::BYTES);
        EXPECT_EQ(values[1].source_type, Core::InputType::MIDI);
        EXPECT_EQ(values[1].device_id, device_id);
        EXPECT_EQ(values[1].as_bytes(), sysex);

        backend.stop();
        EXPECT_FALSE(backend.is_running());
        EXPECT_FALSE(backend.is_device_open(device_id));
        backend.shutdown();
    }

    TEST(CoreMidiBackendTest, DeliversToConfiguredVirtualDestination)
    {
        const auto destination_name = unique_name("MayaFluxCoreMidiDestination");
        Core::CoreMidiBackend::Config config;
        config.auto_open_inputs = false;
        config.enable_virtual_port = true;
        config.enable_sysex = true;
        config.virtual_port_name = destination_name;

        Core::CoreMidiBackend backend(config);
        auto received = std::make_shared<ReceivedValues>();
        backend.set_input_callback([received](const Core::InputValue& value) {
            received->push(value);
        });
        ASSERT_TRUE(backend.initialize());
        backend.start();

        const auto destination = find_destination(destination_name);
        ASSERT_NE(destination, 0U);

        TestMidiClient sender;
        ASSERT_TRUE(sender.create_client(unique_name("MayaFluxCoreMidiSender")));
        ASSERT_TRUE(sender.create_output_port());

        ASSERT_EQ(sender.send(destination, { 0x91, 60, 90 }), noErr);
        ASSERT_TRUE(received->wait_for_size(1));
        auto values = received->values();
        ASSERT_EQ(values.size(), 1U);
        EXPECT_EQ(values[0].type, Core::InputValue::Type::MIDI);
        EXPECT_EQ(values[0].source_type, Core::InputType::MIDI);
        EXPECT_EQ(values[0].device_id, 0U);
        EXPECT_EQ(values[0].as_midi().status, 0x91);
        EXPECT_EQ(values[0].as_midi().data1, 60);
        EXPECT_EQ(values[0].as_midi().data2, 90);

        const std::vector<uint8_t> sysex { 0xF0, 0x7D, 0x10, 0x20, 0x30, 0xF7 };
        ASSERT_EQ(sender.send(destination, sysex), noErr);
        ASSERT_TRUE(received->wait_for_size(2));
        values = received->values();
        ASSERT_EQ(values.size(), 2U);
        EXPECT_EQ(values[1].type, Core::InputValue::Type::BYTES);
        EXPECT_EQ(values[1].source_type, Core::InputType::MIDI);
        EXPECT_EQ(values[1].device_id, 0U);
        EXPECT_EQ(values[1].as_bytes(), sysex);

        backend.stop();
        backend.shutdown();
    }

}
}

#endif
