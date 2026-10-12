#include "gtest/gtest.h"

#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "MayaFlux/Core/Backends/Network/UDPBackend.hpp"

namespace MayaFlux::Test {
namespace {

    using namespace std::chrono_literals;
    using Core::EndpointInfo;
    using Core::EndpointRole;
    using Core::EndpointState;
    using Core::UDPBackend;
    using Core::UDPBackendInfo;

    constexpr const char* k_loopback = "127.0.0.1";

    uint16_t free_port()
    {
        asio::io_context context;
        asio::ip::udp::socket socket(context, asio::ip::udp::endpoint(asio::ip::udp::v4(), 0));
        return socket.local_endpoint().port();
    }

    bool port_is_bindable(uint16_t port)
    {
        asio::io_context context;
        asio::ip::udp::socket socket(context);
        asio::error_code ec;
        if (socket.open(asio::ip::udp::v4(), ec))
            return false;
        return !socket.bind(asio::ip::udp::endpoint(asio::ip::udp::v4(), port), ec);
    }

    struct Received {
        uint64_t id {};
        std::vector<uint8_t> bytes;
        std::string sender;
    };

    class UdpRig {
    public:
        UdpRig()
            : backend(UDPBackendInfo {}, context)
        {
            backend.set_receive_callback(
                [this](uint64_t id, const uint8_t* data, size_t size, std::string_view sender) {
                    received.push_back({ .id = id, .bytes = std::vector<uint8_t>(data, data + size), .sender = std::string(sender) });
                });
        }

        UdpRig(const UdpRig&) = delete;
        UdpRig& operator=(const UdpRig&) = delete;
        UdpRig(UdpRig&&) = delete;
        UdpRig& operator=(UdpRig&&) = delete;

        ~UdpRig() { backend.shutdown(); }

        bool up()
        {
            if (!backend.initialize())
                return false;
            backend.start();
            return true;
        }

        template <typename Predicate>
        bool pump_until(Predicate predicate, std::chrono::milliseconds timeout = 2s)
        {
            const auto deadline = std::chrono::steady_clock::now() + timeout;
            while (std::chrono::steady_clock::now() < deadline) {
                poll();
                if (predicate())
                    return true;
                std::this_thread::sleep_for(1ms);
            }
            poll();
            return predicate();
        }

        void poll()
        {
            if (context.stopped())
                context.restart();
            context.poll();
        }

        [[nodiscard]] size_t count_for(uint64_t id) const
        {
            size_t count = 0;
            for (const auto& item : received) {
                if (item.id == id)
                    ++count;
            }
            return count;
        }

        asio::io_context context;
        UDPBackend backend;
        std::vector<Received> received;
    };

    EndpointInfo endpoint(uint64_t id, EndpointRole role, uint16_t local_port = 0,
        const std::string& remote_address = {}, uint16_t remote_port = 0)
    {
        EndpointInfo info;
        info.id = id;
        info.role = role;
        info.local_port = local_port;
        info.remote_address = remote_address;
        info.remote_port = remote_port;
        return info;
    }

    std::vector<uint8_t> payload(std::initializer_list<uint8_t> bytes)
    {
        return bytes;
    }

    // ------------------------------------------------------------------
    // Lifecycle
    // ------------------------------------------------------------------

    TEST(UDPBackendTest, ReportsIdentity)
    {
        UdpRig rig;

        EXPECT_EQ(rig.backend.get_transport(), Core::NetworkTransport::UDP);
        EXPECT_FALSE(rig.backend.get_name().empty());
        EXPECT_FALSE(rig.backend.get_version().empty());
        EXPECT_FALSE(rig.backend.is_initialized());
        EXPECT_FALSE(rig.backend.is_running());
    }

    TEST(UDPBackendTest, LifecycleCallsAreIdempotent)
    {
        UdpRig rig;

        ASSERT_TRUE(rig.backend.initialize());
        EXPECT_TRUE(rig.backend.initialize());
        EXPECT_TRUE(rig.backend.is_initialized());

        EXPECT_NO_THROW(rig.backend.stop());
        rig.backend.start();
        EXPECT_TRUE(rig.backend.is_running());
        EXPECT_NO_THROW(rig.backend.start());

        rig.backend.stop();
        EXPECT_FALSE(rig.backend.is_running());

        rig.backend.shutdown();
        EXPECT_FALSE(rig.backend.is_initialized());
        EXPECT_NO_THROW(rig.backend.shutdown());
    }

