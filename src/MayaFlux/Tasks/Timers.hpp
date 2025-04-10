#include "Tasks.hpp"

namespace MayaFlux::Nodes {
class Node;
}

namespace MayaFlux::Tasks {

class Timer {
public:
    Timer(Core::Scheduler::TaskScheduler& scheduler);
    ~Timer() = default;

    void schedule(double delay_seconds, std::function<void()> callback);

    void cancel();

    inline bool is_active() const { return m_active; }

private:
    Core::Scheduler::TaskScheduler& m_Scheduler;
    std::shared_ptr<Core::Scheduler::SoundRoutine> m_routine;
    bool m_active;

    std::function<void()> m_callback;
};

class TimedAction {
public:
    TimedAction(Core::Scheduler::TaskScheduler& scheduler);
    ~TimedAction() = default;

    void execute(std::function<void()> start_func, std::function<void()> end_func, double duration_seconds);

    void cancel();

    bool is_pending() const;

private:
    Core::Scheduler::TaskScheduler& m_Scheduler;
    Timer m_timer;
};

class NodeTimer {
public:
    NodeTimer(Core::Scheduler::TaskScheduler& scheduler);
    ~NodeTimer() = default;

    void play_for(std::shared_ptr<Nodes::Node> node, double duration_seconds, unsigned int channel = 0);

    void play_with_processing(std::shared_ptr<Nodes::Node> node,
        std::function<void(std::shared_ptr<Nodes::Node>)> setup_func,
        std::function<void(std::shared_ptr<Nodes::Node>)> cleanup_func,
        double duration_seconds,
        unsigned int channel = 0);

    void cancel();

    inline bool is_active() const { return m_timer.is_active(); }

private:
    Core::Scheduler::TaskScheduler& m_scheduler;
    Timer m_timer;
    std::shared_ptr<Nodes::Node> m_current_node;
    unsigned int m_current_channel = 0;
};

}
