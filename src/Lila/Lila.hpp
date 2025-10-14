#pragma once

#include <expected>

namespace Lila {

class ClangInterpreter;
class Server;
struct ClientInfo;

enum class OperationMode : uint8_t {
    Direct, ///< Only direct evaluation
    Server, ///< Only server mode
    Both ///< Both direct eval and server
};

class Lila {
public:
    Lila();
    ~Lila(); // Remove "= default", declare only

    bool initialize(OperationMode mode = OperationMode::Direct, int server_port = 9090) noexcept;

    bool eval(const std::string& code);
    bool eval_file(const std::string& filepath);

    void start_server(int port = 9090);
    void stop_server();
    [[nodiscard]] bool is_server_running() const;

    void* get_symbol_address(const std::string& name);
    std::vector<std::string> get_defined_symbols();

    void add_include_path(const std::string& path);
    void add_compile_flag(const std::string& flag);

    void on_success(std::function<void()> callback);
    void on_error(std::function<void(const std::string&)> callback);
    void on_server_client_connected(std::function<void(const ClientInfo&)> callback);
    void on_server_client_disconnected(std::function<void(const ClientInfo&)> callback);
    void on_server_started(std::function<void()> callback);

    [[nodiscard]] std::string get_last_error() const;
    [[nodiscard]] OperationMode get_current_mode() const;

private:
    std::unique_ptr<ClangInterpreter> m_interpreter;
    std::unique_ptr<Server> m_server;

    OperationMode m_current_mode;
    std::function<void()> m_success_callback;
    std::function<void(const std::string&)> m_error_callback;

    bool initialize_interpreter();
    bool initialize_server(int port);

    std::expected<std::string, std::string> handle_server_message(std::string_view message);

    static std::string escape_json(const std::string& str);
};

} // namespace Lila
