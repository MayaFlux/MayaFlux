#include "Lila.hpp"

#include "ClangInterpreter.hpp"
#include "Server.hpp"

#include "Commentator.hpp"

namespace Lila {

Lila::Lila()
    : m_interpreter(std::make_unique<ClangInterpreter>())
    , m_current_mode(OperationMode::Direct)
{
    LILA_DEBUG(Emitter::SYSTEM, "Lila instance created");
}

Lila::~Lila()
{
    stop_server();
    LILA_DEBUG(Emitter::SYSTEM, "Lila instance destroyed");
}

bool Lila::initialize(OperationMode mode, int server_port)
{
    LILA_INFO(Emitter::SYSTEM, "Initializing Lila");
    m_current_mode = mode;

    if (!initialize_interpreter()) {
        LILA_ERROR(Emitter::SYSTEM, "Failed to initialize interpreter");
        return false;
    }

    if (mode == OperationMode::Server || mode == OperationMode::Both) {
        if (!initialize_server(server_port)) {
            LILA_ERROR(Emitter::SYSTEM, "Failed to initialize server");
            return false;
        }
    }

    LILA_INFO(Emitter::SYSTEM, "Lila initialized successfully");
    return true;
}

bool Lila::initialize_interpreter()
{
    LILA_DEBUG(Emitter::SYSTEM, "Initializing interpreter subsystem");
    return m_interpreter->initialize();
}

bool Lila::initialize_server(int port)
{
    if (m_server && m_server->is_running()) {
        LILA_WARN(Emitter::SYSTEM, "Stopping existing server before starting new one");
        stop_server();
    }

    LILA_DEBUG(Emitter::SYSTEM,
        std::string("Initializing server on port ") + std::to_string(port));

    m_server = std::make_unique<Server>(port);

    m_server->set_message_handler([this](const std::string& message) {
        return this->handle_server_message(message);
    });

    m_server->start();
    return true;
}

std::string Lila::handle_server_message(const std::string& message)
{
    if (message.empty()) {
        LILA_WARN(Emitter::SERVER, "Received empty message");
        return R"({"status":"error","message":"Empty message"})";
    }

    LILA_DEBUG(Emitter::SYSTEM, "Processing server message");
    auto result = m_interpreter->eval(message);

    if (result.success) {
        if (m_success_callback) {
            m_success_callback();
        }
        return R"({"status":"success"})";
    } else {
        if (m_error_callback) {
            m_error_callback(result.error);
        }
        return R"({"status":"error","message":")" + result.error + "\"}";
    }
}

bool Lila::eval(const std::string& code)
{
    if (!m_interpreter) {
        LILA_ERROR(Emitter::SYSTEM, "Cannot eval: interpreter not initialized");
        return false;
    }

    auto result = m_interpreter->eval(code);

    if (result.success && m_success_callback) {
        m_success_callback();
    } else if (!result.success && m_error_callback) {
        m_error_callback(result.error);
    }

    return result.success;
}

bool Lila::eval_file(const std::string& filepath)
{
    if (!m_interpreter) {
        LILA_ERROR(Emitter::SYSTEM, "Cannot eval file: interpreter not initialized");
        return false;
    }

    auto result = m_interpreter->eval_file(filepath);
    return result.success;
}

void Lila::start_server(int port)
{
    LILA_INFO(Emitter::SYSTEM,
        std::string("Starting server on port ") + std::to_string(port));

    initialize_server(port);
}

void Lila::stop_server()
{
    if (m_server) {
        LILA_INFO(Emitter::SYSTEM, "Stopping server");
        m_server->stop();
        m_server.reset();
    }
}

bool Lila::is_server_running() const
{
    return m_server && m_server->is_running();
}

void* Lila::get_symbol_address(const std::string& name)
{
    return m_interpreter ? m_interpreter->get_symbol_address(name) : nullptr;
}

std::vector<std::string> Lila::get_defined_symbols()
{
    return m_interpreter ? m_interpreter->get_defined_symbols() : std::vector<std::string> {};
}

void Lila::add_include_path(const std::string& path)
{
    if (m_interpreter) {
        m_interpreter->add_include_path(path);
    }
}

void Lila::add_compile_flag(const std::string& flag)
{
    if (m_interpreter) {
        m_interpreter->add_compile_flag(flag);
    }
}

void Lila::on_success(std::function<void()> callback)
{
    m_success_callback = std::move(callback);
    LILA_DEBUG(Emitter::SYSTEM, "Success callback registered");
}

void Lila::on_error(std::function<void(const std::string&)> callback)
{
    m_error_callback = std::move(callback);
    LILA_DEBUG(Emitter::SYSTEM, "Error callback registered");
}

void Lila::on_server_client_connected(std::function<void(int)> callback)
{
    if (m_server) {
        m_server->on_client_connected(std::move(callback));
        LILA_DEBUG(Emitter::SYSTEM, "Client connected callback registered");
    }
}

void Lila::on_server_client_disconnected(std::function<void(int)> callback)
{
    if (m_server) {
        m_server->on_client_disconnected(std::move(callback));
        LILA_DEBUG(Emitter::SYSTEM, "Client disconnected callback registered");
    }
}

std::string Lila::get_last_error() const
{
    return m_interpreter ? m_interpreter->get_last_error() : "Interpreter not initialized";
}

OperationMode Lila::get_current_mode() const
{
    return m_current_mode;
}

} // namespace Lila
