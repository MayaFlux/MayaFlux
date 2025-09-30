#include "Logger.hpp"

namespace MayaFlux::Log {

class Logger::Impl {
public:
    Impl()
        : min_severity_(Severity::INFO)
        , initialized_(false)
    {
        component_filters_.fill(true);
    }

    void init()
    {
        std::lock_guard lock(mutex_);
        if (initialized_)
            return;

        initialized_ = true;
        std::cout << "[MayaFlux::Log] Initialized\n";
    }

    void shutdown()
    {
        std::lock_guard lock(mutex_);
        if (!initialized_)
            return;

        std::cout << "[MayaFlux::Log] Shutdown\n";
        initialized_ = false;
    }

    void log(const LogEntry& entry)
    {
        if (entry.severity < min_severity_.load(std::memory_order_relaxed)) {
            return;
        }

        if (!component_filters_[static_cast<size_t>(entry.component)]) {
            return;
        }

        std::lock_guard lock(mutex_);
        write_to_console(entry);
    }

    void set_min_severity(Severity sev)
    {
        min_severity_.store(sev, std::memory_order_relaxed);
    }

    void set_component_filter(Component comp, bool enabled)
    {
        component_filters_[static_cast<size_t>(comp)] = enabled;
    }

private:
    void write_to_console(const LogEntry& entry)
    {
        std::cout << "[" << Utils::enum_to_string(entry.severity) << "]"
                  << "[" << Utils::enum_to_string(entry.component) << "]"
                  << "[" << Utils::enum_to_string(entry.context) << "] "
                  << entry.message;

        if (entry.location.file_name() != nullptr) {
            std::cout << " (" << entry.location.file_name()
                      << ":" << entry.location.line() << ")";
        }

        std::cout << std::endl;
    }

    std::mutex mutex_;
    std::atomic<Severity> min_severity_;
    std::array<bool, 10> component_filters_;
    bool initialized_;
};

Logger& Logger::instance()
{
    static Logger logger;
    return logger;
}

Logger::Logger()
    : impl_(std::make_unique<Impl>())
{
}
Logger::~Logger() = default;

void Logger::init()
{
    instance().impl_->init();
}

void Logger::shutdown()
{
    instance().impl_->shutdown();
}

void Logger::log(Severity severity, Component component, Context context,
    std::string_view message, std::source_location location)
{
    LogEntry entry(severity, component, context, message, location);
    impl_->log(entry);
}

void Logger::log_rt(Severity severity, Component component, Context context,
    std::string_view message, std::source_location location)
{
    log(severity, component, context, message, location);
}

void Logger::set_min_severity(Severity min_sev)
{
    impl_->set_min_severity(min_sev);
}

void Logger::set_component_filter(Component comp, bool enabled)
{
    impl_->set_component_filter(comp, enabled);
}

} // namespace MayaF
