#include "Listener.hpp"

#include "Commentator.hpp"

#include "MayaFlux/Transitive/Platform/UnixSocket.hpp"

#include <asio.hpp>

#include <deque>
#include <stdexcept>

namespace Lila {

namespace {

    namespace Platform = MayaFlux::Platform;
    constexpr size_t k_read_size = 16384;
    constexpr size_t k_max_block = size_t { 1 } << 20;

    std::string escape_json(std::string_view text)
    {
        std::string escaped;
        escaped.reserve(text.size());

        for (const char c : text) {
            switch (c) {
            case '"':
                escaped += "\\\"";
                break;
            case '\\':
                escaped += "\\\\";
                break;
            case '\n':
                escaped += "\\n";
                break;
            case '\r':
                escaped += "\\r";
                break;
            case '\t':
                escaped += "\\t";
                break;
            default:
                if (static_cast<unsigned char>(c) >= 0x20) {
                    escaped += c;
                }
            }
        }
        return escaped;
    }

    class Decoder {
    public:
        template <typename Sink>
        bool feed(std::string_view chunk, Sink& sink)
        {
            m_block.append(chunk);

            if (m_block.size() > k_max_block) {
                m_block.clear();
                return false;
            }
            if (m_block.empty() || m_block.back() != '\n') {
                return true;
            }

            std::string block = std::move(m_block);
            m_block.clear();
            while (!block.empty() && (block.back() == '\n' || block.back() == '\r')) {
                block.pop_back();
            }
            if (!block.empty()) {
                sink(std::string_view(block));
            }
            return true;
        }

    private:
        std::string m_block;
    };

    std::string describe(const Platform::AcquireFailure& failure)
    {
        using Reason = Platform::AcquireFailure::Reason;

        std::string text;
        switch (failure.reason) {
        case Reason::EmptyRequest:
            text = "empty request";
            break;
        case Reason::InvalidName:
            text = "invalid name";
            break;
        case Reason::Missing:
            text = "not found";
            break;
        case Reason::NotDirectory:
            text = "not a directory";
            break;
        case Reason::NotRegularFile:
            text = "not a regular file";
            break;
        case Reason::MarkerMissing:
            text = "marker file missing";
            break;
        case Reason::TooLong:
            text = "path too long";
            break;
        case Reason::Unsafe:
            text = "unsafe location";
            break;
        case Reason::NotSocket:
            text = "not a socket";
            break;
        case Reason::InUse:
            text = "already in use";
            break;
        case Reason::StaleNotRemoved:
            text = "stale socket could not be removed";
            break;
        }
        if (!failure.request.empty()) {
            text += ": " + failure.request;
        }
        return "unix socket refused, " + text;
    }

    struct TcpTransport {
        using Stream = asio::ip::tcp;

        struct Target {
            std::string host;
            int port;
        };

        struct Bound { };

        static Result<Bound> bind(Stream::acceptor& acceptor, const Target& target)
        {
            try {
                const Stream::endpoint endpoint(asio::ip::make_address(target.host),
                    static_cast<asio::ip::port_type>(target.port));
                acceptor.open(endpoint.protocol());
                acceptor.set_option(asio::socket_base::reuse_address(true));
                acceptor.bind(endpoint);
                acceptor.listen(asio::socket_base::max_listen_connections);
                return Bound {};
            } catch (const std::exception& e) {
                return std::unexpected(std::string("Failed to listen: ") + e.what());
            }
        }

        static void release(const Target&, const Bound&) { }

        static void tune(Stream::socket& socket)
        {
            asio::error_code ec;
            (void)socket.set_option(Stream::no_delay(true), ec);
        }

        static Address describe_target(const Target& target, const std::optional<Bound>&)
        {
            return TcpAddress { .host = target.host, .port = target.port };
        }

        static std::string where(const Target& target, const std::optional<Bound>&)
        {
            return target.host + ":" + std::to_string(target.port);
        }
    };

    bool socket_answers(const std::filesystem::path& path)
    {
        using Stream = asio::local::stream_protocol;
        try {
            asio::io_context context;
            Stream::socket probe(context);
            probe.open(Stream());
            probe.non_blocking(true);

            asio::error_code ec;
            (void)probe.connect(Stream::endpoint(path.string()), ec);

            return !(ec == asio::error::connection_refused
                || ec == std::errc::no_such_file_or_directory);
        } catch (const std::exception&) {
            return true;
        }
    }

    struct UnixTransport {
        using Stream = asio::local::stream_protocol;

        struct Target {
            std::optional<std::filesystem::path> path;
        };

        struct Bound {
            std::filesystem::path path;
        };

