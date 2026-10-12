#include "gtest/gtest.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include <asio/ip/udp.hpp>

#include "MayaFlux/Core/ProcessingArchitecture.hpp"
#include "MayaFlux/Core/Subsystems/NetworkSubsystem.hpp"
#include "MayaFlux/Portal/Network/Network.hpp"
#include "MayaFlux/Registry/BackendRegistry.hpp"
#include "MayaFlux/Registry/Service/NetworkService.hpp"
#include "MayaFlux/Vruta/Scheduler.hpp"

namespace MayaFlux::Test {
namespace {

    using namespace std::chrono_literals;
    using Core::EndpointInfo;
    using Core::EndpointRole;
    using Core::EndpointState;
    using Core::GlobalNetworkConfig;
    using Core::NetworkSubsystem;
    using Core::NetworkTransport;

    template <typename Predicate>
    bool eventually(Predicate predicate, std::chrono::milliseconds timeout = 3s)
    {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            if (predicate())
                return true;
            std::this_thread::sleep_for(1ms);
        }
        return predicate();
    }

    bool network_service_registered()
    {
        return Registry::BackendRegistry::instance().has_service<Registry::Service::NetworkService>();
    }

    uint16_t free_udp_port()
    {
        asio::io_context context;
        asio::ip::udp::socket socket(context, asio::ip::udp::endpoint(asio::ip::udp::v4(), 0));
        return socket.local_endpoint().port();
    }

    struct NetCounters {
        int initialize { 0 };
        int start { 0 };
        int stop { 0 };
        int shutdown { 0 };
        std::vector<EndpointInfo> opened;
        std::vector<uint64_t> closed;
        std::vector<std::pair<uint64_t, std::vector<uint8_t>>> sent;
        std::vector<std::pair<uint64_t, std::string>> sent_to;
    };

    class FakeNetworkBackend : public Core::INetworkBackend {
    public:
        FakeNetworkBackend(NetworkTransport transport, std::shared_ptr<NetCounters> counters,
            bool initialize_succeeds = true)
            : m_transport(transport)
            , m_counters(std::move(counters))
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
        [[nodiscard]] NetworkTransport get_transport() const override { return m_transport; }
        [[nodiscard]] std::string get_name() const override { return "Fake"; }
        [[nodiscard]] std::string get_version() const override { return "0"; }

        uint64_t open_endpoint(const EndpointInfo& info) override
        {
            m_counters->opened.push_back(info);
            if (fail_open)
                return 0;
            EndpointInfo stored = info;
            stored.state = EndpointState::OPEN;
            m_endpoints[info.id] = stored;
            return info.id;
        }

        void close_endpoint(uint64_t endpoint_id) override
        {
            m_counters->closed.push_back(endpoint_id);
            m_endpoints.erase(endpoint_id);
        }

        [[nodiscard]] EndpointState get_endpoint_state(uint64_t endpoint_id) const override
        {
            const auto it = m_endpoints.find(endpoint_id);
            return it == m_endpoints.end() ? EndpointState::CLOSED : it->second.state;
        }

        [[nodiscard]] std::vector<EndpointInfo> get_endpoints() const override
        {
            std::vector<EndpointInfo> result;
            for (const auto& [id, info] : m_endpoints)
                result.push_back(info);
            return result;
        }

        bool send(uint64_t endpoint_id, const uint8_t* data, size_t size) override
        {
            m_counters->sent.emplace_back(endpoint_id, std::vector<uint8_t>(data, data + size));
            return m_endpoints.contains(endpoint_id);
        }

        bool send_to(uint64_t endpoint_id, const uint8_t*, size_t,
            const std::string& address, uint16_t port) override
        {
            m_counters->sent_to.emplace_back(endpoint_id, address + ":" + std::to_string(port));
            return m_endpoints.contains(endpoint_id);
        }

        void set_receive_callback(Core::NetworkReceiveCallback callback) override { m_receive = std::move(callback); }
        void set_state_callback(Core::EndpointStateCallback) override { }

        void emit(uint64_t endpoint_id, const std::vector<uint8_t>& bytes, std::string_view sender)
        {
            if (m_receive)
                m_receive(endpoint_id, bytes.data(), bytes.size(), sender);
        }

        bool fail_open { false };

