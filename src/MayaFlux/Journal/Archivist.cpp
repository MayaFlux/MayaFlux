#include "Archivist.hpp"
#include "RealtimeEntry.hpp"

#include "Ansi.hpp"
#include "Sink.hpp"

#include "MayaFlux/Transitive/Memory/RingBuffer.hpp"

namespace MayaFlux::Journal {

namespace {
    const bool colors_enabled = AnsiColors::initialize_console_colors();
}

class Archivist::Impl {
public:
    static constexpr size_t RING_BUFFER_SIZE = 8192;
    static constexpr size_t RT_RING_BUFFER_SIZE = 4096;

    Impl()
        : m_min_severity(Severity::WARN)
        , m_worker_running(false)
        , m_initialized(false)
        , m_shutdown_in_progress(false)
    {
        for (auto& f : m_component_filters)
            f.store(true, std::memory_order_relaxed);
        for (auto& f : m_context_filters)
            f.store(true, std::memory_order_relaxed);
    }

    ~Impl()
    {
        m_worker_running.store(false, std::memory_order_release);
        if (m_worker_thread.joinable())
            m_worker_thread.join();
    }

    void init()
    {
        if (m_initialized.exchange(true, std::memory_order_acq_rel))
            return;

        m_worker_running.store(true, std::memory_order_release);
        start_worker();
        std::cout << "[MayaFlux::Journal] Initialized\n";
    }

    void shutdown()
    {
        if (!m_initialized.load(std::memory_order_acquire))
            return;
        if (m_shutdown_in_progress.exchange(true, std::memory_order_acq_rel))
            return;

        m_worker_running.store(false, std::memory_order_release);
        if (m_worker_thread.joinable())
            m_worker_thread.join();

        drain_ring_buffer();
        m_initialized.store(false, std::memory_order_release);
        m_shutdown_in_progress.store(false, std::memory_order_release);
        std::cout << "[MayaFlux::Journal] Shutdown\n";
    }

    void scribe(const JournalEntry& entry)
    {
        if (!should_log(entry.severity, entry.component, entry.context))
            return;

        RealtimeEntry rt(entry.severity, entry.component, entry.context,
            entry.message, entry.location);

        while (m_push_lock.test_and_set(std::memory_order_acquire))
            ;
        const bool pushed = m_ring_buffer.push(rt);
        m_push_lock.clear(std::memory_order_release);

        if (!pushed)
            m_dropped_messages.fetch_add(1, std::memory_order_relaxed);
    }

    void scribe_rt(Severity severity, Component component, Context context,
        std::string_view message, std::source_location location)
    {
        if (!should_log(severity, component, context))
            return;

        RealtimeEntry entry(severity, component, context, message, location);
        if (!m_rt_ring_buffer.push(entry))
            m_dropped_messages.fetch_add(1, std::memory_order_relaxed);
    }

    /**
     * @brief Drain all pending ring buffer entries to sinks or console.
     *
     * Lock-free. Safe to call from any thread. The worker calls this on its
     * normal cadence; error() and fatal() call it synchronously before
     * propagating exceptions or aborting to guarantee visibility.
     */
    void drain_ring_buffer()
    {
        while (auto entry = m_rt_ring_buffer.pop()) {
            if (m_sinks.empty()) {
                write_to_console(*entry);
            } else {
                write_to_sinks(*entry);
            }
        }

        while (auto entry = m_ring_buffer.pop()) {
            if (m_sinks.empty()) {
                write_to_console(*entry);
            } else {
                write_to_sinks(*entry);
            }
        }

        const auto dropped = m_dropped_messages.exchange(0, std::memory_order_acq_rel);
        if (dropped > 0) {
            std::cout << "[MayaFlux::Journal] WARNING: Dropped "
                      << dropped << " log messages (buffer full)\n";
        }
    }

    void add_sink(std::unique_ptr<Sink> sink)
    {
        m_sinks.push_back(std::move(sink));
    }

    void clear_sinks()
    {
        m_sinks.clear();
    }

    void set_min_severity(Severity sev)
    {
        if (sev == Severity::NONE)
            return;
        m_min_severity.store(sev, std::memory_order_relaxed);
    }

    void set_component_filter(Component comp, bool enabled)
    {
        auto comp_idx = static_cast<size_t>(comp);
        if (comp_idx >= m_component_filters.size())
            return;

        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
        m_component_filters[comp_idx].store(enabled, std::memory_order_release);
    }

