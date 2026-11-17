#include "Lila.hpp"

#include "ClangInterpreter.hpp"
#include "Listener.hpp"

#include "Commentator.hpp"

#ifdef MAYAFLUX_PLATFORM_MACOS
#include <CoreFoundation/CoreFoundation.h>
#endif

#ifdef MAYAFLUX_PLATFORM_WINDOWS
#include "MayaFlux/Transitive/Parallel/Dispatch.hpp"
#endif

namespace Lila {

namespace {
    constexpr auto k_poll_interval = std::chrono::milliseconds(100);
}

Lila::Lila()
    : m_interpreter(std::make_unique<ClangInterpreter>())
{
    LILA_DEBUG(Emitter::SYSTEM, "Lila instance created");
}

Lila::~Lila()
{
    stop_listening();
    LILA_DEBUG(Emitter::SYSTEM, "Lila instance destroyed");
}

Result<void> Lila::initialize(bool skip_host_library_load)
{
    LILA_INFO(Emitter::SYSTEM, "Initializing Lila");

    if (!m_interpreter->initialize(skip_host_library_load)) {
        LILA_ERROR(Emitter::SYSTEM, "Failed to initialize interpreter");
        return std::unexpected(m_interpreter->get_last_error());
    }

    LILA_INFO(Emitter::SYSTEM, "Lila initialized successfully");
    return {};
}

Result<void> Lila::listen(const Address& where)
{
    stop_listening();

    auto listener = Listener::create(where);
    listener->set_message_handler([this](std::string_view message) {
        return handle_message(message);
    });

    if (auto started = listener->start(); !started) {
        return started;
    }

    m_listener = std::move(listener);
    return {};
}

void Lila::stop_listening()
{
    if (m_listener) {
        LILA_INFO(Emitter::SYSTEM, "Stopping listener");
        m_listener->stop();
        m_listener.reset();
    }
}

bool Lila::is_listening() const
{
    return m_listener && m_listener->is_running();
}

std::optional<Address> Lila::address() const
{
    if (!m_listener || !m_listener->is_running()) {
        return std::nullopt;
    }
    return m_listener->address();
}

Result<std::string> Lila::handle_message(std::string_view message)
{
    if (message.empty()) {
        return std::unexpected("empty message");
    }

    if (auto evaluated = eval(std::string(message)); !evaluated) {
        return std::unexpected(std::move(evaluated.error()));
    }
    return std::string {};
}

Result<void> Lila::eval(const std::string& code)
{
    const auto result = m_interpreter->eval(code);
    if (!result.success) {
        return std::unexpected(result.error);
    }
    return {};
}

Result<void> Lila::load_library(const std::string& path)
{
    if (!m_interpreter->load_library(path)) {
        return std::unexpected(m_interpreter->get_last_error());
    }
    return {};
}

void* Lila::get_symbol_address(const std::string& name)
{
    return m_interpreter->get_symbol_address(name);
}

void Lila::add_include_path(const std::string& path)
{
    m_interpreter->add_include_path(path);
}

void Lila::add_compile_flag(const std::string& flag)
{
    m_interpreter->add_compile_flag(flag);
}

void Lila::await_shutdown(const std::atomic<bool>* external_flag)
{
    LILA_INFO(Emitter::SYSTEM, "Entering main event loop");

    while (!m_shutdown_requested.load(std::memory_order_acquire) && (!external_flag || external_flag->load(std::memory_order_acquire))) {
        if (!is_listening()) {
            LILA_ERROR(Emitter::SYSTEM, "Listener stopped unexpectedly");
            break;
        }
#ifdef MAYAFLUX_PLATFORM_MACOS
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, std::chrono::duration<double>(k_poll_interval).count(), false);
#else
        std::this_thread::sleep_for(k_poll_interval);
#endif
    }

    LILA_INFO(Emitter::SYSTEM, "Exiting main event loop");
}

void Lila::request_shutdown()
{
    m_shutdown_requested.store(true, std::memory_order_release);

#ifdef MAYAFLUX_PLATFORM_MACOS
    CFRunLoopStop(CFRunLoopGetMain());
#endif
}

} // namespace Lila