        static Result<Bound> bind(Stream::acceptor& acceptor, const Target& target)
        {
            const Platform::UnixSocketSource source = target.path
                ? Platform::UnixSocketSource { Platform::UnixSocketFromPath { .path = *target.path } }
                : Platform::UnixSocketSource { Platform::UnixSocketFromName { .file_name = std::string(Platform::default_unix_socket_name) } };

            const auto acquired = Platform::acquire_unix_socket(source, socket_answers);
            if (!acquired) {
                return std::unexpected(describe(acquired.error()));
            }

            Bound bound { .path = acquired->path };
            try {
                const Stream::endpoint endpoint(bound.path.string());
                acceptor.open(endpoint.protocol());
                acceptor.bind(endpoint);
            } catch (const std::exception& e) {
                return std::unexpected(std::string("Failed to listen on unix socket: ") + e.what());
            }

            try {
                acceptor.listen(asio::socket_base::max_listen_connections);
                if (const auto restricted = Platform::restrict_unix_socket_to_owner(bound.path); !restricted) {
                    throw std::runtime_error(describe(restricted.error()));
                }
            } catch (const std::exception& e) {
                asio::error_code ec;
                (void)acceptor.close(ec);
                std::error_code remove_ec;
                std::filesystem::remove(bound.path, remove_ec);
                return std::unexpected(std::string("Failed to listen on unix socket: ") + e.what());
            }

            return bound;
        }

        static void release(const Target&, const Bound& bound)
        {
            std::error_code ec;
            std::filesystem::remove(bound.path, ec);
        }

        static void tune(Stream::socket&) { }

        static Address describe_target(const Target& target, const std::optional<Bound>& bound)
        {
            return UnixAddress { .path = bound ? std::optional<std::filesystem::path>(bound->path) : target.path };
        }

        static std::string where(const Target& target, const std::optional<Bound>& bound)
        {
            return "socket " + (bound ? bound->path : target.path.value_or(std::filesystem::path {})).string();
        }
    };

    template <typename Transport>
    class StreamCore final : public Listener {
    public:
        using Stream = typename Transport::Stream;

        explicit StreamCore(typename Transport::Target target)
            : m_target(std::move(target))
        {
        }

        ~StreamCore() override { stop(); }

        [[nodiscard]] Result<void> start() override
        {
            if (m_running.load(std::memory_order_acquire)) {
                return std::unexpected("Already listening");
            }

            m_acceptor = std::make_unique<typename Stream::acceptor>(m_io_context);
            auto bound = Transport::bind(*m_acceptor, m_target);
            if (!bound) {
                m_acceptor.reset();
                LILA_ERROR(Emitter::SERVER, bound.error());
                return std::unexpected(std::move(bound.error()));
            }
            m_bound = std::move(*bound);

            m_running.store(true, std::memory_order_release);
            start_accept();

            m_io_thread = std::thread([this]() {
                LILA_DEBUG(Emitter::SERVER, "IO thread started");
                m_io_context.run();
                LILA_DEBUG(Emitter::SERVER, "IO thread exiting");
            });

            LILA_INFO(Emitter::SERVER, "Listening on " + Transport::where(m_target, m_bound));
            return {};
        }

        void stop() noexcept override
        {
            if (!m_running.exchange(false, std::memory_order_acq_rel)) {
                return;
            }

            m_io_context.stop();
            if (m_io_thread.joinable()) {
                m_io_thread.join();
            }

            asio::error_code ec;
            if (m_acceptor) {
                (void)m_acceptor->close(ec);
                m_acceptor.reset();
            }

            {
                std::scoped_lock lock(m_sessions_mutex);
                for (auto& [id, session] : m_sessions) {
                    session->close();
                }
                m_sessions.clear();
            }

            if (m_bound) {
                Transport::release(m_target, *m_bound);
            }

            LILA_INFO(Emitter::SERVER, "Stopped listening");
        }

        [[nodiscard]] bool is_running() const override
        {
            return m_running.load(std::memory_order_acquire);
        }

        [[nodiscard]] Address address() const override
        {
            return Transport::describe_target(m_target, m_bound);
        }

    private:
        class Session : public std::enable_shared_from_this<Session> {
        public:
            Session(typename Stream::socket socket, int id, StreamCore& core)
                : m_socket(std::move(socket))
                , m_core(core)
                , m_id(id)
            {
                Transport::tune(m_socket);
            }

            void start() { read(); }

            void send(std::string_view reply)
            {
                auto line = std::make_shared<std::string>(reply);
                line->push_back('\n');

                m_out.push_back(std::move(line));
                if (!m_writing) {
                    write_next();
                }
            }

            void close()
            {
                asio::error_code ec;
                (void)m_socket.shutdown(asio::socket_base::shutdown_both, ec);
                (void)m_socket.close(ec);
            }