    void set_context_filter(Context ctx, bool enabled)
    {
        auto ctx_idx = static_cast<size_t>(ctx);
        if (ctx_idx >= m_context_filters.size())
            return;

        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
        m_context_filters[ctx_idx].store(enabled, std::memory_order_release);
    }

private:
    [[nodiscard]] bool should_log(Severity severity, Component component, Context context) const
    {
        if (severity != Severity::NONE && severity < m_min_severity.load(std::memory_order_relaxed))
            return false;

        auto comp_idx = static_cast<size_t>(component);
        if (comp_idx >= m_component_filters.size()
            || !m_component_filters[comp_idx].load(std::memory_order_relaxed))
            return false;

        auto ctx_idx = static_cast<size_t>(context);

        return ctx_idx < m_context_filters.size()
            && m_context_filters[ctx_idx].load(std::memory_order_relaxed);
    }

    static void write_to_console(const RealtimeEntry& entry)
    {
        if (colors_enabled) {
            switch (entry.severity) {
            case Severity::TRACE:
                std::cout << AnsiColors::Cyan;
                break;
            case Severity::DEBUG:
                std::cout << AnsiColors::Blue;
                break;
            case Severity::INFO:
                std::cout << AnsiColors::Green;
                break;
            case Severity::WARN:
                std::cout << AnsiColors::Yellow;
                break;
            case Severity::ERROR:
                std::cout << AnsiColors::BrightRed;
                break;
            case Severity::FATAL:
                std::cout << AnsiColors::BrightRed << AnsiColors::White;
                break;
            case Severity::NONE:
            default:
                std::cout << AnsiColors::Reset;
                break;
            }
        }

        std::cout << "[" << Reflect::enum_to_string(entry.severity) << "]" << AnsiColors::Reset;
        if (colors_enabled)
            std::cout << AnsiColors::Magenta;
        std::cout << "[" << Reflect::enum_to_string(entry.component) << "]" << AnsiColors::Reset;
        if (colors_enabled)
            std::cout << AnsiColors::Cyan;
        std::cout << "[" << Reflect::enum_to_string(entry.context) << "]" << AnsiColors::Reset << " ";
        std::cout << entry.message;

        if (entry.file_name != nullptr && entry.line != 0) {
            if (colors_enabled)
                std::cout << AnsiColors::BrightBlue;
            std::cout << " (" << entry.file_name << ":" << entry.line << ")" << AnsiColors::Reset;
        }

        std::cout << '\n';
    }

    void write_to_sinks(const RealtimeEntry& entry)
    {
        for (auto& sink : m_sinks) {
            if (sink->is_available()) {
                try {
                    sink->write(entry);
                } catch (...) {
                    std::cout << "[MayaFlux::Journal] WARNING: sink threw during write, skipping\n";
                }
            }
        }
    }

    void start_worker()
    {
        m_worker_thread = std::thread([this] { worker_loop(); });
    }

    void worker_loop()
    {
        using namespace std::chrono_literals;
        while (m_worker_running.load(std::memory_order_acquire)) {
            drain_ring_buffer();
            std::this_thread::sleep_for(10ms);
        }
        drain_ring_buffer();
    }

    std::vector<std::unique_ptr<Sink>> m_sinks;

    std::atomic<Severity> m_min_severity;
    std::array<std::atomic<bool>, magic_enum::enum_count<Component>()> m_component_filters {};
    std::array<std::atomic<bool>, magic_enum::enum_count<Context>()> m_context_filters {};
    std::atomic<bool> m_initialized;
    std::atomic<bool> m_worker_running;
    std::atomic<bool> m_shutdown_in_progress;
    std::atomic<uint64_t> m_dropped_messages { 0 };

    alignas(64) std::atomic_flag m_push_lock = ATOMIC_FLAG_INIT;
    Memory::MPSCQueue<RealtimeEntry, RT_RING_BUFFER_SIZE> m_rt_ring_buffer;
    Memory::LockFreeQueue<RealtimeEntry, RING_BUFFER_SIZE> m_ring_buffer;
    std::thread m_worker_thread;
};

Archivist& Archivist::instance()
{
    static Archivist archivist;
    return archivist;
}

Archivist::Archivist()
    : m_impl(std::make_unique<Impl>())
{
    m_impl->init();
}

Archivist::~Archivist() = default;

void Archivist::shutdown()
{
    instance().m_impl->shutdown();
}

void Archivist::flush()
{
    m_impl->drain_ring_buffer();
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

void Archivist::scribe_simple(Component component, Context context,
    std::string_view message)
{
    JournalEntry entry(Severity::NONE, component, context, message, std::source_location {});
    m_impl->scribe(entry);
}

void Archivist::add_sink(std::unique_ptr<Sink> sink)
{
    m_impl->add_sink(std::move(sink));
}

void Archivist::clear_sinks()
{
    m_impl->clear_sinks();
}

void Archivist::set_min_severity(Severity min_sev)
{
    m_impl->set_min_severity(min_sev);
}

void Archivist::set_component_filter(Component comp, bool enabled)
{
    m_impl->set_component_filter(comp, enabled);
}

void Archivist::set_context_filter(Context ctx, bool enabled)
{
    m_impl->set_context_filter(ctx, enabled);
}

} // namespace MayaFlux::Journal