    TEST(UDPBackendTest, ShutdownStopsAndClosesEveryEndpoint)
    {
        UdpRig rig;
        ASSERT_TRUE(rig.up());
        const uint16_t port = free_port();
        ASSERT_EQ(rig.backend.open_endpoint(endpoint(1, EndpointRole::RECEIVE, port)), 1U);

        rig.backend.shutdown();

        EXPECT_FALSE(rig.backend.is_running());
        EXPECT_TRUE(rig.backend.get_endpoints().empty());
        EXPECT_TRUE(port_is_bindable(port));
    }

    // ------------------------------------------------------------------
    // Endpoints
    // ------------------------------------------------------------------

    TEST(UDPBackendTest, OpenReceiveEndpointReportsOpenAndItsPort)
    {
        UdpRig rig;
        ASSERT_TRUE(rig.up());
        const uint16_t port = free_port();

        const uint64_t id = rig.backend.open_endpoint(endpoint(7, EndpointRole::RECEIVE, port));

        EXPECT_EQ(id, 7U);
        EXPECT_EQ(rig.backend.get_endpoint_state(7), EndpointState::OPEN);
        const auto endpoints = rig.backend.get_endpoints();
        ASSERT_EQ(endpoints.size(), 1U);
        EXPECT_EQ(endpoints.front().id, 7U);
        EXPECT_EQ(endpoints.front().local_port, port);
        EXPECT_EQ(endpoints.front().state, EndpointState::OPEN);
    }

    TEST(UDPBackendTest, UnknownEndpointIsClosedAndRejectsTraffic)
    {
        UdpRig rig;
        ASSERT_TRUE(rig.up());
        const std::vector<uint8_t> data = payload({ 1, 2, 3 });

        EXPECT_EQ(rig.backend.get_endpoint_state(99), EndpointState::CLOSED);
        EXPECT_FALSE(rig.backend.send(99, data.data(), data.size()));
        EXPECT_FALSE(rig.backend.send_to(99, data.data(), data.size(), k_loopback, 9));
        EXPECT_NO_THROW(rig.backend.close_endpoint(99));
    }

    TEST(UDPBackendTest, InvalidRemoteAddressFailsToOpenAndLeavesNoEndpoint)
    {
        UdpRig rig;
        ASSERT_TRUE(rig.up());
        const uint16_t port = free_port();

        const uint64_t id = rig.backend.open_endpoint(
            endpoint(1, EndpointRole::BIDIRECTIONAL, port, "not-an-address", 9000));

        EXPECT_EQ(id, 0U);
        EXPECT_TRUE(rig.backend.get_endpoints().empty());
        EXPECT_TRUE(port_is_bindable(port));
    }

    TEST(UDPBackendTest, CloseEndpointRemovesItAndReleasesItsPort)
    {
        UdpRig rig;
        ASSERT_TRUE(rig.up());
        const uint16_t port = free_port();
        ASSERT_EQ(rig.backend.open_endpoint(endpoint(1, EndpointRole::RECEIVE, port)), 1U);
        EXPECT_FALSE(port_is_bindable(port));

        rig.backend.close_endpoint(1);

        EXPECT_EQ(rig.backend.get_endpoint_state(1), EndpointState::CLOSED);
        EXPECT_TRUE(rig.backend.get_endpoints().empty());
        EXPECT_TRUE(port_is_bindable(port));
    }

    TEST(UDPBackendTest, EndpointsOnOnePortShareItUntilTheLastCloses)
    {
        UdpRig rig;
        ASSERT_TRUE(rig.up());
        const uint16_t port = free_port();
        ASSERT_EQ(rig.backend.open_endpoint(endpoint(1, EndpointRole::RECEIVE, port)), 1U);
        ASSERT_EQ(rig.backend.open_endpoint(endpoint(2, EndpointRole::RECEIVE, port)), 2U);

        rig.backend.close_endpoint(1);
        EXPECT_FALSE(port_is_bindable(port));

        rig.backend.close_endpoint(2);
        EXPECT_TRUE(port_is_bindable(port));
    }

    // ------------------------------------------------------------------
    // Data transfer
    // ------------------------------------------------------------------

