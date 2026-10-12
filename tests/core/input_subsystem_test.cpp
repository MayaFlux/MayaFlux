#include "gtest/gtest.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "MayaFlux/Core/Input/InputManager.hpp"
#include "MayaFlux/Core/ProcessingArchitecture.hpp"
#include "MayaFlux/Core/Subsystems/InputSubsystem.hpp"
#include "MayaFlux/Nodes/Input/InputNode.hpp"
#include "MayaFlux/Registry/BackendRegistry.hpp"
#include "MayaFlux/Registry/Service/InputService.hpp"
#include "MayaFlux/Registry/Service/NetworkService.hpp"
#include "MayaFlux/Vruta/Scheduler.hpp"

#include "../support/input_test_support.hpp"

namespace MayaFlux::Test {
namespace {

    using namespace std::chrono_literals;
    using Core::InputBinding;
    using Core::InputDeviceInfo;
    using Core::InputManager;
    using Core::InputSubsystem;
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

    bool input_service_registered()
    {
        return Registry::BackendRegistry::instance().has_service<Registry::Service::InputService>();
    }

    struct BackendCounters {
        int initialize { 0 };
        int start { 0 };
        int stop { 0 };
        int shutdown { 0 };
        std::vector<uint32_t> opened;
        std::vector<uint32_t> closed;
    };

    class FakeBackend : public Core::IInputBackend {
    public:
        FakeBackend(InputType type, std::shared_ptr<BackendCounters> counters,
            std::vector<InputDeviceInfo> devices = {}, bool initialize_succeeds = true)
            : m_type(type)
            , m_counters(std::move(counters))
            , m_devices(std::move(devices))
            , m_initialize_succeeds(initialize_succeeds)
        {
        }

        bool initialize() override
        {
            ++m_counters->initialize;
            m_initialized = m_initialize_succeeds;
            return m_initialize_succeeds;
        }

        void start() override
        {
            ++m_counters->start;
            m_running = true;
        }

        void stop() override
        {
            ++m_counters->stop;
            m_running = false;
        }

        void shutdown() override
        {
            ++m_counters->shutdown;
            m_initialized = false;
            m_running = false;
        }

        [[nodiscard]] bool is_initialized() const override { return m_initialized; }
        [[nodiscard]] bool is_running() const override { return m_running; }
        [[nodiscard]] std::vector<InputDeviceInfo> get_devices() const override { return m_devices; }
        size_t refresh_devices() override { return m_devices.size(); }

        bool open_device(uint32_t device_id) override
        {
            m_counters->opened.push_back(device_id);
            return std::ranges::any_of(m_devices, [device_id](const auto& d) { return d.id == device_id; });
        }

        void close_device(uint32_t device_id) override { m_counters->closed.push_back(device_id); }
        [[nodiscard]] bool is_device_open(uint32_t) const override { return false; }
        [[nodiscard]] std::vector<uint32_t> get_open_devices() const override { return {}; }
        void set_input_callback(Core::InputCallback callback) override { m_input = std::move(callback); }
        void set_device_callback(Core::DeviceCallback) override { }
        [[nodiscard]] InputType get_type() const override { return m_type; }
        [[nodiscard]] std::string get_name() const override { return "Fake"; }
        [[nodiscard]] std::string get_version() const override { return "0"; }

        void emit(const InputValue& value)
        {
            if (m_input)
                m_input(value);
        }

    private:
        InputType m_type;
        std::shared_ptr<BackendCounters> m_counters;
        std::vector<InputDeviceInfo> m_devices;
        bool m_initialize_succeeds;
        bool m_initialized { false };
        bool m_running { false };
        Core::InputCallback m_input;
    };

    /**
     * @brief Owns an InputSubsystem together with the managers behind its processing handle.
     */
    class SubsystemRig {
    public:
        explicit SubsystemRig(Core::GlobalInputConfig input_config = {})
            : config(std::move(input_config))
            , manager(std::make_shared<InputManager>())
            , scheduler(std::make_shared<Vruta::TaskScheduler>(48000))
            , subsystem(config)
            , handle(nullptr, nullptr, scheduler, nullptr, manager, subsystem.get_tokens())
        {
        }

