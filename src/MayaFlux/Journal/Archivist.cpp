#include "Archivist.hpp"
#include "RealtimeEntry.hpp"
#include "RingBuffer.hpp"

namespace MayaFlux::Journal {

class Archivist::Impl {
public:
    static constexpr size_t RING_BUFFER_SIZE = 8192;

    Impl()
        : m_min_severity(Severity::INFO)
    {
        m_component_filters.fill(true);
    }

    void init()
    {
        std::lock_guard lock(m_mutex);
        if (m_initialized)
            return;

        m_initialized = true;
        start_worker();
        std::cout << "[MayaFlux::Journal] Initialized\n";
    }

    void shutdown()
    {
        std::lock_guard lock(m_mutex);
        if (!m_initialized)
            return;

        std::cout << "[MayaFlux::Journal] Shutdown\n";
        m_initialized = false;
    }

    void scribe(const JournalEntry& entry)
    {
        if (entry.severity < m_min_severity.load(std::memory_order_relaxed)) {
            return;
        }

        auto comp_idx = static_cast<size_t>(entry.component);
        if (comp_idx >= m_component_filters.size()) {
            return;
        }

        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
        if (!m_component_filters[comp_idx]) {
            return;
        }

        std::lock_guard lock(m_mutex);
        write_to_console(entry);
    }

    void scribe_rt(Severity severity, Component component, Context context, std::string_view message, std::source_location location)
    {
        if (!should_log(severity, component)) {
            return;
        }

        RealtimeEntry entry(severity, component, context, message, location);

        if (m_ring_buffer.try_push(entry)) {
            m_dropped_messages.fetch_add(1, std::memory_order_relaxed);
        }
    }

    void set_min_severity(Severity sev)
    {
        m_min_severity.store(sev, std::memory_order_relaxed);
    }

    void set_component_filter(Component comp, bool enabled)
    {
        auto comp_idx = static_cast<size_t>(comp);
        if (comp_idx >= m_component_filters.size()) {
            return;
        }

        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
        m_component_filters[comp_idx] = enabled;
    }

private:
    [[nodiscard]] bool should_log(Severity severity, Component component) const
    {
        if (severity < m_min_severity.load(std::memory_order_relaxed)) {
            return false;
        }

        auto comp_idx = static_cast<size_t>(component);
        if (comp_idx >= m_component_filters.size()) {
            return false;
        }

        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
        return m_component_filters[comp_idx];
    }

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

    static void write_to_console(const RealtimeEntry& entry)
    {
        std::cout << "[" << Utils::enum_to_string(entry.severity) << "]"
                  << "[" << Utils::enum_to_string(entry.component) << "]"
                  << "[" << Utils::enum_to_string(entry.context) << "] "
                  << entry.message;

        if (entry.file_name != nullptr) {
            std::cout << " (" << entry.file_name
                      << ":" << entry.line << ")";
        }

        std::cout << '\n';
    }

    void start_worker()
    {
        m_worker_running.store(true, std::memory_order_release);
        m_worker_thread = std::thread([this]() { worker_loop(); });
    }

    void stop_worker()
    {
        if (m_worker_thread.joinable()) {
            m_worker_running.store(false, std::memory_order_release);
            m_worker_thread.join();

            drain_ring_buffer();
        }
    }

    void worker_loop()
    {
        using namespace std::chrono_literals;

        while (m_worker_running.load(std::memory_order_acquire)) {
            drain_ring_buffer();
            std::this_thread::sleep_for(100ms);
        }
    }

    void drain_ring_buffer()
    {
        while (auto entry = m_ring_buffer.try_pop()) {
            std::lock_guard lock(m_mutex);
            write_to_console(*entry);
        }

        auto dropped = m_dropped_messages.exchange(0, std::memory_order_acquire);
        if (dropped > 0) {
            std::lock_guard lock(m_mutex);
            std::cout << "[MayaFlux::Journal] WARNING: Dropped " << dropped
                      << " realtime log messages (buffer full)\n";
        }
    }

    std::mutex m_mutex;
    std::atomic<Severity> m_min_severity;
    std::array<bool, magic_enum::enum_count<Component>()> m_component_filters {};
    bool m_initialized {};

    RingBuffer<RealtimeEntry, RING_BUFFER_SIZE> m_ring_buffer;
    std::atomic<bool> m_worker_running;
    std::thread m_worker_thread;
    std::atomic<uint64_t> m_dropped_messages { 0 };
};

Archivist& Archivist::instance()
{
    static Archivist archivist;
    return archivist;
}

Archivist::Archivist()
    : m_impl(std::make_unique<Impl>())
{
}

Archivist::~Archivist() = default;

void Archivist::init()
{
    instance().m_impl->init();
}

void Archivist::shutdown()
{
    instance().m_impl->shutdown();
}

void Archivist::scribe(Severity severity, Component component, Context context,
    std::string_view message, std::source_location location)
{
    JournalEntry entry(severity, component, context, message, location);
    m_impl->scribe(entry);
}

void Archivist::scribe_rt(Severity severity, Component component, Context context,
    std::string_view message, std::source_location location)
{
    m_impl->scribe_rt(severity, component, context, message, location);
}

void Archivist::set_min_severity(Severity min_sev)
{
    m_impl->set_min_severity(min_sev);
}

void Archivist::set_component_filter(Component comp, bool enabled)
{
    m_impl->set_component_filter(comp, enabled);
}

} // namespace MayaFlux::Journal
