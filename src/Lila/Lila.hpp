#pragma once

namespace Lila {

class ClangInterpreter;

class Lila {
public:
    Lila();
    ~Lila();

    bool initialize();

    bool eval(const std::string& code);

    std::string get_last_error() const;

    void on_success(std::function<void()> callback);

    void on_error(std::function<void(const std::string&)> callback);

private:
    std::unique_ptr<ClangInterpreter> m_interpreter;
    std::function<void()> m_success_callback;
    std::function<void(const std::string&)> m_error_callback;
};

} // namespace Lila