    TEST(UDPBackendTest, SendWithoutADefaultRemoteFails)
    {
        UdpRig rig;
        ASSERT_TRUE(rig.up());
        ASSERT_EQ(rig.backend.open_endpoint(endpoint(1, EndpointRole::BIDIRECTIONAL, free_port())), 1U);
        const std::vector<uint8_t> data = payload({ 1 });

        EXPECT_FALSE(rig.backend.send(1, data.data(), data.size()));
    }

    TEST(UDPBackendTest, SendToRejectsAnInvalidAddress)
    {
        UdpRig rig;
        ASSERT_TRUE(rig.up());
        ASSERT_EQ(rig.backend.open_endpoint(endpoint(1, EndpointRole::SEND)), 1U);
        const std::vector<uint8_t> data = payload({ 1 });

        EXPECT_FALSE(rig.backend.send_to(1, data.data(), data.size(), "not-an-address", 9000));
    }

    TEST(UDPBackendTest, SendDeliversToTheDefaultRemoteOverLoopback)
    {
        UdpRig rig;
        ASSERT_TRUE(rig.up());
        const uint16_t port = free_port();
        ASSERT_EQ(rig.backend.open_endpoint(endpoint(1, EndpointRole::RECEIVE, port)), 1U);
        ASSERT_EQ(rig.backend.open_endpoint(endpoint(2, EndpointRole::SEND, 0, k_loopback, port)), 2U);
        const std::vector<uint8_t> data = payload({ 0xDE, 0xAD, 0xBE, 0xEF });

        ASSERT_TRUE(rig.backend.send(2, data.data(), data.size()));

        ASSERT_TRUE(rig.pump_until([&] { return rig.count_for(1) == 1; }));
        EXPECT_EQ(rig.received.front().bytes, data);
        EXPECT_EQ(rig.received.front().sender.rfind("127.0.0.1:", 0), 0U);
    }

    TEST(UDPBackendTest, SendToDeliversToAnExplicitTarget)
    {
        UdpRig rig;
        ASSERT_TRUE(rig.up());
        const uint16_t port = free_port();
        ASSERT_EQ(rig.backend.open_endpoint(endpoint(1, EndpointRole::RECEIVE, port)), 1U);
        ASSERT_EQ(rig.backend.open_endpoint(endpoint(2, EndpointRole::SEND)), 2U);
        const std::vector<uint8_t> data = payload({ 9, 8, 7 });

        ASSERT_TRUE(rig.backend.send_to(2, data.data(), data.size(), k_loopback, port));

        ASSERT_TRUE(rig.pump_until([&] { return rig.count_for(1) == 1; }));
        EXPECT_EQ(rig.received.front().bytes, data);
    }

    TEST(UDPBackendTest, LargeDatagramArrivesIntact)
    {
        UdpRig rig;
        ASSERT_TRUE(rig.up());
        const uint16_t port = free_port();
        ASSERT_EQ(rig.backend.open_endpoint(endpoint(1, EndpointRole::RECEIVE, port)), 1U);
        ASSERT_EQ(rig.backend.open_endpoint(endpoint(2, EndpointRole::SEND, 0, k_loopback, port)), 2U);
        std::vector<uint8_t> data(1400);
        for (size_t i = 0; i < data.size(); ++i)
            data[i] = static_cast<uint8_t>(i % 251);

        ASSERT_TRUE(rig.backend.send(2, data.data(), data.size()));

        ASSERT_TRUE(rig.pump_until([&] { return rig.count_for(1) == 1; }));
        EXPECT_EQ(rig.received.front().bytes, data);
    }

    TEST(UDPBackendTest, EveryReceivingEndpointOnAPortGetsTheDatagram)
    {
        UdpRig rig;
        ASSERT_TRUE(rig.up());
        const uint16_t port = free_port();
        ASSERT_EQ(rig.backend.open_endpoint(endpoint(1, EndpointRole::RECEIVE, port)), 1U);
        ASSERT_EQ(rig.backend.open_endpoint(endpoint(2, EndpointRole::BIDIRECTIONAL, port)), 2U);
        ASSERT_EQ(rig.backend.open_endpoint(endpoint(3, EndpointRole::SEND)), 3U);
        const std::vector<uint8_t> data = payload({ 5 });

        ASSERT_TRUE(rig.backend.send_to(3, data.data(), data.size(), k_loopback, port));

        ASSERT_TRUE(rig.pump_until([&] { return rig.count_for(1) == 1 && rig.count_for(2) == 1; }));
        EXPECT_EQ(rig.received.size(), 2U);
    }

