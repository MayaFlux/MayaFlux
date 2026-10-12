#include "gtest/gtest.h"

#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "MayaFlux/Core/Backends/Network/TCPBackend.hpp"

namespace MayaFlux::Test {
namespace {

    using namespace std::chrono_literals;
    using Core::EndpointInfo;
    using Core::EndpointRole;
    using Core::EndpointState;
    using Core::TCPBackend;
    using Core::TCPBackendInfo;

    constexpr const char* k_loopback = "127.0.0.1";
    constexpr uint64_t k_first_accepted_id = 100;

    uint16_t free_port()
    {
        asio::io_context context;
        asio::ip::tcp::acceptor acceptor(context, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), 0));
        return acceptor.local_endpoint().port();
    }

    bool can_listen(uint16_t port)
    {
        asio::io_context context;
        asio::ip::tcp::acceptor acceptor(context);
        asio::error_code ec;
        if (acceptor.open(asio::ip::tcp::v4(), ec))
            return false;
        if (acceptor.set_option(asio::socket_base::reuse_address(true), ec))
            return false;
        if (acceptor.bind(asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port), ec))
            return false;
        return !acceptor.listen(asio::socket_base::max_listen_connections, ec);
    }

    struct Received {
        uint64_t id {};
        std::vector<uint8_t> bytes;
        std::string sender;
    };

    struct StateEvent {
        uint64_t id {};
        EndpointState previous {};
        EndpointState current {};
    };

    TCPBackendInfo reconnecting(bool enabled, uint32_t interval_ms = 20)
    {
        TCPBackendInfo info;
        info.auto_reconnect = enabled;
        info.reconnect_interval_ms = interval_ms;
        return info;
    }

    class TcpRig {
    public:
        explicit TcpRig(const TCPBackendInfo& config = reconnecting(false))
            : backend(config, context)
        {
            backend.set_endpoint_id_allocator([this] { return next_accepted_id++; });
            backend.set_receive_callback(
                [this](uint64_t id, const uint8_t* data, size_t size, std::string_view sender) {
                    received.push_back({ .id = id, .bytes = std::vector<uint8_t>(data, data + size), .sender = std::string(sender) });
                });
            backend.set_state_callback(
                [this](const EndpointInfo& info, EndpointState previous, EndpointState current) {
                    states.push_back({ .id = info.id, .previous = previous, .current = current });
                });
        }

        TcpRig(const TcpRig&) = delete;
        TcpRig& operator=(const TcpRig&) = delete;
        TcpRig(TcpRig&&) = delete;
        TcpRig& operator=(TcpRig&&) = delete;

        ~TcpRig() { backend.shutdown(); }

        bool up()
        {
            if (!backend.initialize())
                return false;
            backend.start();
            return true;
        }

        template <typename Predicate>
        bool pump_until(Predicate predicate, std::chrono::milliseconds timeout = 3s)
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

        [[nodiscard]] bool saw(uint64_t id, EndpointState previous, EndpointState current) const
        {
            return std::ranges::any_of(states, [&](const StateEvent& event) {
                return event.id == id && event.previous == previous && event.current == current;
            });
        }

        [[nodiscard]] uint64_t accepted_id() const
        {
            for (const auto& event : states) {
                if (event.id >= k_first_accepted_id && event.previous == EndpointState::CLOSED
                    && event.current == EndpointState::OPEN)
                    return event.id;
            }
            return 0;
        }

        uint64_t connect_pair(uint16_t port)
        {
            if (backend.open_endpoint(listener(1, port)) != 1)
                return 0;
            if (backend.open_endpoint(outbound(2, port)) != 2)
                return 0;
            const bool connected = pump_until([&] {
                return backend.get_endpoint_state(2) == EndpointState::OPEN && accepted_id() != 0;
            });
            return connected ? accepted_id() : 0;
        }

        static EndpointInfo listener(uint64_t id, uint16_t port)
        {
            EndpointInfo info;
            info.id = id;
            info.transport = Core::NetworkTransport::TCP;
            info.role = EndpointRole::BIDIRECTIONAL;
            info.local_port = port;
            return info;
        }

        static EndpointInfo outbound(uint64_t id, uint16_t port, const std::string& address = k_loopback)
        {
            EndpointInfo info;
            info.id = id;
            info.transport = Core::NetworkTransport::TCP;
            info.role = EndpointRole::BIDIRECTIONAL;
            info.remote_address = address;
            info.remote_port = port;
            return info;
        }

        asio::io_context context;
        TCPBackend backend;
        uint64_t next_accepted_id { k_first_accepted_id };
        std::vector<Received> received;
        std::vector<StateEvent> states;
    };

    std::vector<uint8_t> bytes_of(std::initializer_list<uint8_t> values)
    {
        return values;
    }

    // ------------------------------------------------------------------
    // Lifecycle
    // ------------------------------------------------------------------

    TEST(TCPBackendTest, ReportsIdentity)
    {
        TcpRig rig;

        EXPECT_EQ(rig.backend.get_transport(), Core::NetworkTransport::TCP);
        EXPECT_FALSE(rig.backend.get_name().empty());
        EXPECT_FALSE(rig.backend.get_version().empty());
        EXPECT_FALSE(rig.backend.is_initialized());
        EXPECT_FALSE(rig.backend.is_running());
    }

    TEST(TCPBackendTest, LifecycleCallsAreIdempotent)
    {
        TcpRig rig;

        ASSERT_TRUE(rig.backend.initialize());
        EXPECT_TRUE(rig.backend.initialize());

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

    TEST(TCPBackendTest, ShutdownClosesEveryEndpointAndReleasesTheListeningPort)
    {
        TcpRig rig;
        ASSERT_TRUE(rig.up());
        const uint16_t port = free_port();
        ASSERT_NE(rig.connect_pair(port), 0U);

        rig.backend.shutdown();

        EXPECT_TRUE(rig.backend.get_endpoints().empty());
        EXPECT_TRUE(can_listen(port));
    }

    // ------------------------------------------------------------------
    // Listeners
    // ------------------------------------------------------------------

    TEST(TCPBackendTest, ListenerOpensAndAnnouncesItself)
    {
        TcpRig rig;
        ASSERT_TRUE(rig.up());
        const uint16_t port = free_port();

        const uint64_t id = rig.backend.open_endpoint(TcpRig::listener(1, port));

        EXPECT_EQ(id, 1U);
        EXPECT_EQ(rig.backend.get_endpoint_state(1), EndpointState::OPEN);
        EXPECT_TRUE(rig.saw(1, EndpointState::OPENING, EndpointState::OPEN));
        EXPECT_FALSE(can_listen(port));
        const auto endpoints = rig.backend.get_endpoints();
        ASSERT_EQ(endpoints.size(), 1U);
        EXPECT_EQ(endpoints.front().local_port, port);
    }

    TEST(TCPBackendTest, SecondListenerOnTheSamePortFails)
    {
        TcpRig rig;
        ASSERT_TRUE(rig.up());
        const uint16_t port = free_port();
        ASSERT_EQ(rig.backend.open_endpoint(TcpRig::listener(1, port)), 1U);

        EXPECT_EQ(rig.backend.open_endpoint(TcpRig::listener(2, port)), 0U);
        EXPECT_EQ(rig.backend.get_endpoints().size(), 1U);
    }

    TEST(TCPBackendTest, ClosingAListenerReleasesItsPort)
    {
        TcpRig rig;
        ASSERT_TRUE(rig.up());
        const uint16_t port = free_port();
        ASSERT_EQ(rig.backend.open_endpoint(TcpRig::listener(1, port)), 1U);

        rig.backend.close_endpoint(1);

        EXPECT_EQ(rig.backend.get_endpoint_state(1), EndpointState::CLOSED);
        EXPECT_TRUE(rig.backend.get_endpoints().empty());
        EXPECT_TRUE(can_listen(port));
        EXPECT_EQ(rig.backend.open_endpoint(TcpRig::listener(3, port)), 3U);
    }

    // ------------------------------------------------------------------
    // Connections
    // ------------------------------------------------------------------

    TEST(TCPBackendTest, OutboundConnectionOpensAndTheListenerAcceptsIt)
    {
        TcpRig rig;
        ASSERT_TRUE(rig.up());
        const uint16_t port = free_port();

        const uint64_t accepted = rig.connect_pair(port);

        ASSERT_GE(accepted, k_first_accepted_id);
        EXPECT_EQ(rig.backend.get_endpoint_state(2), EndpointState::OPEN);
        EXPECT_EQ(rig.backend.get_endpoint_state(accepted), EndpointState::OPEN);
        EXPECT_TRUE(rig.saw(2, EndpointState::OPENING, EndpointState::OPEN));
        EXPECT_EQ(rig.backend.get_endpoints().size(), 3U);
    }

    TEST(TCPBackendTest, AcceptedConnectionDescribesItsPeer)
    {
        TcpRig rig;
        ASSERT_TRUE(rig.up());
        const uint16_t port = free_port();
        const uint64_t accepted = rig.connect_pair(port);
        ASSERT_NE(accepted, 0U);

        bool found = false;
        for (const auto& info : rig.backend.get_endpoints()) {
            if (info.id != accepted)
                continue;
            found = true;
            EXPECT_EQ(info.remote_address, k_loopback);
            EXPECT_EQ(info.local_port, port);
            EXPECT_EQ(info.role, EndpointRole::BIDIRECTIONAL);
            EXPECT_EQ(info.transport, Core::NetworkTransport::TCP);
        }
        EXPECT_TRUE(found);
    }

    TEST(TCPBackendTest, ConnectionToAClosedPortWithoutReconnectReportsError)
    {
        TcpRig rig(reconnecting(false));
        ASSERT_TRUE(rig.up());

        ASSERT_EQ(rig.backend.open_endpoint(TcpRig::outbound(2, free_port())), 2U);

        ASSERT_TRUE(rig.pump_until([&] { return rig.backend.get_endpoint_state(2) == EndpointState::ERROR; }));
        EXPECT_TRUE(rig.saw(2, EndpointState::OPENING, EndpointState::ERROR));
    }

    TEST(TCPBackendTest, ConnectionToAClosedPortWithReconnectRetriesUntilTheListenerAppears)
    {
        TcpRig rig(reconnecting(true, 20));
        ASSERT_TRUE(rig.up());
        const uint16_t port = free_port();

        ASSERT_EQ(rig.backend.open_endpoint(TcpRig::outbound(2, port)), 2U);
        ASSERT_TRUE(rig.pump_until([&] { return rig.backend.get_endpoint_state(2) == EndpointState::RECONNECTING; }));

        ASSERT_EQ(rig.backend.open_endpoint(TcpRig::listener(1, port)), 1U);

        ASSERT_TRUE(rig.pump_until([&] { return rig.backend.get_endpoint_state(2) == EndpointState::OPEN; }));
        EXPECT_TRUE(rig.saw(2, EndpointState::RECONNECTING, EndpointState::OPEN));
    }

    TEST(TCPBackendTest, InvalidRemoteAddressReportsErrorState)
    {
        TcpRig rig;
        ASSERT_TRUE(rig.up());

        rig.backend.open_endpoint(TcpRig::outbound(2, 9, "not-an-address"));

        EXPECT_EQ(rig.backend.get_endpoint_state(2), EndpointState::ERROR);
    }

    TEST(TCPBackendTest, PeerClosingMovesTheAcceptedSideToError)
    {
        TcpRig rig;
        ASSERT_TRUE(rig.up());
        const uint64_t accepted = rig.connect_pair(free_port());
        ASSERT_NE(accepted, 0U);

        rig.backend.close_endpoint(2);

        ASSERT_TRUE(rig.pump_until([&] { return rig.backend.get_endpoint_state(accepted) == EndpointState::ERROR; }));
        EXPECT_TRUE(rig.saw(accepted, EndpointState::OPEN, EndpointState::ERROR));
        EXPECT_EQ(rig.backend.get_endpoint_state(2), EndpointState::CLOSED);
    }

    TEST(TCPBackendTest, ClosingAConnectionRemovesIt)
    {
        TcpRig rig;
        ASSERT_TRUE(rig.up());
        ASSERT_NE(rig.connect_pair(free_port()), 0U);

        rig.backend.close_endpoint(2);

        EXPECT_EQ(rig.backend.get_endpoint_state(2), EndpointState::CLOSED);
        for (const auto& info : rig.backend.get_endpoints())
            EXPECT_NE(info.id, 2U);
        EXPECT_NO_THROW(rig.backend.close_endpoint(2));
    }

    // ------------------------------------------------------------------
    // Framed transfer
    // ------------------------------------------------------------------

    TEST(TCPBackendTest, SendFailsOnUnknownListenerAndUnconnectedEndpoints)
    {
        TcpRig rig;
        ASSERT_TRUE(rig.up());
        const uint16_t port = free_port();
        const std::vector<uint8_t> data = bytes_of({ 1 });
        ASSERT_EQ(rig.backend.open_endpoint(TcpRig::listener(1, port)), 1U);
        ASSERT_EQ(rig.backend.open_endpoint(TcpRig::outbound(2, port)), 2U);

        EXPECT_FALSE(rig.backend.send(99, data.data(), data.size()));
        EXPECT_FALSE(rig.backend.send(1, data.data(), data.size()));
        EXPECT_FALSE(rig.backend.send(2, data.data(), data.size()));
    }

    TEST(TCPBackendTest, FrameSentByTheClientArrivesAtTheAcceptedEndpoint)
    {
        TcpRig rig;
        ASSERT_TRUE(rig.up());
        const uint64_t accepted = rig.connect_pair(free_port());
        ASSERT_NE(accepted, 0U);
        const std::vector<uint8_t> data = bytes_of({ 0xDE, 0xAD, 0xBE, 0xEF });

        ASSERT_TRUE(rig.backend.send(2, data.data(), data.size()));

        ASSERT_TRUE(rig.pump_until([&] { return rig.count_for(accepted) == 1; }));
        EXPECT_EQ(rig.received.front().bytes, data);
        EXPECT_EQ(rig.received.front().sender.rfind("127.0.0.1:", 0), 0U);
    }

    TEST(TCPBackendTest, FrameSentByTheAcceptedEndpointArrivesAtTheClient)
    {
        TcpRig rig;
        ASSERT_TRUE(rig.up());
        const uint64_t accepted = rig.connect_pair(free_port());
        ASSERT_NE(accepted, 0U);
        const std::vector<uint8_t> data = bytes_of({ 7, 8, 9 });

        ASSERT_TRUE(rig.backend.send(accepted, data.data(), data.size()));

        ASSERT_TRUE(rig.pump_until([&] { return rig.count_for(2) == 1; }));
        EXPECT_EQ(rig.received.front().bytes, data);
    }

    TEST(TCPBackendTest, ConsecutiveFramesKeepTheirBoundaries)
    {
        TcpRig rig;
        ASSERT_TRUE(rig.up());
        const uint64_t accepted = rig.connect_pair(free_port());
        ASSERT_NE(accepted, 0U);
        const std::vector<std::vector<uint8_t>> frames {
            bytes_of({ 1 }), bytes_of({ 2, 2 }), bytes_of({ 3, 3, 3 }), bytes_of({ 4, 4, 4, 4 })
        };

        for (const auto& frame : frames)
            ASSERT_TRUE(rig.backend.send(2, frame.data(), frame.size()));

        ASSERT_TRUE(rig.pump_until([&] { return rig.count_for(accepted) == frames.size(); }));
        for (size_t i = 0; i < frames.size(); ++i)
            EXPECT_EQ(rig.received[i].bytes, frames[i]) << i;
    }

    TEST(TCPBackendTest, LargeFrameArrivesIntact)
    {
        TcpRig rig;
        ASSERT_TRUE(rig.up());
        const uint64_t accepted = rig.connect_pair(free_port());
        ASSERT_NE(accepted, 0U);
        std::vector<uint8_t> data(200000);
        for (size_t i = 0; i < data.size(); ++i)
            data[i] = static_cast<uint8_t>(i % 251);

        ASSERT_TRUE(rig.backend.send(2, data.data(), data.size()));

        ASSERT_TRUE(rig.pump_until([&] { return rig.count_for(accepted) == 1; }));
        EXPECT_EQ(rig.received.front().bytes, data);
    }

    TEST(TCPBackendTest, SendToDelegatesToTheEstablishedConnection)
    {
        TcpRig rig;
        ASSERT_TRUE(rig.up());
        const uint64_t accepted = rig.connect_pair(free_port());
        ASSERT_NE(accepted, 0U);
        const std::vector<uint8_t> data = bytes_of({ 5, 6 });

        ASSERT_TRUE(rig.backend.send_to(2, data.data(), data.size(), "10.9.9.9", 1));

        ASSERT_TRUE(rig.pump_until([&] { return rig.count_for(accepted) == 1; }));
        EXPECT_EQ(rig.received.front().bytes, data);
    }

} // namespace
} // namespace MayaFlux::Test