        SubsystemRig(const SubsystemRig&) = delete;
        SubsystemRig& operator=(const SubsystemRig&) = delete;
        SubsystemRig(SubsystemRig&&) = delete;
        SubsystemRig& operator=(SubsystemRig&&) = delete;

        ~SubsystemRig() { subsystem.shutdown(); }

        void initialize() { subsystem.initialize(handle); }

        FakeBackend* add_fake(InputType type, const std::shared_ptr<BackendCounters>& counters,
            std::vector<InputDeviceInfo> devices = {})
        {
            auto backend = std::make_unique<FakeBackend>(type, counters, std::move(devices));
            auto* raw = backend.get();
            return subsystem.add_backend(std::move(backend)) ? raw : nullptr;
        }

        Core::GlobalInputConfig config;
        std::shared_ptr<InputManager> manager;
        std::shared_ptr<Vruta::TaskScheduler> scheduler;
        InputSubsystem subsystem;
        Core::SubsystemProcessingHandle handle;
    };

    std::vector<InputDeviceInfo> pad_devices()
    {
        return {
            InputDeviceInfo { .id = 1, .name = "Pad A", .backend_type = InputType::HID, .vendor_id = 0x1111, .product_id = 0x2222 },
            InputDeviceInfo { .id = 2, .name = "Pad B", .backend_type = InputType::HID, .vendor_id = 0x3333, .product_id = 0x4444 }
        };
    }

    std::vector<InputDeviceInfo> midi_devices()
    {
        return {
            InputDeviceInfo { .id = 1, .name = "Keys", .backend_type = InputType::MIDI, .is_input = true }
        };
    }

    // ------------------------------------------------------------------
    // Construction and initialization
    // ------------------------------------------------------------------

    TEST(InputSubsystemTest, NewSubsystemIsIdleAndReportsItsTokens)
    {
        SubsystemRig rig;

        EXPECT_FALSE(rig.subsystem.is_ready());
        EXPECT_FALSE(rig.subsystem.is_running());
        EXPECT_EQ(rig.subsystem.get_type(), Core::SubsystemType::CUSTOM);
        EXPECT_EQ(rig.subsystem.get_processing_context_handle(), nullptr);
        EXPECT_EQ(rig.subsystem.get_tokens().Buffer, Buffers::ProcessingToken::INPUT_BACKEND);
        EXPECT_EQ(rig.subsystem.get_tokens().Node, Nodes::ProcessingToken::EVENT_RATE);
        EXPECT_EQ(rig.subsystem.get_tokens().Task, Vruta::ProcessingToken::EVENT_DRIVEN);
    }

    TEST(InputSubsystemTest, StartBeforeInitializeIsRejected)
    {
        SubsystemRig rig;

        rig.subsystem.start();

        EXPECT_FALSE(rig.subsystem.is_running());
        EXPECT_FALSE(rig.manager->is_running());
    }

    TEST(InputSubsystemTest, InitializeWithNoBackendsEnabledBecomesReadyAndRegistersTheService)
    {
        ASSERT_FALSE(input_service_registered());
        SubsystemRig rig;

        rig.initialize();

        EXPECT_TRUE(rig.subsystem.is_ready());
        EXPECT_FALSE(rig.subsystem.is_running());
        EXPECT_EQ(rig.subsystem.get_processing_context_handle(), &rig.handle);
        EXPECT_TRUE(rig.subsystem.get_backends().empty());
        EXPECT_TRUE(rig.subsystem.get_all_devices().empty());
        EXPECT_TRUE(input_service_registered());
    }

    TEST(InputSubsystemTest, HidConfigCreatesAHidBackendThatHonoursItsFilters)
    {
        Core::GlobalInputConfig config;
        config.hid.enabled = true;
        config.hid.filters.push_back(Core::HIDDeviceFilter::device(0xFFFF, 0xFFFF));
        SubsystemRig rig(config);

        rig.initialize();

        ASSERT_NE(rig.subsystem.get_backend(InputType::HID), nullptr);
        EXPECT_EQ(rig.subsystem.get_backend(InputType::HID)->get_type(), InputType::HID);
        EXPECT_TRUE(rig.subsystem.get_hid_devices().empty());
        EXPECT_EQ(rig.subsystem.get_backends().size(), 1U);
    }