            void set_name(std::string name) { m_name = std::move(name); }

        private:
            void read()
            {
                m_socket.async_read_some(asio::buffer(m_read_buffer),
                    [self = this->shared_from_this()](const asio::error_code& ec, size_t bytes) {
                        self->on_read(ec, bytes);
                    });
            }

            void on_read(const asio::error_code& ec, size_t bytes)
            {
                if (ec) {
                    m_core.remove_session(m_id);
                    return;
                }

                auto deliver = [this](std::string_view text) { m_core.handle_message(*this, text); };
                if (!m_decoder.feed(std::string_view(m_read_buffer.data(), bytes), deliver)) {
                    LILA_WARN(Emitter::SERVER,
                        "Client " + std::to_string(m_id) + " sent a block over 1 MiB, dropped");
                }

                read();
            }

            void write_next()
            {
                m_writing = true;
                const auto line = m_out.front();

                asio::async_write(m_socket, asio::buffer(*line),
                    [self = this->shared_from_this(), line](const asio::error_code& ec, size_t) {
                        self->m_out.pop_front();
                        if (ec) {
                            LILA_WARN(Emitter::SERVER,
                                "Write failed for client " + std::to_string(self->m_id) + ": " + ec.message());
                            self->m_writing = false;
                            self->m_core.remove_session(self->m_id);
                            return;
                        }
                        if (self->m_out.empty()) {
                            self->m_writing = false;
                            return;
                        }
                        self->write_next();
                    });
            }

            typename Stream::socket m_socket;
            StreamCore& m_core;
            int m_id;
            Decoder m_decoder;
            std::array<char, k_read_size> m_read_buffer {};
            std::deque<std::shared_ptr<std::string>> m_out;
            std::string m_name;
            bool m_writing { false };
        };

        void start_accept()
        {
            m_acceptor->async_accept(
                [this](const asio::error_code& ec, typename Stream::socket socket) {
                    if (ec) {
                        if (m_running.load(std::memory_order_acquire)) {
                            LILA_ERROR(Emitter::SERVER, "Accept failed: " + ec.message());
                        }
                        return;
                    }

                    const int id = m_next_id++;
                    auto session = std::make_shared<Session>(std::move(socket), id, *this);
                    {
                        std::scoped_lock lock(m_sessions_mutex);
                        m_sessions[id] = session;
                    }
                    LILA_INFO(Emitter::SERVER, "Client connected id: " + std::to_string(id));

                    session->start();
                    start_accept();
                });
        }

        void remove_session(int id)
        {
            std::shared_ptr<Session> session;
            {
                std::scoped_lock lock(m_sessions_mutex);
                const auto it = m_sessions.find(id);
                if (it == m_sessions.end()) {
                    return;
                }
                session = std::move(it->second);
                m_sessions.erase(it);
            }

            session->close();
            LILA_INFO(Emitter::SERVER, "Client disconnected (id: " + std::to_string(id) + ")");
        }

        void handle_message(Session& session, std::string_view text)
        {
            if (text.starts_with('@')) {
                handle_control(session, text.substr(1));
                return;
            }

            if (!m_message) {
                return;
            }

            const auto result = m_message(text);
            if (result) {
                session.send(*result);
            } else {
                session.send(R"({"status":"error","message":")" + escape_json(result.error()) + "\"}");
            }
        }

        static void handle_control(Session& session, std::string_view command)
        {
            if (command.starts_with("session ")) {
                const std::string name(command.substr(8));
                session.set_name(name);
                session.send(R"({"status":"success","message":"Session ID set to ')" + escape_json(name) + R"('"})");
            } else if (command.starts_with("ping")) {
                session.send(R"({"status":"success","message":"pong"})");
            } else {
                session.send(R"({"status":"error","message":"Unknown command: )" + escape_json(command) + "\"}");
            }
        }

        typename Transport::Target m_target;
        std::optional<typename Transport::Bound> m_bound;

        asio::io_context m_io_context;
        std::unique_ptr<typename Stream::acceptor> m_acceptor;
        std::thread m_io_thread;
        std::atomic<bool> m_running { false };

        mutable std::mutex m_sessions_mutex;
        std::unordered_map<int, std::shared_ptr<Session>> m_sessions;
        int m_next_id { 1 };
    };

}

std::unique_ptr<Listener> Listener::create(const Address& where)
{
    if (const auto* tcp = std::get_if<TcpAddress>(&where)) {
        return std::make_unique<StreamCore<TcpTransport>>(
            TcpTransport::Target { .host = tcp->host, .port = tcp->port });
    }
    return std::make_unique<StreamCore<UnixTransport>>(
        UnixTransport::Target { .path = std::get<UnixAddress>(where).path });
}

} // namespace Lila
