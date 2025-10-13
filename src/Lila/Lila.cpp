#include "Lila.hpp"
#include "ClangInterpreter.hpp"

namespace Lila {

Lila::Lila()
    : m_interpreter(std::make_unique<ClangInterpreter>())
{
}

Lila::~Lila() = default;

bool Lila::initialize()
{
    return m_interpreter->initialize();
}

bool Lila::eval(const std::string& code)
{
    auto success = m_interpreter->eval(code);

    if (success.success && m_success_callback) {
        m_success_callback();
    } else if (!success.success && m_error_callback) {
        m_error_callback(success.error);
    }

    return success.success;
}

std::string Lila::get_last_error() const
{
    return m_interpreter->get_last_error();
}

void Lila::on_success(std::function<void()> callback)
{
    m_success_callback = std::move(callback);
}

void Lila::on_error(std::function<void(const std::string&)> callback)
{
    m_error_callback = std::move(callback);
}

} // namespace Lila
