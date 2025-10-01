#include "Archivist.hpp"

namespace MayaFlux::Journal {

class Archivist::Impl {
public:
    Impl()
        : min_severity_(Severity::INFO)
    {
        component_filters_.fill(true);
    }

    void init()
    {
        std::lock_guard lock(mutex_);
        if (initialized_)
            return;

        initialized_ = true;
        std::cout << "[MayaFlux::Journal] Initialized\n";
    }

    void shutdown()
    {
        std::lock_guard lock(mutex_);
        if (!initialized_)
            return;

        std::cout << "[MayaFlux::Journal] Shutdown\n";
        initialized_ = false;
    }

    void scribe(const JournalEntry& entry)
    {
        if (entry.severity < min_severity_.load(std::memory_order_relaxed)) {
            return;
        }

        auto comp_idx = static_cast<size_t>(entry.component);
        if (comp_idx >= component_filters_.size()) {
            return;
        }

        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
        if (!component_filters_[comp_idx]) {
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
        auto comp_idx = static_cast<size_t>(comp);
        if (comp_idx >= component_filters_.size()) {
            return;
        }

        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
        component_filters_[comp_idx] = enabled;
    }

private:
    static void write_to_console(const JournalEntry& entry)
    {
        std::cout << "[" << Utils::enum_to_string(entry.severity) << "]"
                  << "[" << Utils::enum_to_string(entry.component) << "]"
                  << "[" << Utils::enum_to_string(entry.context) << "] "
                  << entry.message;

        if (entry.location.file_name() != nullptr) {
            std::cout << " (" << entry.location.file_name()
                      << ":" << entry.location.line() << ")";
        }

        std::cout << '\n';
    }

    std::mutex mutex_;
    std::atomic<Severity> min_severity_;
    std::array<bool, magic_enum::enum_count<Component>()> component_filters_ {};
    bool initialized_ {};
};

Archivist& Archivist::instance()
{
    static Archivist archivist;
    return archivist;
}

Archivist::Archivist()
    : impl_(std::make_unique<Impl>())
{
}

Archivist::~Archivist() = default;

void Archivist::init()
{
    instance().impl_->init();
}

void Archivist::shutdown()
{
    instance().impl_->shutdown();
}

void Archivist::scribe(Severity severity, Component component, Context context,
    std::string_view message, std::source_location location)
{
    JournalEntry entry(severity, component, context, message, location);
    impl_->scribe(entry);
}

void Archivist::scribe_rt(Severity severity, Component component, Context context,
    std::string_view message, std::source_location location)
{
    scribe(severity, component, context, message, location);
}

void Archivist::set_min_severity(Severity min_sev)
{
    impl_->set_min_severity(min_sev);
}

void Archivist::set_component_filter(Component comp, bool enabled)
{
    impl_->set_component_filter(comp, enabled);
}

} // namespace MayaFlux::Journal
