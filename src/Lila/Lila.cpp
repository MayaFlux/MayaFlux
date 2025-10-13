#include "Lila.hpp"

#include "ClangInterpreter.hpp"
#include "Server.hpp"

namespace Lila {

Lila::Lila()
    : m_interpreter(std::make_unique<ClangInterpreter>())
    , m_current_mode(OperationMode::Direct)
{
}

Lila::~Lila()
{
    stop_server();
}

bool Lila::initialize(OperationMode mode, int server_port)
{
    m_current_mode = mode;

    if (!initialize_interpreter()) {
        return false;
    }

    if (mode == OperationMode::Server || mode == OperationMode::Both) {
        if (!initialize_server(server_port)) {
            return false;
        }
    }

    return true;
}

bool Lila::initialize_interpreter()
{
    return m_interpreter->initialize();
}

bool Lila::initialize_server(int port)
{
    if (m_server && m_server->is_running()) {
        stop_server();
    }

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
        return R"({"status":"error","message":"Empty message"})";
    }

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
        return false;
    }

    auto result = m_interpreter->eval_file(filepath);
    return result.success;
}

void Lila::start_server(int port)
{
    initialize_server(port);
}

void Lila::stop_server()
{
    if (m_server) {
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
}

void Lila::on_error(std::function<void(const std::string&)> callback)
{
    m_error_callback = std::move(callback);
}

void Lila::on_server_client_connected(std::function<void(int)> callback)
{
    if (m_server) {
        m_server->on_client_connected(std::move(callback));
    }
}

void Lila::on_server_client_disconnected(std::function<void(int)> callback)
{
    if (m_server) {
        m_server->on_client_disconnected(std::move(callback));
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