    TEST(InputSubsystemTest, OscConfigOpensItsEndpointThroughTheNetworkService)
    {
        FakeNetworkService network;
        Core::GlobalInputConfig config;
        config.osc.enabled = true;
        config.osc.receive_port = 9321;
        SubsystemRig rig(config);

        rig.initialize();

        ASSERT_EQ(network.opened.size(), 1U);
        EXPECT_EQ(network.opened.front().local_port, 9321U);
    }

    // ------------------------------------------------------------------
    // Backend management
    // ------------------------------------------------------------------

    TEST(InputSubsystemTest, AddBackendRejectsNull)
    {
        SubsystemRig rig;
        rig.initialize();

        EXPECT_FALSE(rig.subsystem.add_backend(nullptr));
        EXPECT_TRUE(rig.subsystem.get_backends().empty());
    }

    TEST(InputSubsystemTest, AddBackendInitializesAndRegistersTheBackend)
    {
        SubsystemRig rig;
        rig.initialize();
        auto counters = std::make_shared<BackendCounters>();

        auto* backend = rig.add_fake(InputType::CUSTOM, counters);

        ASSERT_NE(backend, nullptr);
        EXPECT_EQ(counters->initialize, 1);
        EXPECT_EQ(rig.subsystem.get_backend(InputType::CUSTOM), backend);
        ASSERT_EQ(rig.subsystem.get_backends().size(), 1U);
        EXPECT_EQ(rig.subsystem.get_backends().front(), backend);
    }

    TEST(InputSubsystemTest, AddBackendRejectsADuplicateTypeWithoutInitializingIt)
    {
        SubsystemRig rig;
        rig.initialize();
        auto first_counters = std::make_shared<BackendCounters>();
        auto second_counters = std::make_shared<BackendCounters>();
        auto* first = rig.add_fake(InputType::CUSTOM, first_counters);
        ASSERT_NE(first, nullptr);

        EXPECT_EQ(rig.add_fake(InputType::CUSTOM, second_counters), nullptr);

        EXPECT_EQ(second_counters->initialize, 0);
        EXPECT_EQ(rig.subsystem.get_backend(InputType::CUSTOM), first);
        EXPECT_EQ(rig.subsystem.get_backends().size(), 1U);
    }

    TEST(InputSubsystemTest, AddBackendRejectsABackendThatFailsToInitialize)
    {
        SubsystemRig rig;
        rig.initialize();
        auto counters = std::make_shared<BackendCounters>();

        const bool added = rig.subsystem.add_backend(
            std::make_unique<FakeBackend>(InputType::CUSTOM, counters, std::vector<InputDeviceInfo> {}, false));

        EXPECT_FALSE(added);
        EXPECT_EQ(counters->initialize, 1);
        EXPECT_EQ(rig.subsystem.get_backend(InputType::CUSTOM), nullptr);
    }

    // ------------------------------------------------------------------
    // Lifecycle
    // ------------------------------------------------------------------

    TEST(InputSubsystemTest, StartStartsTheManagerAndEveryBackend)
    {
        SubsystemRig rig;
        rig.initialize();
        auto hid = std::make_shared<BackendCounters>();
        auto midi = std::make_shared<BackendCounters>();
        rig.add_fake(InputType::HID, hid);
        rig.add_fake(InputType::MIDI, midi);

        rig.subsystem.start();

        EXPECT_TRUE(rig.subsystem.is_running());
        EXPECT_TRUE(rig.manager->is_running());
        EXPECT_EQ(hid->start, 1);
        EXPECT_EQ(midi->start, 1);
    }

    TEST(InputSubsystemTest, StartIsIdempotent)
    {
        SubsystemRig rig;
        rig.initialize();
        auto counters = std::make_shared<BackendCounters>();
        rig.add_fake(InputType::HID, counters);

        rig.subsystem.start();
        rig.subsystem.start();

        EXPECT_EQ(counters->start, 1);
    }

    TEST(InputSubsystemTest, PauseStopsBackendsAndResumeStartsThemAgain)
    {
        SubsystemRig rig;
        rig.initialize();
        auto counters = std::make_shared<BackendCounters>();
        rig.add_fake(InputType::HID, counters);
        rig.subsystem.start();

        rig.subsystem.pause();
        EXPECT_FALSE(rig.subsystem.is_running());
        EXPECT_EQ(counters->stop, 1);

        rig.subsystem.resume();
        EXPECT_TRUE(rig.subsystem.is_running());
        EXPECT_EQ(counters->start, 2);
    }