    private:
        NetworkTransport m_transport;
        std::shared_ptr<NetCounters> m_counters;
        bool m_initialize_succeeds;
        bool m_initialized { false };
        bool m_running { false };
        std::unordered_map<uint64_t, EndpointInfo> m_endpoints;
        Core::NetworkReceiveCallback m_receive;
    };

    class SubsystemRig {
    public:
        explicit SubsystemRig(const GlobalNetworkConfig& network_config = {})
            : scheduler(std::make_shared<Vruta::TaskScheduler>(48000))
            , subsystem(network_config)
            , handle(nullptr, nullptr, scheduler, nullptr, nullptr, subsystem.get_tokens())
        {
        }

        SubsystemRig(const SubsystemRig&) = delete;
        SubsystemRig& operator=(const SubsystemRig&) = delete;
        SubsystemRig(SubsystemRig&&) = delete;
        SubsystemRig& operator=(SubsystemRig&&) = delete;

        ~SubsystemRig() { subsystem.shutdown(); }

        void initialize() { subsystem.initialize(handle); }

        FakeNetworkBackend* add_fake(NetworkTransport transport, const std::shared_ptr<NetCounters>& counters)
        {
            auto backend = std::make_unique<FakeNetworkBackend>(transport, counters);
            auto* raw = backend.get();
            return subsystem.add_backend(std::move(backend)) ? raw : nullptr;
        }

        [[nodiscard]] static Registry::Service::NetworkService* service()
        {
            return Registry::BackendRegistry::instance().get_service<Registry::Service::NetworkService>();
        }

        std::shared_ptr<Vruta::TaskScheduler> scheduler;
        NetworkSubsystem subsystem;
        Core::SubsystemProcessingHandle handle;
    };

    EndpointInfo info_for(NetworkTransport transport, EndpointRole role = EndpointRole::BIDIRECTIONAL)
    {
        EndpointInfo info;
        info.transport = transport;
        info.role = role;
        return info;
    }

    struct Inbox {
        void push(std::vector<uint8_t> bytes)
        {
            std::scoped_lock lock(mutex);
            items.push_back(std::move(bytes));
        }

        [[nodiscard]] size_t size()
        {
            std::scoped_lock lock(mutex);
            return items.size();
        }

        std::mutex mutex;
        std::vector<std::vector<uint8_t>> items;
    };

    // ------------------------------------------------------------------
    // Construction and initialization
    // ------------------------------------------------------------------

    TEST(NetworkSubsystemTest, NewSubsystemIsIdleAndReportsItsTokens)
    {
        SubsystemRig rig;

        EXPECT_FALSE(rig.subsystem.is_ready());
        EXPECT_FALSE(rig.subsystem.is_running());
        EXPECT_EQ(rig.subsystem.get_type(), Core::SubsystemType::NETWORK);
        EXPECT_EQ(rig.subsystem.get_processing_context_handle(), nullptr);
        EXPECT_EQ(rig.subsystem.get_tokens().Buffer, Buffers::ProcessingToken::EVENT_RATE);
        EXPECT_EQ(rig.subsystem.get_tokens().Node, Nodes::ProcessingToken::EVENT_RATE);
        EXPECT_EQ(rig.subsystem.get_tokens().Task, Vruta::ProcessingToken::EVENT_DRIVEN);
    }

    TEST(NetworkSubsystemTest, StartBeforeInitializeIsRejected)
    {
        SubsystemRig rig;

        rig.subsystem.start();

        EXPECT_FALSE(rig.subsystem.is_running());
    }

    TEST(NetworkSubsystemTest, InitializeRegistersTheServiceAndTheNetworkPortal)
    {
        ASSERT_FALSE(network_service_registered());
        ASSERT_FALSE(Portal::Network::is_initialized());
        SubsystemRig rig;

        rig.initialize();

        EXPECT_TRUE(rig.subsystem.is_ready());
        EXPECT_FALSE(rig.subsystem.is_running());
        EXPECT_EQ(rig.subsystem.get_processing_context_handle(), &rig.handle);
        EXPECT_TRUE(network_service_registered());
        EXPECT_TRUE(Portal::Network::is_initialized());
        EXPECT_TRUE(rig.subsystem.get_backends().empty());
    }

