#include "gtest/gtest.h"

#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "MayaFlux/Core/Input/InputManager.hpp"
#include "MayaFlux/Registry/BackendRegistry.hpp"
#include "MayaFlux/Registry/Service/InputService.hpp"
#include "MayaFlux/Registry/Service/NetworkService.hpp"

#include "../support/input_test_support.hpp"

namespace MayaFlux::Test {
namespace {

    using namespace std::chrono_literals;
    using Core::InputBinding;
    using Core::InputDeviceInfo;
    using Core::InputManager;
    using Core::InputType;
    using Core::InputValue;

    template <typename Predicate>
    bool eventually(Predicate predicate, std::chrono::milliseconds timeout = 2s)
    {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            if (predicate())
                return true;
            std::this_thread::sleep_for(1ms);
        }
        return predicate();
    }

    Core::OSCConfigInfo osc_config(uint16_t receive_port = 9123)
    {
        Core::OSCConfigInfo config;
        config.enabled = true;
        config.receive_port = receive_port;
        config.send_port = 9124;
        config.send_address = "10.0.0.7";
        return config;
    }

    std::vector<uint8_t> osc_float_datagram()
    {
        return {
            '/', 'm', 'f', '/', 't', 'e', 's', 't', 0, 0, 0, 0,
            ',', 'f', 0, 0,
            0x3F, 0x00, 0x00, 0x00
        };
    }

    InputBinding wildcard(InputType type)
    {
        return InputBinding { .backend = type };
    }

    // ------------------------------------------------------------------
    // Lifecycle
    // ------------------------------------------------------------------

    TEST(InputManagerTest, StartWithoutInputServiceStaysStopped)
    {
        ASSERT_FALSE(Registry::BackendRegistry::instance().has_service<Registry::Service::InputService>());
        InputManager manager;

        manager.start();

        EXPECT_FALSE(manager.is_running());
    }

    TEST(InputManagerTest, StartSucceedsOnceTheServiceAppears)
    {
        InputManager manager;
        manager.start();
        ASSERT_FALSE(manager.is_running());

        FakeInputService service;
        manager.start();

        EXPECT_TRUE(manager.is_running());
    }

    TEST(InputManagerTest, StartAndStopAreIdempotent)
    {
        FakeInputService service;
        InputManager manager;

        EXPECT_NO_THROW(manager.stop());

        manager.start();
        EXPECT_TRUE(manager.is_running());
        EXPECT_NO_THROW(manager.start());
        EXPECT_TRUE(manager.is_running());

        manager.stop();
        EXPECT_FALSE(manager.is_running());
        EXPECT_NO_THROW(manager.stop());
    }

    TEST(InputManagerTest, RestartsRepeatedlyAndKeepsDispatching)
    {
        FakeInputService service;
        InputManager manager;
        auto node = std::make_shared<RecordingNode>();
        manager.register_node(node, wildcard(InputType::HID));

        for (size_t round = 1; round <= 3; ++round) {
            manager.start();
            ASSERT_TRUE(manager.is_running()) << round;
            manager.enqueue(InputValue::make_scalar(1.0, 1, InputType::HID));
            ASSERT_TRUE(eventually([&] { return node->count() == round; })) << round;
            manager.stop();
            EXPECT_FALSE(manager.is_running()) << round;
        }
    }

    TEST(InputManagerTest, DestructionWhileRunningIsClean)
    {
        FakeInputService service;
        auto manager = std::make_unique<InputManager>();
        manager->start();
        ASSERT_TRUE(manager->is_running());

        EXPECT_NO_THROW(manager.reset());
    }

    // ------------------------------------------------------------------
    // Binding matching
    // ------------------------------------------------------------------

    struct DispatchCase {
        const char* name;
        InputBinding binding;
        InputValue value;
        bool delivered;
    };

    class InputManagerDispatchTest : public ::testing::TestWithParam<DispatchCase> { };

    TEST_P(InputManagerDispatchTest, DeliversOnlyValuesTheBindingAccepts)
    {
        const auto& param = GetParam();
        FakeInputService service;
        InputManager manager;
        auto target = std::make_shared<RecordingNode>();
        auto sentinel = std::make_shared<RecordingNode>();
        manager.start();
        ASSERT_TRUE(manager.is_running());
        manager.register_node(target, param.binding);
        manager.register_node(sentinel, wildcard(param.value.source_type));

        manager.enqueue(param.value);

        ASSERT_TRUE(eventually([&] { return sentinel->count() == 1; }));
        EXPECT_EQ(target->count(), param.delivered ? 1U : 0U);
    }