    TEST(UDPBackendTest, SendOnlyEndpointNeverReceives)
    {
        UdpRig rig;
        ASSERT_TRUE(rig.up());
        const uint16_t send_only_port = free_port();
        const uint16_t sentinel_port = free_port();
        ASSERT_EQ(rig.backend.open_endpoint(endpoint(1, EndpointRole::SEND, send_only_port)), 1U);
        ASSERT_EQ(rig.backend.open_endpoint(endpoint(2, EndpointRole::RECEIVE, sentinel_port)), 2U);
        ASSERT_EQ(rig.backend.open_endpoint(endpoint(3, EndpointRole::SEND)), 3U);
        const std::vector<uint8_t> data = payload({ 1 });

        ASSERT_TRUE(rig.backend.send_to(3, data.data(), data.size(), k_loopback, send_only_port));
        ASSERT_TRUE(rig.backend.send_to(3, data.data(), data.size(), k_loopback, sentinel_port));

        ASSERT_TRUE(rig.pump_until([&] { return rig.count_for(2) == 1; }));
        EXPECT_EQ(rig.count_for(1), 0U);
    }

    TEST(UDPBackendTest, ClosedEndpointStopsReceiving)
    {
        UdpRig rig;
        ASSERT_TRUE(rig.up());
        const uint16_t port = free_port();
        ASSERT_EQ(rig.backend.open_endpoint(endpoint(1, EndpointRole::RECEIVE, port)), 1U);
        ASSERT_EQ(rig.backend.open_endpoint(endpoint(2, EndpointRole::RECEIVE, port)), 2U);
        ASSERT_EQ(rig.backend.open_endpoint(endpoint(3, EndpointRole::SEND)), 3U);
        rig.backend.close_endpoint(1);
        const std::vector<uint8_t> data = payload({ 4 });

        ASSERT_TRUE(rig.backend.send_to(3, data.data(), data.size(), k_loopback, port));

        ASSERT_TRUE(rig.pump_until([&] { return rig.count_for(2) == 1; }));
        EXPECT_EQ(rig.count_for(1), 0U);
    }

    TEST(UDPBackendTest, EmptyDatagramIsNotDelivered)
    {
        UdpRig rig;
        ASSERT_TRUE(rig.up());
        const uint16_t port = free_port();
        ASSERT_EQ(rig.backend.open_endpoint(endpoint(1, EndpointRole::RECEIVE, port)), 1U);
        ASSERT_EQ(rig.backend.open_endpoint(endpoint(2, EndpointRole::SEND)), 2U);
        const std::vector<uint8_t> data = payload({ 6 });

        ASSERT_TRUE(rig.backend.send_to(2, data.data(), 0, k_loopback, port));
        ASSERT_TRUE(rig.backend.send_to(2, data.data(), data.size(), k_loopback, port));

        ASSERT_TRUE(rig.pump_until([&] { return rig.count_for(1) >= 1; }));
        ASSERT_EQ(rig.received.size(), 1U);
        EXPECT_EQ(rig.received.front().bytes, data);
    }

    TEST(UDPBackendTest, DatagramsArriveInSendOrder)
    {
        UdpRig rig;
        ASSERT_TRUE(rig.up());
        const uint16_t port = free_port();
        ASSERT_EQ(rig.backend.open_endpoint(endpoint(1, EndpointRole::RECEIVE, port)), 1U);
        ASSERT_EQ(rig.backend.open_endpoint(endpoint(2, EndpointRole::SEND, 0, k_loopback, port)), 2U);

        for (uint8_t i = 0; i < 20; ++i) {
            const uint8_t byte = i;
            ASSERT_TRUE(rig.backend.send(2, &byte, 1));
        }

        ASSERT_TRUE(rig.pump_until([&] { return rig.count_for(1) == 20; }));
        for (size_t i = 0; i < rig.received.size(); ++i)
            EXPECT_EQ(rig.received[i].bytes.front(), static_cast<uint8_t>(i));
    }

} // namespace
} // namespace MayaFlux::Test