    TEST(NetworkSubsystemTest, ConfigFlagsCreateTheMatchingBackends)
    {
        GlobalNetworkConfig config;
        config.udp.enabled = true;
        config.tcp.enabled = true;
        SubsystemRig rig(config);

        rig.initialize();

        EXPECT_EQ(rig.subsystem.get_backends().size(), 2U);
        ASSERT_NE(rig.subsystem.get_backend(NetworkTransport::UDP), nullptr);
        ASSERT_NE(rig.subsystem.get_backend(NetworkTransport::TCP), nullptr);
        EXPECT_EQ(rig.subsystem.get_backend(NetworkTransport::UDP)->get_transport(), NetworkTransport::UDP);
        EXPECT_EQ(rig.subsystem.get_backend(NetworkTransport::TCP)->get_transport(), NetworkTransport::TCP);
    }

    TEST(NetworkSubsystemTest, SharedMemoryConfigCreatesNoBackend)
    {
        GlobalNetworkConfig config;
        config.shared_memory.enabled = true;
        SubsystemRig rig(config);

        rig.initialize();

        EXPECT_TRUE(rig.subsystem.is_ready());
        EXPECT_TRUE(rig.subsystem.get_backends().empty());
        EXPECT_EQ(rig.subsystem.get_backend(NetworkTransport::SHARED_MEMORY), nullptr);
    }

    // ------------------------------------------------------------------
    // Backend management
    // ------------------------------------------------------------------

    TEST(NetworkSubsystemTest, AddBackendRejectsNull)
    {
        SubsystemRig rig;
        rig.initialize();

        EXPECT_FALSE(rig.subsystem.add_backend(nullptr));
        EXPECT_TRUE(rig.subsystem.get_backends().empty());
    }

    TEST(NetworkSubsystemTest, AddBackendInitializesAndRegistersTheBackend)
    {
        SubsystemRig rig;
        rig.initialize();
        auto counters = std::make_shared<NetCounters>();

        auto* backend = rig.add_fake(NetworkTransport::UDP, counters);

        ASSERT_NE(backend, nullptr);
        EXPECT_EQ(counters->initialize, 1);
        EXPECT_EQ(rig.subsystem.get_backend(NetworkTransport::UDP), backend);
        ASSERT_EQ(rig.subsystem.get_backends().size(), 1U);
        EXPECT_EQ(rig.subsystem.get_backends().front(), backend);
    }

    TEST(NetworkSubsystemTest, AddBackendRejectsADuplicateTransportWithoutInitializingIt)
    {
        SubsystemRig rig;
        rig.initialize();
        auto first_counters = std::make_shared<NetCounters>();
        auto second_counters = std::make_shared<NetCounters>();
        auto* first = rig.add_fake(NetworkTransport::UDP, first_counters);
        ASSERT_NE(first, nullptr);

        EXPECT_EQ(rig.add_fake(NetworkTransport::UDP, second_counters), nullptr);

        EXPECT_EQ(second_counters->initialize, 0);
        EXPECT_EQ(rig.subsystem.get_backend(NetworkTransport::UDP), first);
    }

    TEST(NetworkSubsystemTest, AddBackendRejectsABackendThatFailsToInitialize)
    {
        SubsystemRig rig;
        rig.initialize();
        auto counters = std::make_shared<NetCounters>();

        const bool added = rig.subsystem.add_backend(
            std::make_unique<FakeNetworkBackend>(NetworkTransport::UDP, counters, false));

        EXPECT_FALSE(added);
        EXPECT_EQ(counters->initialize, 1);
        EXPECT_EQ(rig.subsystem.get_backend(NetworkTransport::UDP), nullptr);
    }

    // ------------------------------------------------------------------
    // Lifecycle
    // ------------------------------------------------------------------

    TEST(NetworkSubsystemTest, StartAndStopDriveEveryBackend)
    {
        SubsystemRig rig;
        rig.initialize();
        auto udp = std::make_shared<NetCounters>();
        auto tcp = std::make_shared<NetCounters>();
        rig.add_fake(NetworkTransport::UDP, udp);
        rig.add_fake(NetworkTransport::TCP, tcp);

        rig.subsystem.start();
        EXPECT_TRUE(rig.subsystem.is_running());
        EXPECT_EQ(udp->start, 1);
        EXPECT_EQ(tcp->start, 1);

        rig.subsystem.stop();
        EXPECT_FALSE(rig.subsystem.is_running());
        EXPECT_EQ(udp->stop, 1);
        EXPECT_EQ(tcp->stop, 1);
    }