    INSTANTIATE_TEST_SUITE_P(
        InputBindings,
        InputManagerDispatchTest,
        ::testing::Values(
            DispatchCase { "HidWildcardMatchesHidValue", InputBinding::hid(), InputValue::make_bytes({ 1, 2 }, 3, InputType::HID), true },
            DispatchCase { "HidWildcardRejectsMidiValue", InputBinding::hid(), InputValue::make_midi(0x90, 60, 100, 1), false },
            DispatchCase { "DeviceBindingMatchesSameDevice", InputBinding::hid(7), InputValue::make_bytes({ 1 }, 7, InputType::HID), true },
            DispatchCase { "DeviceBindingRejectsOtherDevice", InputBinding::hid(7), InputValue::make_bytes({ 1 }, 8, InputType::HID), false },
            DispatchCase { "TabletBindingMatchesTabletValue", InputBinding::tablet(), InputValue::make_vector({ 0.5 }, 2, InputType::TABLET), true },
            DispatchCase { "TabletBindingRejectsHidValue", InputBinding::tablet(), InputValue::make_bytes({ 1 }, 2, InputType::HID), false },
            DispatchCase { "MidiChannelMatchesStatusNibble", InputBinding::midi(0, 3), InputValue::make_midi(0x93, 60, 100, 1), true },
            DispatchCase { "MidiChannelRejectsOtherChannel", InputBinding::midi(0, 3), InputValue::make_midi(0x94, 60, 100, 1), false },
            DispatchCase { "MidiCcMatchesNumber", InputBinding::midi_cc(7, std::nullopt, 0), InputValue::make_midi(0xB0, 7, 64, 1), true },
            DispatchCase { "MidiCcRejectsOtherNumber", InputBinding::midi_cc(7, std::nullopt, 0), InputValue::make_midi(0xB0, 8, 64, 1), false },
            DispatchCase { "MidiCcRejectsNoteOn", InputBinding::midi_cc(7, std::nullopt, 0), InputValue::make_midi(0x90, 7, 64, 1), false },
            DispatchCase { "NoteOnBindingMatchesNoteOn", InputBinding::midi_note_on(std::nullopt, 0), InputValue::make_midi(0x90, 60, 100, 1), true },
            DispatchCase { "NoteOnBindingRejectsNoteOff", InputBinding::midi_note_on(std::nullopt, 0), InputValue::make_midi(0x80, 60, 0, 1), false },
            DispatchCase { "NoteOnBindingFiltersChannel", InputBinding::midi_note_on(2, 0), InputValue::make_midi(0x91, 60, 100, 1), false },
            DispatchCase { "OscPrefixMatchesLongerAddress", InputBinding::osc("/mf/fader"), InputValue::make_osc("/mf/fader/1", { 0.5F }, 1), true },
            DispatchCase { "OscPrefixRejectsOtherAddress", InputBinding::osc("/mf/fader"), InputValue::make_osc("/mf/knob/1", { 0.5F }, 1), false },
            DispatchCase { "OscEmptyPatternMatchesAnyAddress", InputBinding::osc(), InputValue::make_osc("/anything", {}, 1), true }),
        [](const ::testing::TestParamInfo<DispatchCase>& info) { return std::string(info.param.name); });

    TEST(InputManagerTest, EveryMatchingNodeReceivesTheValue)
    {
        FakeInputService service;
        InputManager manager;
        auto first = std::make_shared<RecordingNode>();
        auto second = std::make_shared<RecordingNode>();
        manager.register_node(first, wildcard(InputType::HID));
        manager.register_node(second, wildcard(InputType::HID));
        manager.start();

        manager.enqueue(InputValue::make_scalar(0.25, 1, InputType::HID));

        ASSERT_TRUE(eventually([&] { return first->count() == 1 && second->count() == 1; }));
        EXPECT_DOUBLE_EQ(first->values().front().as_scalar(), 0.25);
    }

    // ------------------------------------------------------------------
    // Registration
    // ------------------------------------------------------------------

    TEST(InputManagerTest, RegisterNullNodeIsIgnored)
    {
        InputManager manager;

        manager.register_node(nullptr, wildcard(InputType::HID));
        manager.unregister_node(nullptr);

        EXPECT_EQ(manager.get_registered_node_count(), 0U);
    }

