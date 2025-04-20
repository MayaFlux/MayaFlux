#pragma once

#include "MayaFlux/Utils.hpp"
#include "Operators.hpp"

namespace MayaFlux::Tasks {
class EventChain {
public:
    EventChain();
    EventChain(Core::Scheduler::TaskScheduler& scheduler);

    EventChain& then(std::function<void()> action, double delay_seconds = 0.f);

    void start();

private:
    struct TimedEvent {
        std::function<void()> action;
        double delay_seconds;
    };
    std::vector<TimedEvent> m_events;
    Core::Scheduler::TaskScheduler& m_Scheduler;
    std::shared_ptr<Core::Scheduler::SoundRoutine> m_routine;
};

class ActionToken {
public:
    ActionToken(std::shared_ptr<Nodes::Node> _node);
    ActionToken(double _seconds);
    ActionToken(std::function<void()> _func);

    Utils::ActionType type;
    std::shared_ptr<Nodes::Node> node;
    std::function<void()> func;
    double seconds = 0.f;
};

class Sequence {
public:
    Sequence& operator>>(const ActionToken& token);
    void execute();

    void execute(std::shared_ptr<Nodes::NodeGraphManager> node_manager, std::shared_ptr<Core::Scheduler::TaskScheduler> scheduler);

private:
    std::vector<ActionToken> tokens;
};
}