    TEST(NetworkSubsystemTest, StartAndStopAreIdempotent)
    {
        SubsystemRig rig;
        rig.initialize();
        auto counters = std::make_shared<NetCounters>();
        rig.add_fake(NetworkTransport::UDP, counters);

        EXPECT_NO_THROW(rig.subsystem.stop());
        EXPECT_EQ(counters->stop, 0);

        rig.subsystem.start();
        rig.subsystem.start();
        EXPECT_EQ(counters->start, 1);

        rig.subsystem.stop();
        rig.subsystem.stop();
        EXPECT_EQ(counters->stop, 1);
    }

    TEST(NetworkSubsystemTest, PauseStopsAndResumeStartsAgain)
    {
        SubsystemRig rig;
        rig.initialize();
        auto counters = std::make_shared<NetCounters>();
        rig.add_fake(NetworkTransport::UDP, counters);
        rig.subsystem.start();

        rig.subsystem.pause();
        EXPECT_FALSE(rig.subsystem.is_running());
        EXPECT_EQ(counters->stop, 1);

        rig.subsystem.resume();
        EXPECT_TRUE(rig.subsystem.is_running());
        EXPECT_EQ(counters->start, 2);
    }

    TEST(NetworkSubsystemTest, ShutdownReleasesBackendsServiceAndPortal)
    {
        SubsystemRig rig;
        rig.initialize();
        auto counters = std::make_shared<NetCounters>();
        rig.add_fake(NetworkTransport::UDP, counters);
        rig.subsystem.start();

        rig.subsystem.shutdown();

        EXPECT_FALSE(rig.subsystem.is_ready());
        EXPECT_FALSE(rig.subsystem.is_running());
        EXPECT_EQ(counters->stop, 1);
        EXPECT_EQ(counters->shutdown, 1);
        EXPECT_TRUE(rig.subsystem.get_backends().empty());
        EXPECT_FALSE(network_service_registered());
        EXPECT_FALSE(Portal::Network::is_initialized());
    }

    TEST(NetworkSubsystemTest, ShutdownIsIdempotentAndDestructionShutsDown)
    {
        auto counters = std::make_shared<NetCounters>();
        {
            SubsystemRig rig;
            rig.initialize();
            rig.add_fake(NetworkTransport::UDP, counters);
            rig.subsystem.shutdown();
            EXPECT_NO_THROW(rig.subsystem.shutdown());
        }

        EXPECT_EQ(counters->shutdown, 1);
        EXPECT_FALSE(network_service_registered());
    }

    TEST(NetworkSubsystemTest, ReinitializeAfterShutdownIsPossible)
    {
        SubsystemRig rig;
        rig.initialize();
        rig.subsystem.shutdown();
        ASSERT_FALSE(rig.subsystem.is_ready());

        rig.initialize();

        EXPECT_TRUE(rig.subsystem.is_ready());
        EXPECT_TRUE(network_service_registered());
    }

    // ------------------------------------------------------------------
    // Endpoint routing
    // ------------------------------------------------------------------

    TEST(NetworkSubsystemTest, OpenEndpointAssignsDistinctIdsAndRoutesToTheTransportBackend)
    {
        SubsystemRig rig;
        rig.initialize();
        auto udp = std::make_shared<NetCounters>();
        auto tcp = std::make_shared<NetCounters>();
        rig.add_fake(NetworkTransport::UDP, udp);
        rig.add_fake(NetworkTransport::TCP, tcp);

        const uint64_t first = rig.subsystem.open_endpoint(info_for(NetworkTransport::UDP));
        const uint64_t second = rig.subsystem.open_endpoint(info_for(NetworkTransport::TCP));
        const uint64_t third = rig.subsystem.open_endpoint(info_for(NetworkTransport::UDP));

        EXPECT_NE(first, 0U);
        EXPECT_NE(second, 0U);
        EXPECT_NE(third, 0U);
        EXPECT_NE(first, second);
        EXPECT_NE(first, third);
        ASSERT_EQ(udp->opened.size(), 2U);
        ASSERT_EQ(tcp->opened.size(), 1U);
        EXPECT_EQ(udp->opened[0].id, first);
        EXPECT_EQ(udp->opened[1].id, third);
        EXPECT_EQ(tcp->opened[0].id, second);
    }

    TEST(NetworkSubsystemTest, OpenEndpointWithoutABackendForTheTransportFails)
    {
        SubsystemRig rig;
        rig.initialize();
        auto udp = std::make_shared<NetCounters>();
        rig.add_fake(NetworkTransport::UDP, udp);

        EXPECT_EQ(rig.subsystem.open_endpoint(info_for(NetworkTransport::TCP)), 0U);
        EXPECT_TRUE(udp->opened.empty());
    }