    TEST(InputManagerTest, RegisterAndUnregisterTrackNodeCount)
    {
        InputManager manager;
        auto first = std::make_shared<RecordingNode>();
        auto second = std::make_shared<RecordingNode>();

        manager.register_node(first, wildcard(InputType::HID));
        manager.register_node(second, wildcard(InputType::MIDI));
        EXPECT_EQ(manager.get_registered_node_count(), 2U);
        EXPECT_EQ(manager.get_nodes().size(), 2U);

        manager.unregister_node(first);
        EXPECT_EQ(manager.get_registered_node_count(), 1U);
        ASSERT_EQ(manager.get_nodes().size(), 1U);
        EXPECT_EQ(manager.get_nodes().front(), second);
    }

    TEST(InputManagerTest, UnregisterRemovesEveryBindingOfThatNode)
    {
        InputManager manager;
        auto node = std::make_shared<RecordingNode>();
        manager.register_node(node, wildcard(InputType::HID));
        manager.register_node(node, wildcard(InputType::MIDI));
        ASSERT_EQ(manager.get_registered_node_count(), 2U);

        manager.unregister_node(node);

        EXPECT_EQ(manager.get_registered_node_count(), 0U);
        EXPECT_TRUE(manager.get_nodes().empty());
    }

    TEST(InputManagerTest, UnregisterAllClearsEveryRegistration)
    {
        InputManager manager;
        manager.register_node(std::make_shared<RecordingNode>(), wildcard(InputType::HID));
        manager.register_node(std::make_shared<RecordingNode>(), wildcard(InputType::MIDI));

        manager.unregister_all_nodes();

        EXPECT_EQ(manager.get_registered_node_count(), 0U);
        EXPECT_TRUE(manager.get_nodes().empty());
    }

    TEST(InputManagerTest, RegisteredNodesAreKeptAliveUntilUnregistered)
    {
        InputManager manager;
        std::weak_ptr<RecordingNode> weak;
        {
            auto node = std::make_shared<RecordingNode>();
            weak = node;
            manager.register_node(node, wildcard(InputType::HID));
        }

        EXPECT_FALSE(weak.expired());

        manager.unregister_all_nodes();
        EXPECT_TRUE(weak.expired());
    }

    TEST(InputManagerTest, UnregisteredNodeStopsReceivingValues)
    {
        FakeInputService service;
        InputManager manager;
        auto removed = std::make_shared<RecordingNode>();
        auto kept = std::make_shared<RecordingNode>();
        manager.register_node(removed, wildcard(InputType::HID));
        manager.register_node(kept, wildcard(InputType::HID));
        manager.start();
        manager.unregister_node(removed);

        manager.enqueue(InputValue::make_scalar(1.0, 1, InputType::HID));

        ASSERT_TRUE(eventually([&] { return kept->count() == 1; }));
        EXPECT_EQ(removed->count(), 0U);
    }

    TEST(InputManagerTest, DeviceBindingOpensItsDeviceThroughTheService)
    {
        FakeInputService service;
        InputManager manager;
        manager.start();

        manager.register_node(std::make_shared<RecordingNode>(), InputBinding::hid(7));

        ASSERT_EQ(service.opened.size(), 1U);
        EXPECT_EQ(service.opened.front().first, InputType::HID);
        EXPECT_EQ(service.opened.front().second, 7U);
        EXPECT_EQ(manager.get_registered_node_count(), 1U);
    }

    TEST(InputManagerTest, WildcardBindingDoesNotOpenAnyDevice)
    {
        FakeInputService service;
        InputManager manager;
        manager.start();

        manager.register_node(std::make_shared<RecordingNode>(), InputBinding::hid());

        EXPECT_TRUE(service.opened.empty());
    }

    TEST(InputManagerTest, VendorProductBindingResolvesToTheMatchingDevice)
    {
        FakeInputService service;
        service.devices = {
            InputDeviceInfo { .id = 5, .backend_type = InputType::HID, .vendor_id = 0x1234, .product_id = 0x5678 },
            InputDeviceInfo { .id = 6, .backend_type = InputType::HID, .vendor_id = 0x1234, .product_id = 0x9999 }
        };
        InputManager manager;
        auto node = std::make_shared<RecordingNode>();
        auto sentinel = std::make_shared<RecordingNode>();
        manager.start();

        manager.register_node(node, InputBinding::hid_by_vid_pid(0x1234, 0x5678));
        manager.register_node(sentinel, wildcard(InputType::HID));
        manager.enqueue(InputValue::make_bytes({ 1 }, 6, InputType::HID));
        manager.enqueue(InputValue::make_bytes({ 2 }, 5, InputType::HID));

        ASSERT_TRUE(eventually([&] { return sentinel->count() == 2; }));
        ASSERT_EQ(node->count(), 1U);
        EXPECT_EQ(node->values().front().device_id, 5U);
        ASSERT_EQ(service.opened.size(), 1U);
        EXPECT_EQ(service.opened.front().second, 5U);
    }

