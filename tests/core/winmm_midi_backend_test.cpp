#include "gtest/gtest.h"

#if defined(MAYAFLUX_PLATFORM_WINDOWS)

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "MayaFlux/Core/Backends/Input/WinMmMidiBackend.hpp"

namespace MayaFlux::Test {
namespace {

    Core::WinMmMidiBackend::Config manual_config()
    {
        Core::WinMmMidiBackend::Config config;
        config.auto_open_inputs = false;
        return config;
    }

    std::optional<std::string> find_loopback_port_name()
    {
        std::vector<std::string> output_names;
        for (UINT i = 0; i < midiOutGetNumDevs(); ++i) {
            MIDIOUTCAPS caps {};
            if (midiOutGetDevCaps(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR)
                output_names.emplace_back(caps.szPname);
        }

        for (UINT i = 0; i < midiInGetNumDevs(); ++i) {
            MIDIINCAPS caps {};
            if (midiInGetDevCaps(i, &caps, sizeof(caps)) != MMSYSERR_NOERROR)
                continue;
            const std::string name(caps.szPname);
            if (name.find("loopMIDI") == std::string::npos)
                continue;
            if (std::ranges::find(output_names, name) != output_names.end())
                return name;
        }
        return std::nullopt;
    }

    class LoopbackSender {
    public:
        LoopbackSender() = default;
        LoopbackSender(const LoopbackSender&) = delete;
        LoopbackSender& operator=(const LoopbackSender&) = delete;
        LoopbackSender(LoopbackSender&&) = delete;
        LoopbackSender& operator=(LoopbackSender&&) = delete;

        ~LoopbackSender()
        {
            if (m_handle) {
                midiOutReset(m_handle);
                midiOutClose(m_handle);
            }
        }

        bool open(const std::string& name)
        {
            for (UINT i = 0; i < midiOutGetNumDevs(); ++i) {
                MIDIOUTCAPS caps {};
                if (midiOutGetDevCaps(i, &caps, sizeof(caps)) != MMSYSERR_NOERROR)
                    continue;
                if (name == caps.szPname)
                    return midiOutOpen(&m_handle, i, 0, 0, CALLBACK_NULL) == MMSYSERR_NOERROR;
            }
            return false;
        }

        bool send_short(uint8_t status, uint8_t data1, uint8_t data2)
        {
            const DWORD packed = static_cast<DWORD>(status)
                | (static_cast<DWORD>(data1) << 8)
                | (static_cast<DWORD>(data2) << 16);
            return midiOutShortMsg(m_handle, packed) == MMSYSERR_NOERROR;
        }

        bool send_sysex(std::vector<uint8_t> bytes)
        {
            MIDIHDR header {};
            header.lpData = reinterpret_cast<LPSTR>(bytes.data());
            header.dwBufferLength = static_cast<DWORD>(bytes.size());
            header.dwBytesRecorded = static_cast<DWORD>(bytes.size());
            if (midiOutPrepareHeader(m_handle, &header, sizeof(header)) != MMSYSERR_NOERROR)
                return false;

            bool sent = midiOutLongMsg(m_handle, &header, sizeof(header)) == MMSYSERR_NOERROR;
            if (sent) {
                const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
                while (!(header.dwFlags & MHDR_DONE) && std::chrono::steady_clock::now() < deadline)
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                sent = (header.dwFlags & MHDR_DONE) != 0;
            }
            while (midiOutUnprepareHeader(m_handle, &header, sizeof(header)) == MIDIERR_STILLPLAYING)
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            return sent;
        }

    private:
        HMIDIOUT m_handle {};
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

    TEST(WinMmMidiBackendTest, InitializesAndSupportsRepeatedLifecycleCalls)
    {
        Core::WinMmMidiBackend backend(manual_config());

        EXPECT_FALSE(backend.is_initialized());
        EXPECT_FALSE(backend.is_running());
        EXPECT_EQ(backend.get_type(), Core::InputType::MIDI);
        EXPECT_EQ(backend.get_name(), "MIDI (WinMM)");
        EXPECT_EQ(backend.get_version(), "WinMM");

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

    TEST(WinMmMidiBackendTest, StartRequiresInitialization)
    {
        Core::WinMmMidiBackend backend(manual_config());
        backend.start();
        EXPECT_FALSE(backend.is_running());
    }

    TEST(WinMmMidiBackendTest, EnumeratedDevicesDescribeInputPorts)
    {
        Core::WinMmMidiBackend backend(manual_config());
        ASSERT_TRUE(backend.initialize());

        const auto devices = backend.get_devices();
        std::vector<uint32_t> ids;
        for (const auto& device : devices) {
            EXPECT_FALSE(device.name.empty());
            EXPECT_GT(device.id, 0U);
            EXPECT_EQ(device.backend_type, Core::InputType::MIDI);
            EXPECT_TRUE(device.is_connected);
            EXPECT_TRUE(device.is_input);
            EXPECT_FALSE(device.is_output);
            ids.push_back(device.id);
        }
        std::ranges::sort(ids);
        EXPECT_EQ(std::ranges::adjacent_find(ids), ids.end());

        EXPECT_EQ(backend.refresh_devices(), devices.size());
        const auto refreshed = backend.get_devices();
        ASSERT_EQ(refreshed.size(), devices.size());
        for (const auto& device : devices) {
            const auto match = std::ranges::find_if(refreshed, [&](const auto& other) {
                return other.name == device.name;
            });
            ASSERT_NE(match, refreshed.end());
            EXPECT_EQ(match->id, device.id);
        }

        backend.shutdown();
    }

    TEST(WinMmMidiBackendTest, FilterExcludesNonMatchingPorts)
    {
        auto config = manual_config();
        config.input_port_filters = { "MayaFlux no such midi port" };
        Core::WinMmMidiBackend backend(config);
        ASSERT_TRUE(backend.initialize());

        EXPECT_TRUE(backend.get_devices().empty());
        EXPECT_EQ(backend.refresh_devices(), 0U);

        backend.shutdown();
    }

    TEST(WinMmMidiBackendTest, FilterSelectsPortBySubstring)
    {
        std::string first_name;
        {
            Core::WinMmMidiBackend probe(manual_config());
            ASSERT_TRUE(probe.initialize());
            const auto devices = probe.get_devices();
            if (devices.empty())
                GTEST_SKIP() << "WinMM has no MIDI input port";
            first_name = devices.front().name;
            probe.shutdown();
        }

        auto config = manual_config();
        config.input_port_filters = { first_name };
        Core::WinMmMidiBackend backend(config);
        ASSERT_TRUE(backend.initialize());

        const auto devices = backend.get_devices();
        ASSERT_FALSE(devices.empty());
        for (const auto& device : devices)
            EXPECT_NE(device.name.find(first_name), std::string::npos);

        backend.shutdown();
    }

    TEST(WinMmMidiBackendTest, ReportsDiscoveredPortsThroughDeviceCallback)
    {
        Core::WinMmMidiBackend backend(manual_config());
        std::vector<std::string> announced;
        backend.set_device_callback([&announced](const Core::InputDeviceInfo& info, bool connected) {
            if (connected)
                announced.push_back(info.name);
        });
        ASSERT_TRUE(backend.initialize());

        const auto devices = backend.get_devices();
        ASSERT_EQ(announced.size(), devices.size());
        for (const auto& device : devices)
            EXPECT_NE(std::ranges::find(announced, device.name), announced.end());

        backend.shutdown();
    }

    TEST(WinMmMidiBackendTest, RejectsUnknownDeviceIds)
    {
        Core::WinMmMidiBackend backend(manual_config());
        ASSERT_TRUE(backend.initialize());

        EXPECT_FALSE(backend.open_device(UINT32_MAX));
        EXPECT_FALSE(backend.is_device_open(UINT32_MAX));
        EXPECT_TRUE(backend.get_open_devices().empty());
        backend.close_device(UINT32_MAX);

        backend.shutdown();
    }

    TEST(WinMmMidiBackendTest, OpensAndClosesInputPort)
    {
        Core::WinMmMidiBackend backend(manual_config());
        ASSERT_TRUE(backend.initialize());
        const auto devices = backend.get_devices();
        if (devices.empty())
            GTEST_SKIP() << "WinMM has no MIDI input port";

        const auto device_id = devices.front().id;
        if (!backend.open_device(device_id))
            GTEST_SKIP() << "WinMM input port is held by another client";

        EXPECT_TRUE(backend.is_device_open(device_id));
        EXPECT_EQ(backend.get_open_devices(), std::vector<uint32_t> { device_id });
        EXPECT_TRUE(backend.open_device(device_id));
        EXPECT_EQ(backend.get_open_devices().size(), 1U);

        backend.close_device(device_id);
        EXPECT_FALSE(backend.is_device_open(device_id));
        EXPECT_TRUE(backend.get_open_devices().empty());
        backend.close_device(device_id);

        backend.shutdown();
    }

    TEST(WinMmMidiBackendTest, OpensConfiguredInputsOnStartAndClosesOnStop)
    {
        Core::WinMmMidiBackend backend;
        ASSERT_TRUE(backend.initialize());
        const auto devices = backend.get_devices();
        if (devices.empty())
            GTEST_SKIP() << "WinMM has no MIDI input port";
        EXPECT_TRUE(backend.get_open_devices().empty());

        backend.start();
        ASSERT_TRUE(backend.is_running());
        if (backend.get_open_devices().empty())
            GTEST_SKIP() << "WinMM input ports are held by another client";
        for (const auto id : backend.get_open_devices())
            EXPECT_TRUE(backend.is_device_open(id));

        backend.stop();
        EXPECT_FALSE(backend.is_running());
        EXPECT_TRUE(backend.get_open_devices().empty());
        backend.shutdown();
    }

    TEST(WinMmMidiBackendTest, ShutdownClosesOpenPortsAndAllowsReinitialization)
    {
        Core::WinMmMidiBackend backend;
        ASSERT_TRUE(backend.initialize());
        if (backend.get_devices().empty())
            GTEST_SKIP() << "WinMM has no MIDI input port";

        backend.start();
        if (backend.get_open_devices().empty())
            GTEST_SKIP() << "WinMM input ports are held by another client";

        backend.shutdown();
        EXPECT_FALSE(backend.is_initialized());
        EXPECT_FALSE(backend.is_running());
        EXPECT_TRUE(backend.get_open_devices().empty());
        EXPECT_TRUE(backend.get_devices().empty());

        ASSERT_TRUE(backend.initialize());
        EXPECT_FALSE(backend.get_devices().empty());
        backend.shutdown();
    }

    TEST(WinMmMidiBackendTest, DeliversShortAndSystemExclusiveMessagesFromLoopback)
    {
        const auto port_name = find_loopback_port_name();
        if (!port_name)
            GTEST_SKIP() << "No loopMIDI port with matching input and output endpoints";

        LoopbackSender sender;
        if (!sender.open(*port_name))
            GTEST_SKIP() << "loopMIDI output endpoint could not be opened";

        auto config = manual_config();
        config.input_port_filters = { *port_name };
        Core::WinMmMidiBackend backend(config);
        auto received = std::make_shared<ReceivedValues>();
        backend.set_input_callback([received](const Core::InputValue& value) {
            received->push(value);
        });
        ASSERT_TRUE(backend.initialize());

        const auto devices = backend.get_devices();
        ASSERT_EQ(devices.size(), 1U);
        const auto device_id = devices.front().id;
        if (!backend.open_device(device_id))
            GTEST_SKIP() << "loopMIDI input endpoint is held by another client";
        backend.start();
        ASSERT_TRUE(backend.is_running());

        ASSERT_TRUE(sender.send_short(0x92, 64, 101));
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
        ASSERT_TRUE(sender.send_sysex(sysex));
        ASSERT_TRUE(received->wait_for_size(2));
        values = received->values();
        ASSERT_EQ(values.size(), 2U);
        EXPECT_EQ(values[1].type, Core::InputValue::Type::BYTES);
        EXPECT_EQ(values[1].source_type, Core::InputType::MIDI);
        EXPECT_EQ(values[1].device_id, device_id);
        EXPECT_EQ(values[1].as_bytes(), sysex);

        backend.stop();
        EXPECT_FALSE(backend.is_device_open(device_id));
        backend.shutdown();
    }

    TEST(WinMmMidiBackendTest, StopsDeliveringAfterPortIsClosed)
    {
        const auto port_name = find_loopback_port_name();
        if (!port_name)
            GTEST_SKIP() << "No loopMIDI port with matching input and output endpoints";

        LoopbackSender sender;
        if (!sender.open(*port_name))
            GTEST_SKIP() << "loopMIDI output endpoint could not be opened";

        auto config = manual_config();
        config.input_port_filters = { *port_name };
        Core::WinMmMidiBackend backend(config);
        auto received = std::make_shared<ReceivedValues>();
        backend.set_input_callback([received](const Core::InputValue& value) {
            received->push(value);
        });
        ASSERT_TRUE(backend.initialize());

        const auto devices = backend.get_devices();
        ASSERT_EQ(devices.size(), 1U);
        const auto device_id = devices.front().id;
        if (!backend.open_device(device_id))
            GTEST_SKIP() << "loopMIDI input endpoint is held by another client";

        ASSERT_TRUE(sender.send_short(0x90, 60, 80));
        ASSERT_TRUE(received->wait_for_size(1));

        backend.close_device(device_id);
        ASSERT_TRUE(sender.send_short(0x80, 60, 0));
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        EXPECT_EQ(received->values().size(), 1U);

        backend.shutdown();
    }

}
}

#endif