    TEST(NetworkSubsystemTest, OpenEndpointThatTheBackendRefusesIsNotRouted)
    {
        SubsystemRig rig;
        rig.initialize();
        auto counters = std::make_shared<NetCounters>();
        auto* backend = rig.add_fake(NetworkTransport::UDP, counters);
        ASSERT_NE(backend, nullptr);
        backend->fail_open = true;
        const std::vector<uint8_t> data { 1 };

        EXPECT_EQ(rig.subsystem.open_endpoint(info_for(NetworkTransport::UDP)), 0U);

        ASSERT_EQ(counters->opened.size(), 1U);
        EXPECT_FALSE(rig.subsystem.send(counters->opened.front().id, data.data(), data.size()));
        EXPECT_EQ(rig.subsystem.get_endpoint_state(counters->opened.front().id), EndpointState::CLOSED);
    }

    TEST(NetworkSubsystemTest, SendAndSendToReachTheOwningBackend)
    {
        SubsystemRig rig;
        rig.initialize();
        auto udp = std::make_shared<NetCounters>();
        auto tcp = std::make_shared<NetCounters>();
        rig.add_fake(NetworkTransport::UDP, udp);
        rig.add_fake(NetworkTransport::TCP, tcp);
        const uint64_t id = rig.subsystem.open_endpoint(info_for(NetworkTransport::TCP));
        const std::vector<uint8_t> data { 4, 5, 6 };

        EXPECT_TRUE(rig.subsystem.send(id, data.data(), data.size()));
        EXPECT_TRUE(rig.subsystem.send_to(id, data.data(), data.size(), "10.0.0.2", 7000));

        ASSERT_EQ(tcp->sent.size(), 1U);
        EXPECT_EQ(tcp->sent.front().first, id);
        EXPECT_EQ(tcp->sent.front().second, data);
        ASSERT_EQ(tcp->sent_to.size(), 1U);
        EXPECT_EQ(tcp->sent_to.front().second, "10.0.0.2:7000");
        EXPECT_TRUE(udp->sent.empty());
        EXPECT_TRUE(udp->sent_to.empty());
    }

    TEST(NetworkSubsystemTest, UnknownEndpointIsClosedAndRejectsTraffic)
    {
        SubsystemRig rig;
        rig.initialize();
        auto counters = std::make_shared<NetCounters>();
        rig.add_fake(NetworkTransport::UDP, counters);
        const std::vector<uint8_t> data { 1 };

        EXPECT_EQ(rig.subsystem.get_endpoint_state(999), EndpointState::CLOSED);
        EXPECT_FALSE(rig.subsystem.send(999, data.data(), data.size()));
        EXPECT_FALSE(rig.subsystem.send_to(999, data.data(), data.size(), "10.0.0.2", 7000));
        EXPECT_NO_THROW(rig.subsystem.close_endpoint(999));
        EXPECT_TRUE(counters->closed.empty());
    }

    TEST(NetworkSubsystemTest, CloseEndpointForgetsTheRoute)
    {
        SubsystemRig rig;
        rig.initialize();
        auto counters = std::make_shared<NetCounters>();
        rig.add_fake(NetworkTransport::UDP, counters);
        const uint64_t id = rig.subsystem.open_endpoint(info_for(NetworkTransport::UDP));
        const std::vector<uint8_t> data { 1 };
        ASSERT_EQ(rig.subsystem.get_endpoint_state(id), EndpointState::OPEN);

        rig.subsystem.close_endpoint(id);

        EXPECT_EQ(counters->closed, (std::vector<uint64_t> { id }));
        EXPECT_EQ(rig.subsystem.get_endpoint_state(id), EndpointState::CLOSED);
        EXPECT_FALSE(rig.subsystem.send(id, data.data(), data.size()));

        rig.subsystem.close_endpoint(id);
        EXPECT_EQ(counters->closed.size(), 1U);
    }

