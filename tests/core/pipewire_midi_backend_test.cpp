#include "gtest/gtest.h"

#if defined(MAYAFLUX_PLATFORM_LINUX) && defined(PIPEWIRE_BACKEND)

#include <alsa/asoundlib.h>
#include <pipewire/pipewire.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "MayaFlux/Core/Backends/Audio/PipewireBackend.hpp"
#include "MayaFlux/Core/Backends/Input/PipewireMidiBackend.hpp"

namespace MayaFlux::Test {
namespace {

    std::string unique_name(const char* prefix)
    {
        return std::string(prefix)
            + std::to_string(
                std::chrono::steady_clock::now().time_since_epoch().count());
    }

    template <typename Predicate>
    bool eventually(Predicate predicate)
    {
        const auto deadline = std::chrono::steady_clock::now()
            + std::chrono::seconds(2);
        while (std::chrono::steady_clock::now() < deadline) {
            if (predicate()) {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        return predicate();
    }

    class TestAlsaMidiSource {
    public:
        TestAlsaMidiSource() = default;
        TestAlsaMidiSource(const TestAlsaMidiSource&) = delete;
        TestAlsaMidiSource& operator=(const TestAlsaMidiSource&) = delete;

        ~TestAlsaMidiSource()
        {
            if (m_source) {
                snd_seq_close(m_source);
            }
        }

        bool create(const std::string& name)
        {
            if (snd_seq_open(&m_source, "default", SND_SEQ_OPEN_DUPLEX,
                    SND_SEQ_NONBLOCK)
                < 0) {
                return false;
            }
            snd_seq_set_client_name(m_source, name.c_str());
            m_source_port = snd_seq_create_simple_port(
                m_source, name.c_str(),
                SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ,
                SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION);
            if (m_source_port < 0) {
                return false;
            }

            return true;
        }

        [[nodiscard]] int client_id() const
        {
            return snd_seq_client_id(m_source);
        }

        [[nodiscard]] int port_id() const
        {
            return m_source_port;
        }

        bool connect_to(int client, int port)
        {
            return snd_seq_connect_to(m_source, m_source_port, client, port) >= 0;
        }

        bool send_note_on(uint8_t channel, uint8_t note, uint8_t velocity)
        {
            snd_seq_event_t event {};
            snd_seq_ev_clear(&event);
            snd_seq_ev_set_source(&event, m_source_port);
            snd_seq_ev_set_subs(&event);
            snd_seq_ev_set_direct(&event);
            snd_seq_ev_set_noteon(&event, channel, note, velocity);
            return snd_seq_event_output_direct(m_source, &event) >= 0;
        }

        bool send_controller(uint8_t channel, uint8_t controller, uint8_t value)
        {
            snd_seq_event_t event {};
            snd_seq_ev_clear(&event);
            snd_seq_ev_set_source(&event, m_source_port);
            snd_seq_ev_set_subs(&event);
            snd_seq_ev_set_direct(&event);
            snd_seq_ev_set_controller(&event, channel, controller, value);
            return snd_seq_event_output_direct(m_source, &event) >= 0;
        }

    private:
        snd_seq_t* m_source {};
        int m_source_port { -1 };
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

    std::string alsa_object_path(const TestAlsaMidiSource& source)
    {
        return "client_" + std::to_string(source.client_id())
            + "/capture_" + std::to_string(source.port_id());
    }

    uint32_t register_source(
        Core::PipewireMidiBackend& backend,
        uint32_t pw_id,
        const std::string& name,
        const TestAlsaMidiSource& source)
    {
        backend.register_midi_port(pw_id, name, alsa_object_path(source));
        for (const auto& info : backend.get_devices()) {
            if (info.name == name) {
                return info.id;
            }
        }
        return UINT32_MAX;
    }

    bool find_alsa_port(const std::string& name, int& client, int& port)
    {
        snd_seq_t* seq {};
        if (snd_seq_open(&seq, "default", SND_SEQ_OPEN_DUPLEX, SND_SEQ_NONBLOCK) < 0) {
            return false;
        }

        bool found = false;
        snd_seq_client_info_t* cinfo {};
        snd_seq_client_info_alloca(&cinfo);
        snd_seq_port_info_t* pinfo {};
        snd_seq_port_info_alloca(&pinfo);

        snd_seq_client_info_set_client(cinfo, -1);
        while (!found && snd_seq_query_next_client(seq, cinfo) >= 0) {
            const int c = snd_seq_client_info_get_client(cinfo);
            snd_seq_port_info_set_client(pinfo, c);
            snd_seq_port_info_set_port(pinfo, -1);
            while (snd_seq_query_next_port(seq, pinfo) >= 0) {
                if (name == snd_seq_port_info_get_name(pinfo)) {
                    client = c;
                    port = snd_seq_port_info_get_port(pinfo);
                    found = true;
                    break;
                }
            }
        }

        snd_seq_close(seq);
        return found;
    }

    TEST(PipewireMidiBackendTest, InitializesAndSupportsRepeatedLifecycleCalls)
    {
        Core::PipewireMidiBackend::Config config;
        config.auto_open_inputs = false;
        Core::PipewireMidiBackend backend(config);

        EXPECT_FALSE(backend.is_initialized());
        EXPECT_FALSE(backend.is_running());
        EXPECT_EQ(backend.get_type(), Core::InputType::MIDI);
        EXPECT_EQ(backend.get_name(), "MIDI (PipeWire)");
        EXPECT_FALSE(backend.get_version().empty());

        ASSERT_TRUE(backend.initialize());
        EXPECT_TRUE(backend.is_initialized());
        EXPECT_TRUE(backend.initialize());

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

        ASSERT_TRUE(backend.initialize());
        EXPECT_TRUE(backend.is_initialized());
        backend.shutdown();
    }

    TEST(PipewireMidiBackendTest, EnumeratesPipeWireMidiSources)
    {
        Core::PipewireMidiBackend::Config config;
        config.auto_open_inputs = false;
        Core::PipewireMidiBackend backend(config);
        ASSERT_TRUE(backend.initialize());

        const auto devices = backend.get_devices();
        for (const auto& device : devices) {
            EXPECT_GT(device.id, 0U);
            EXPECT_FALSE(device.name.empty());
            EXPECT_EQ(device.backend_type, Core::InputType::MIDI);
            EXPECT_TRUE(device.is_connected);
            EXPECT_TRUE(device.is_input);
            EXPECT_FALSE(device.is_output);
        }
        EXPECT_EQ(backend.refresh_devices(), devices.size());

        backend.shutdown();
    }

    TEST(PipewireMidiBackendTest, FiltersRegisteredPortsAndReportsDeviceChanges)
    {
        const auto accepted_name = unique_name("MayaFluxPipeWireMidiAccepted");
        Core::PipewireMidiBackend::Config config;
        config.auto_open_inputs = false;
        config.input_port_filters = { accepted_name };
        Core::PipewireMidiBackend backend(config);

        std::vector<std::pair<Core::InputDeviceInfo, bool>> changes;
        backend.set_device_callback([&](const Core::InputDeviceInfo& info, bool connected) {
            changes.emplace_back(info, connected);
        });

        backend.register_midi_port(7001, "unmatched port", "");
        backend.register_midi_port(7002, accepted_name, "");

        const auto devices = backend.get_devices();
        ASSERT_EQ(devices.size(), 1U);
        EXPECT_EQ(devices.front().name, accepted_name);
        EXPECT_EQ(devices.front().backend_type, Core::InputType::MIDI);
        EXPECT_TRUE(devices.front().is_connected);
        EXPECT_TRUE(devices.front().is_input);
        EXPECT_FALSE(devices.front().is_output);
        EXPECT_EQ(backend.refresh_devices(), 1U);

        ASSERT_EQ(changes.size(), 1U);
        EXPECT_EQ(changes.front().first.name, accepted_name);
        EXPECT_TRUE(changes.front().second);
    }

    TEST(PipewireMidiBackendTest, OpensAndClosesAlsaInputAndRejectsUnknownIds)
    {
        TestAlsaMidiSource source;
        if (!source.create(unique_name("MayaFluxPipeWireMidiOpenSource"))) {
            GTEST_SKIP() << "ALSA sequencer source is unavailable";
        }

        Core::PipewireMidiBackend::Config config;
        config.auto_open_inputs = false;
        Core::PipewireMidiBackend backend(config);
        ASSERT_TRUE(backend.initialize());

        const auto name = unique_name("MayaFluxPipeWireMidiOpen");
        const auto device_id = register_source(backend, 7101, name, source);
        ASSERT_NE(device_id, UINT32_MAX);

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

        ASSERT_TRUE(backend.open_device(device_id));
        backend.close_device(device_id);
        backend.shutdown();
        EXPECT_FALSE(backend.is_initialized());
        EXPECT_FALSE(backend.is_device_open(device_id));
    }

    TEST(PipewireMidiBackendTest, DeliversMidiInputFromAnOpenedAlsaSource)
    {
        TestAlsaMidiSource source;
        if (!source.create(unique_name("MayaFluxPipeWireMidiInputSource"))) {
            GTEST_SKIP() << "ALSA sequencer source is unavailable";
        }

        Core::PipewireMidiBackend::Config config;
        config.auto_open_inputs = false;
        Core::PipewireMidiBackend backend(config);
        auto received = std::make_shared<ReceivedValues>();
        backend.set_input_callback([received](const Core::InputValue& value) {
            received->push(value);
        });
        ASSERT_TRUE(backend.initialize());

        const auto name = unique_name("MayaFluxPipeWireMidiInput");
        const auto device_id = register_source(backend, 7301, name, source);
        ASSERT_NE(device_id, UINT32_MAX);
        ASSERT_TRUE(backend.open_device(device_id));
        backend.start();

        ASSERT_TRUE(source.send_note_on(2, 64, 101));
        ASSERT_TRUE(received->wait_for_size(1));
        auto values = received->values();
        ASSERT_EQ(values.size(), 1U);
        EXPECT_EQ(values[0].type, Core::InputValue::Type::MIDI);
        EXPECT_EQ(values[0].source_type, Core::InputType::MIDI);
        EXPECT_EQ(values[0].device_id, device_id);
        EXPECT_EQ(values[0].as_midi().status, 0x92);
        EXPECT_EQ(values[0].as_midi().data1, 64);
        EXPECT_EQ(values[0].as_midi().data2, 101);

        ASSERT_TRUE(source.send_controller(1, 74, 83));
        ASSERT_TRUE(received->wait_for_size(2));
        values = received->values();
        ASSERT_EQ(values.size(), 2U);
        EXPECT_EQ(values[1].as_midi().status, 0xB1);
        EXPECT_EQ(values[1].as_midi().data1, 74);
        EXPECT_EQ(values[1].as_midi().data2, 83);

        backend.stop();
        EXPECT_FALSE(backend.is_running());
        EXPECT_FALSE(backend.is_device_open(device_id));
        backend.shutdown();
    }

    TEST(PipewireMidiBackendTest, VirtualPortIsAbsentByDefault)
    {
        const auto name = unique_name("MayaFluxVirtualDisabled");
        Core::PipewireMidiBackend::Config config;
        config.virtual_port_name = name;
        Core::PipewireMidiBackend backend(config);
        ASSERT_TRUE(backend.initialize());

        int client = -1;
        int port = -1;
        EXPECT_FALSE(find_alsa_port(name, client, port));
        EXPECT_TRUE(backend.get_open_devices().empty());

        backend.shutdown();
    }

    TEST(PipewireMidiBackendTest, VirtualPortAcceptsMidiAndFollowsLifecycle)
    {
        TestAlsaMidiSource source;
        if (!source.create(unique_name("MayaFluxVirtualSource"))) {
            GTEST_SKIP() << "ALSA sequencer source is unavailable";
        }

        const auto name = unique_name("MayaFluxVirtualIn");
        Core::PipewireMidiBackend::Config config;
        config.enable_virtual_port = true;
        config.virtual_port_name = name;
        Core::PipewireMidiBackend backend(config);
        auto received = std::make_shared<ReceivedValues>();
        backend.set_input_callback([received](const Core::InputValue& value) {
            received->push(value);
        });
        ASSERT_TRUE(backend.initialize());

        const auto devices = backend.get_devices();
        ASSERT_EQ(std::ranges::count_if(devices, [&](const auto& d) { return d.name == name; }), 1);
        const auto virtual_device = *std::ranges::find_if(devices, [&](const auto& d) { return d.name == name; });
        EXPECT_TRUE(virtual_device.is_input);
        EXPECT_FALSE(virtual_device.is_output);
        const auto device_id = virtual_device.id;
        EXPECT_TRUE(backend.is_device_open(device_id));

        int client = -1;
        int port = -1;
        ASSERT_TRUE(find_alsa_port(name, client, port));
        ASSERT_TRUE(source.connect_to(client, port));

        backend.start();
        ASSERT_TRUE(source.send_note_on(3, 60, 90));
        ASSERT_TRUE(received->wait_for_size(1));
        auto values = received->values();
        ASSERT_EQ(values.size(), 1U);
        EXPECT_EQ(values[0].source_type, Core::InputType::MIDI);
        EXPECT_EQ(values[0].device_id, device_id);
        EXPECT_EQ(values[0].as_midi().status, 0x93);
        EXPECT_EQ(values[0].as_midi().data1, 60);
        EXPECT_EQ(values[0].as_midi().data2, 90);

        backend.stop();
        EXPECT_TRUE(backend.is_device_open(device_id));
        ASSERT_TRUE(source.send_controller(0, 7, 100));
        ASSERT_TRUE(received->wait_for_size(2));

        backend.shutdown();
        EXPECT_FALSE(find_alsa_port(name, client, port));

        ASSERT_TRUE(backend.initialize());
        EXPECT_TRUE(find_alsa_port(name, client, port));
        backend.shutdown();
    }

    TEST(PipewireMidiBackendTest, MidiCleanupLeavesAudioBackendUsable)
    {
        Core::PipewireBackend audio_backend;
        auto audio_devices = audio_backend.create_device_manager();
        ASSERT_NE(audio_devices, nullptr);

        Core::PipewireMidiBackend midi_backend;
        ASSERT_TRUE(midi_backend.initialize());
        midi_backend.shutdown();

        EXPECT_FALSE(midi_backend.is_initialized());
        EXPECT_NE(audio_backend.core(), nullptr);
        EXPECT_NO_THROW(audio_devices->get_output_devices());
        EXPECT_NO_THROW(audio_devices->get_input_devices());
    }
}
}

#endif