    TEST(InputManagerTest, VendorOnlyBindingMatchesAnyProduct)
    {
        FakeInputService service;
        service.devices = {
            InputDeviceInfo { .id = 9, .backend_type = InputType::HID, .vendor_id = 0x1234, .product_id = 0x0001 }
        };
        InputManager manager;
        manager.start();
        InputBinding binding = InputBinding::hid_by_vid_pid(0x1234, 0);
        binding.hid_product_id.reset();

        manager.register_node(std::make_shared<RecordingNode>(), binding);

        EXPECT_EQ(manager.get_registered_node_count(), 1U);
        ASSERT_EQ(service.opened.size(), 1U);
        EXPECT_EQ(service.opened.front().second, 9U);
    }

    TEST(InputManagerTest, VendorProductBindingWithoutAMatchIsRefused)
    {
        FakeInputService service;
        service.devices = {
            InputDeviceInfo { .id = 5, .backend_type = InputType::HID, .vendor_id = 0x1111, .product_id = 0x2222 }
        };
        InputManager manager;
        manager.start();

        manager.register_node(std::make_shared<RecordingNode>(), InputBinding::hid_by_vid_pid(0x1234, 0x5678));

        EXPECT_EQ(manager.get_registered_node_count(), 0U);
        EXPECT_TRUE(service.opened.empty());
    }

    TEST(InputManagerTest, VendorProductBindingIgnoresDevicesOfOtherBackends)
    {
        FakeInputService service;
        service.devices = {
            InputDeviceInfo { .id = 5, .backend_type = InputType::MIDI, .vendor_id = 0x1234, .product_id = 0x5678 }
        };
        InputManager manager;
        manager.start();

        manager.register_node(std::make_shared<RecordingNode>(), InputBinding::hid_by_vid_pid(0x1234, 0x5678));

        EXPECT_EQ(manager.get_registered_node_count(), 0U);
    }

    // ------------------------------------------------------------------
    // Queue and statistics
    // ------------------------------------------------------------------

    TEST(InputManagerTest, BatchDeliversInOrder)
    {
        FakeInputService service;
        InputManager manager;
        auto node = std::make_shared<RecordingNode>();
        manager.register_node(node, wildcard(InputType::HID));
        manager.start();

        std::vector<InputValue> batch;
        for (int i = 1; i <= 5; ++i)
            batch.push_back(InputValue::make_scalar(static_cast<double>(i), 1, InputType::HID));
        manager.enqueue_batch(batch);

        ASSERT_TRUE(eventually([&] { return node->count() == 5; }));
        const auto values = node->values();
        for (size_t i = 0; i < values.size(); ++i)
            EXPECT_DOUBLE_EQ(values[i].as_scalar(), static_cast<double>(i + 1));
    }

    TEST(InputManagerTest, EmptyBatchIsIgnored)
    {
        InputManager manager;

        manager.enqueue_batch({});

        EXPECT_EQ(manager.get_queue_depth(), 0U);
    }

    TEST(InputManagerTest, ValuesQueuedBeforeStartAreDeliveredOnStart)
    {
        FakeInputService service;
        InputManager manager;
        auto node = std::make_shared<RecordingNode>();
        manager.register_node(node, wildcard(InputType::HID));

        for (int i = 0; i < 3; ++i)
            manager.enqueue(InputValue::make_scalar(1.0, 1, InputType::HID));
        EXPECT_EQ(manager.get_queue_depth(), 3U);
        EXPECT_EQ(node->count(), 0U);

        manager.start();

        ASSERT_TRUE(eventually([&] { return node->count() == 3; }));
        EXPECT_EQ(manager.get_queue_depth(), 0U);
    }

    TEST(InputManagerTest, EventsProcessedCountsEveryDispatchedValue)
    {
        FakeInputService service;
        InputManager manager;
        manager.start();
        EXPECT_EQ(manager.get_events_processed(), 0U);

        for (int i = 0; i < 10; ++i)
            manager.enqueue(InputValue::make_scalar(1.0, 1, InputType::HID));

        EXPECT_TRUE(eventually([&] { return manager.get_events_processed() == 10; }));
    }

    TEST(InputManagerTest, StopDeliversEverythingEnqueuedBeforeIt)
    {
        FakeInputService service;
        InputManager manager;
        auto node = std::make_shared<RecordingNode>();
        manager.register_node(node, wildcard(InputType::HID));
        manager.start();

        for (int i = 0; i < 100; ++i)
            manager.enqueue(InputValue::make_scalar(static_cast<double>(i), 1, InputType::HID));
        manager.stop();

        EXPECT_EQ(node->count(), 100U);
        EXPECT_EQ(manager.get_events_processed(), 100U);
    }