    TEST(InputSubsystemTest, PauseAndResumeAreNoOpsInTheWrongState)
    {
        SubsystemRig rig;
        rig.initialize();
        auto counters = std::make_shared<BackendCounters>();
        rig.add_fake(InputType::HID, counters);

        rig.subsystem.pause();
        EXPECT_EQ(counters->stop, 0);

        rig.subsystem.start();
        rig.subsystem.resume();
        EXPECT_EQ(counters->start, 1);
    }

    TEST(InputSubsystemTest, StopStopsBackendsAndTheManager)
    {
        SubsystemRig rig;
        rig.initialize();
        auto counters = std::make_shared<BackendCounters>();
        rig.add_fake(InputType::HID, counters);
        rig.subsystem.start();

        rig.subsystem.stop();

        EXPECT_FALSE(rig.subsystem.is_running());
        EXPECT_FALSE(rig.manager->is_running());
        EXPECT_EQ(counters->stop, 1);

        rig.subsystem.stop();
        EXPECT_EQ(counters->stop, 1);
    }

    TEST(InputSubsystemTest, ShutdownReleasesBackendsNodesAndTheService)
    {
        SubsystemRig rig;
        rig.initialize();
        auto counters = std::make_shared<BackendCounters>();
        rig.add_fake(InputType::HID, counters);
        rig.manager->register_node(std::make_shared<RecordingNode>(), InputBinding::hid());
        rig.subsystem.start();
        ASSERT_EQ(rig.manager->get_registered_node_count(), 1U);

        rig.subsystem.shutdown();

        EXPECT_FALSE(rig.subsystem.is_ready());
        EXPECT_FALSE(rig.subsystem.is_running());
        EXPECT_FALSE(rig.manager->is_running());
        EXPECT_EQ(counters->stop, 1);
        EXPECT_EQ(counters->shutdown, 1);
        EXPECT_TRUE(rig.subsystem.get_backends().empty());
        EXPECT_EQ(rig.manager->get_registered_node_count(), 0U);
        EXPECT_FALSE(input_service_registered());
    }

    TEST(InputSubsystemTest, ShutdownIsIdempotent)
    {
        SubsystemRig rig;
        rig.initialize();
        auto counters = std::make_shared<BackendCounters>();
        rig.add_fake(InputType::HID, counters);

        rig.subsystem.shutdown();
        EXPECT_NO_THROW(rig.subsystem.shutdown());

        EXPECT_EQ(counters->shutdown, 1);
    }

    TEST(InputSubsystemTest, DestructionShutsTheSubsystemDown)
    {
        auto counters = std::make_shared<BackendCounters>();
        {
            SubsystemRig rig;
            rig.initialize();
            rig.add_fake(InputType::HID, counters);
            rig.subsystem.start();
            ASSERT_TRUE(input_service_registered());
        }

        EXPECT_EQ(counters->shutdown, 1);
        EXPECT_FALSE(input_service_registered());
    }

    TEST(InputSubsystemTest, ReinitializeAfterShutdownIsPossible)
    {
        SubsystemRig rig;
        rig.initialize();
        rig.subsystem.shutdown();
        ASSERT_FALSE(rig.subsystem.is_ready());

        rig.initialize();

        EXPECT_TRUE(rig.subsystem.is_ready());
        EXPECT_TRUE(input_service_registered());
    }

    // ------------------------------------------------------------------
    // Devices
    // ------------------------------------------------------------------

    TEST(InputSubsystemTest, DeviceQueriesAggregateAndFilterByBackend)
    {
        SubsystemRig rig;
        rig.initialize();
        rig.add_fake(InputType::HID, std::make_shared<BackendCounters>(), pad_devices());
        rig.add_fake(InputType::MIDI, std::make_shared<BackendCounters>(), midi_devices());

        EXPECT_EQ(rig.subsystem.get_all_devices().size(), 3U);
        EXPECT_EQ(rig.subsystem.get_hid_devices().size(), 2U);
        EXPECT_EQ(rig.subsystem.get_midi_devices().size(), 1U);
        EXPECT_TRUE(rig.subsystem.get_tablet_devices().empty());
    }