    TEST(NetworkSubsystemTest, GetAllEndpointsAggregatesEveryBackend)
    {
        SubsystemRig rig;
        rig.initialize();
        rig.add_fake(NetworkTransport::UDP, std::make_shared<NetCounters>());
        rig.add_fake(NetworkTransport::TCP, std::make_shared<NetCounters>());
        const uint64_t udp_id = rig.subsystem.open_endpoint(info_for(NetworkTransport::UDP));
        const uint64_t tcp_id = rig.subsystem.open_endpoint(info_for(NetworkTransport::TCP));

        const auto endpoints = rig.subsystem.get_all_endpoints();

        ASSERT_EQ(endpoints.size(), 2U);
        const bool has_udp = std::ranges::any_of(endpoints, [&](const auto& e) { return e.id == udp_id; });
        const bool has_tcp = std::ranges::any_of(endpoints, [&](const auto& e) { return e.id == tcp_id; });
        EXPECT_TRUE(has_udp);
        EXPECT_TRUE(has_tcp);
    }

    // ------------------------------------------------------------------
    // Receive dispatch
    // ------------------------------------------------------------------

    TEST(NetworkSubsystemTest, ReceiveCallbackIsDispatchedPerEndpoint)
    {
        SubsystemRig rig;
        rig.initialize();
        auto* backend = rig.add_fake(NetworkTransport::UDP, std::make_shared<NetCounters>());
        ASSERT_NE(backend, nullptr);
        const uint64_t watched = rig.subsystem.open_endpoint(info_for(NetworkTransport::UDP));
        const uint64_t other = rig.subsystem.open_endpoint(info_for(NetworkTransport::UDP));
        uint64_t seen_id = 0;
        std::vector<uint8_t> seen_bytes;
        std::string seen_sender;
        int calls = 0;
        rig.subsystem.set_endpoint_receive_callback(watched,
            [&](uint64_t id, const uint8_t* data, size_t size, std::string_view sender) {
                ++calls;
                seen_id = id;
                seen_bytes.assign(data, data + size);
                seen_sender = std::string(sender);
            });

        backend->emit(other, { 9 }, "9.9.9.9:1");
        EXPECT_EQ(calls, 0);

        backend->emit(watched, { 1, 2, 3 }, "1.2.3.4:5");

        EXPECT_EQ(calls, 1);
        EXPECT_EQ(seen_id, watched);
        EXPECT_EQ(seen_bytes, (std::vector<uint8_t> { 1, 2, 3 }));
        EXPECT_EQ(seen_sender, "1.2.3.4:5");
    }

    TEST(NetworkSubsystemTest, ReplacingTheReceiveCallbackRedirectsDelivery)
    {
        SubsystemRig rig;
        rig.initialize();
        auto* backend = rig.add_fake(NetworkTransport::UDP, std::make_shared<NetCounters>());
        ASSERT_NE(backend, nullptr);
        const uint64_t id = rig.subsystem.open_endpoint(info_for(NetworkTransport::UDP));
        int first = 0;
        int second = 0;
        rig.subsystem.set_endpoint_receive_callback(id,
            [&](uint64_t, const uint8_t*, size_t, std::string_view) { ++first; });
        rig.subsystem.set_endpoint_receive_callback(id,
            [&](uint64_t, const uint8_t*, size_t, std::string_view) { ++second; });

        backend->emit(id, { 1 }, "1.1.1.1:1");

        EXPECT_EQ(first, 0);
        EXPECT_EQ(second, 1);
    }

    TEST(NetworkSubsystemTest, ClosedEndpointNoLongerReceivesCallbacks)
    {
        SubsystemRig rig;
        rig.initialize();
        auto* backend = rig.add_fake(NetworkTransport::UDP, std::make_shared<NetCounters>());
        ASSERT_NE(backend, nullptr);
        const uint64_t id = rig.subsystem.open_endpoint(info_for(NetworkTransport::UDP));
        int calls = 0;
        rig.subsystem.set_endpoint_receive_callback(id,
            [&](uint64_t, const uint8_t*, size_t, std::string_view) { ++calls; });
        backend->emit(id, { 1 }, "1.1.1.1:1");
        ASSERT_EQ(calls, 1);

        rig.subsystem.close_endpoint(id);
        backend->emit(id, { 1 }, "1.1.1.1:1");

        EXPECT_EQ(calls, 1);
    }

    // ------------------------------------------------------------------
    // Registered service
    // ------------------------------------------------------------------

