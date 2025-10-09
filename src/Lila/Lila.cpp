#include "Lila.hpp"
#include "JITCompiler.hpp"

namespace Lila {

Lila::Lila()
    : m_compiler(std::make_unique<JITCompiler>())
{
}

Lila::~Lila() = default;

bool Lila::initialize()
{
    return m_compiler->initialize();
}

bool Lila::eval(const std::string& code)
{
    bool success = m_compiler->compile_and_execute(code);

    if (success && m_success_callback) {
        m_success_callback();
    } else if (!success && m_error_callback) {
        m_error_callback(m_compiler->get_last_error());
    }

    return success;
}

std::string Lila::get_last_error() const
{
    return m_compiler->get_last_error();
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