    TEST(InputManagerTest, OverflowKeepsOrderAndNeverExceedsQueueCapacity)
    {
        FakeInputService service;
        InputManager manager;
        auto node = std::make_shared<RecordingNode>();
        manager.register_node(node, wildcard(InputType::HID));
        constexpr int enqueued = 4096 + 20;

        for (int i = 0; i < enqueued; ++i)
            manager.enqueue(InputValue::make_scalar(static_cast<double>(i), 1, InputType::HID));

        const size_t depth = manager.get_queue_depth();
        EXPECT_GT(depth, 0U);
        EXPECT_LE(depth, 4096U);
        EXPECT_LT(depth, static_cast<size_t>(enqueued));

        manager.start();
        ASSERT_TRUE(eventually([&] { return node->count() == depth; }));

        const auto values = node->values();
        for (size_t i = 1; i < values.size(); ++i)
            EXPECT_LT(values[i - 1].as_scalar(), values[i].as_scalar());
    }

    // ------------------------------------------------------------------
    // OSC bridge
    // ------------------------------------------------------------------

    TEST(InputManagerOscTest, DisabledConfigOpensNoEndpoint)
    {
        FakeNetworkService network;
        InputManager manager;
        Core::OSCConfigInfo config = osc_config();
        config.enabled = false;

        manager.setup_osc_bridge(config);

        EXPECT_TRUE(network.opened.empty());
    }

    TEST(InputManagerOscTest, MissingNetworkServiceIsTolerated)
    {
        ASSERT_FALSE(Registry::BackendRegistry::instance().has_service<Registry::Service::NetworkService>());
        InputManager manager;

        EXPECT_NO_THROW(manager.setup_osc_bridge(osc_config()));
    }

    TEST(InputManagerOscTest, EnabledConfigOpensABidirectionalUdpEndpoint)
    {
        FakeNetworkService network;
        InputManager manager;

        manager.setup_osc_bridge(osc_config(9123));

        ASSERT_EQ(network.opened.size(), 1U);
        const auto& endpoint = network.opened.front();
        EXPECT_EQ(endpoint.transport, Core::NetworkTransport::UDP);
        EXPECT_EQ(endpoint.role, Core::EndpointRole::BIDIRECTIONAL);
        EXPECT_EQ(endpoint.local_port, 9123U);
        EXPECT_EQ(endpoint.remote_address, "10.0.0.7");
        EXPECT_EQ(endpoint.remote_port, 9124U);
        EXPECT_EQ(network.callbacks.count(network.next_endpoint_id), 1U);
    }

    TEST(InputManagerOscTest, FailedEndpointOpenRegistersNoReceiveCallback)
    {
        FakeNetworkService network;
        network.next_endpoint_id = 0;
        InputManager manager;

        manager.setup_osc_bridge(osc_config());

        EXPECT_EQ(network.opened.size(), 1U);
        EXPECT_TRUE(network.callbacks.empty());
    }

    TEST(InputManagerOscTest, ReceivedDatagramReachesBoundNodeAsOscValue)
    {
        FakeInputService service;
        FakeNetworkService network;
        InputManager manager;
        auto node = std::make_shared<RecordingNode>();
        manager.register_node(node, InputBinding::osc("/mf"));
        manager.start();
        manager.setup_osc_bridge(osc_config());

        network.deliver(network.next_endpoint_id, osc_float_datagram());

        ASSERT_TRUE(eventually([&] { return node->count() == 1; }));
        const auto value = node->values().front();
        ASSERT_EQ(value.type, InputValue::Type::OSC);
        EXPECT_EQ(value.source_type, InputType::OSC);
        EXPECT_EQ(value.as_osc().address, "/mf/test");
    }

    TEST(InputManagerOscTest, MalformedDatagramIsDropped)
    {
        FakeInputService service;
        FakeNetworkService network;
        InputManager manager;
        auto node = std::make_shared<RecordingNode>();
        manager.register_node(node, wildcard(InputType::OSC));
        manager.start();
        manager.setup_osc_bridge(osc_config());

        network.deliver(network.next_endpoint_id, { 'x', 'y', 'z', 'w', 0 });
        network.deliver(network.next_endpoint_id, osc_float_datagram());

        ASSERT_TRUE(eventually([&] { return node->count() == 1; }));
        EXPECT_EQ(node->values().front().as_osc().address, "/mf/test");
    }

} // namespace
} // namespace MayaFlux::Test