    TEST(NetworkSubsystemTest, RegisteredServiceRoutesEveryOperationToTheSubsystem)
    {
        SubsystemRig rig;
        rig.initialize();
        auto counters = std::make_shared<NetCounters>();
        auto* backend = rig.add_fake(NetworkTransport::UDP, counters);
        ASSERT_NE(backend, nullptr);
        auto* service = SubsystemRig::service();
        ASSERT_NE(service, nullptr);
        const std::vector<uint8_t> data { 8, 9 };

        const uint64_t id = service->open_endpoint(info_for(NetworkTransport::UDP));
        ASSERT_NE(id, 0U);
        EXPECT_EQ(service->get_endpoint_state(id), EndpointState::OPEN);
        EXPECT_EQ(service->get_all_endpoints().size(), 1U);
        EXPECT_TRUE(service->send(id, data.data(), data.size()));
        EXPECT_TRUE(service->send_to(id, data.data(), data.size(), "10.1.1.1", 4000));

        int calls = 0;
        service->set_endpoint_receive_callback(id,
            [&](uint64_t, const uint8_t*, size_t, std::string_view) { ++calls; });
        backend->emit(id, data, "2.2.2.2:2");
        EXPECT_EQ(calls, 1);

        service->close_endpoint(id);
        EXPECT_EQ(counters->closed, (std::vector<uint64_t> { id }));
        EXPECT_EQ(service->get_endpoint_state(id), EndpointState::CLOSED);
    }

    // ------------------------------------------------------------------
    // Real UDP loopback through the service
    // ------------------------------------------------------------------

    TEST(NetworkSubsystemTest, UdpLoopbackThroughTheRegisteredService)
    {
        Inbox inbox;
        SubsystemRig rig(GlobalNetworkConfig::with_udp());
        rig.initialize();
        rig.subsystem.start();
        auto* service = SubsystemRig::service();
        ASSERT_NE(service, nullptr);
        const uint16_t port = free_udp_port();

        EndpointInfo receiver = info_for(NetworkTransport::UDP, EndpointRole::RECEIVE);
        receiver.local_port = port;
        const uint64_t receiver_id = service->open_endpoint(receiver);
        ASSERT_NE(receiver_id, 0U);
        service->set_endpoint_receive_callback(receiver_id,
            [&inbox](uint64_t, const uint8_t* data, size_t size, std::string_view) {
                inbox.push(std::vector<uint8_t>(data, data + size));
            });

        EndpointInfo sender = info_for(NetworkTransport::UDP, EndpointRole::SEND);
        sender.remote_address = "127.0.0.1";
        sender.remote_port = port;
        const uint64_t sender_id = service->open_endpoint(sender);
        ASSERT_NE(sender_id, 0U);
        const std::vector<uint8_t> data { 0xCA, 0xFE };

        ASSERT_TRUE(service->send(sender_id, data.data(), data.size()));

        ASSERT_TRUE(eventually([&] { return inbox.size() == 1; }));
        EXPECT_EQ(inbox.items.front(), data);
        EXPECT_EQ(service->get_endpoint_state(receiver_id), EndpointState::OPEN);
    }

    TEST(NetworkSubsystemTest, UdpKeepsDeliveringAfterPauseAndResume)
    {
        Inbox inbox;
        SubsystemRig rig(GlobalNetworkConfig::with_udp());
        rig.initialize();
        rig.subsystem.start();
        auto* service = SubsystemRig::service();
        ASSERT_NE(service, nullptr);
        const uint16_t port = free_udp_port();

        EndpointInfo receiver = info_for(NetworkTransport::UDP, EndpointRole::RECEIVE);
        receiver.local_port = port;
        const uint64_t receiver_id = service->open_endpoint(receiver);
        ASSERT_NE(receiver_id, 0U);
        service->set_endpoint_receive_callback(receiver_id,
            [&inbox](uint64_t, const uint8_t* data, size_t size, std::string_view) {
                inbox.push(std::vector<uint8_t>(data, data + size));
            });
        EndpointInfo sender = info_for(NetworkTransport::UDP, EndpointRole::SEND);
        sender.remote_address = "127.0.0.1";
        sender.remote_port = port;
        const uint64_t sender_id = service->open_endpoint(sender);
        ASSERT_NE(sender_id, 0U);
        const std::vector<uint8_t> data { 1 };

        rig.subsystem.pause();
        rig.subsystem.resume();
        ASSERT_TRUE(rig.subsystem.is_running());
        ASSERT_TRUE(service->send(sender_id, data.data(), data.size()));

        ASSERT_TRUE(eventually([&] { return inbox.size() == 1; }));
    }

} // namespace
} // namespace MayaFlux::Test