    TEST(InputSubsystemTest, GetDeviceInfoFindsDevicesPerBackend)
    {
        SubsystemRig rig;
        rig.initialize();
        rig.add_fake(InputType::HID, std::make_shared<BackendCounters>(), pad_devices());

        const auto info = rig.subsystem.get_device_info(InputType::HID, 2);

        ASSERT_TRUE(info.has_value());
        EXPECT_EQ(info->name, "Pad B");
        EXPECT_FALSE(rig.subsystem.get_device_info(InputType::HID, 99).has_value());
        EXPECT_FALSE(rig.subsystem.get_device_info(InputType::OSC, 1).has_value());
    }

    TEST(InputSubsystemTest, FindHidDeviceMatchesVendorAndProduct)
    {
        SubsystemRig rig;
        rig.initialize();
        rig.add_fake(InputType::HID, std::make_shared<BackendCounters>(), pad_devices());

        const auto found = rig.subsystem.find_hid_device(0x3333, 0x4444);

        ASSERT_TRUE(found.has_value());
        EXPECT_EQ(found->id, 2U);
        EXPECT_FALSE(rig.subsystem.find_hid_device(0x3333, 0x2222).has_value());
    }

    TEST(InputSubsystemTest, OpenAndCloseRouteToTheOwningBackend)
    {
        SubsystemRig rig;
        rig.initialize();
        auto hid = std::make_shared<BackendCounters>();
        auto midi = std::make_shared<BackendCounters>();
        rig.add_fake(InputType::HID, hid, pad_devices());
        rig.add_fake(InputType::MIDI, midi, midi_devices());

        EXPECT_TRUE(rig.subsystem.open_device(InputType::HID, 1));
        EXPECT_FALSE(rig.subsystem.open_device(InputType::HID, 99));
        rig.subsystem.close_device(InputType::MIDI, 1);

        EXPECT_EQ(hid->opened, (std::vector<uint32_t> { 1, 99 }));
        EXPECT_TRUE(midi->opened.empty());
        EXPECT_EQ(midi->closed, (std::vector<uint32_t> { 1 }));
        EXPECT_TRUE(hid->closed.empty());
    }

    TEST(InputSubsystemTest, OpenOnAnUnknownBackendFailsAndCloseIsIgnored)
    {
        SubsystemRig rig;
        rig.initialize();

        EXPECT_FALSE(rig.subsystem.open_device(InputType::OSC, 1));
        EXPECT_NO_THROW(rig.subsystem.close_device(InputType::OSC, 1));
    }

    TEST(InputSubsystemTest, RegisteredServiceRoutesToTheSubsystem)
    {
        SubsystemRig rig;
        rig.initialize();
        auto hid = std::make_shared<BackendCounters>();
        rig.add_fake(InputType::HID, hid, pad_devices());

        auto* service = Registry::BackendRegistry::instance().get_service<Registry::Service::InputService>();
        ASSERT_NE(service, nullptr);

        EXPECT_TRUE(service->open_device(InputType::HID, 2));
        service->close_device(InputType::HID, 2);
        EXPECT_EQ(hid->opened, (std::vector<uint32_t> { 2 }));
        EXPECT_EQ(hid->closed, (std::vector<uint32_t> { 2 }));
    }

    // ------------------------------------------------------------------
    // Input flow
    // ------------------------------------------------------------------

    TEST(InputSubsystemTest, BackendInputReachesNodesRegisteredWithTheManager)
    {
        SubsystemRig rig;
        rig.initialize();
        auto* backend = rig.add_fake(InputType::HID, std::make_shared<BackendCounters>(), pad_devices());
        ASSERT_NE(backend, nullptr);
        auto node = std::make_shared<RecordingNode>();
        rig.manager->register_node(node, InputBinding::hid());
        rig.subsystem.start();

        backend->emit(InputValue::make_bytes({ 1, 2, 3 }, 1, InputType::HID));

        ASSERT_TRUE(eventually([&] { return node->count() == 1; }));
        EXPECT_TRUE(eventually([&] { return rig.manager->get_events_processed() == 1; }));
    }

} // namespace
} // namespace MayaFlux::Test
