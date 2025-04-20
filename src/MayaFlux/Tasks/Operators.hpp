#pragma once

#include "Timers.hpp"

namespace MayaFlux::Tasks {

class TimeOperation {
public:
    TimeOperation(double seconds);

    TimeOperation(double seconds, Core::Scheduler::TaskScheduler& scheduler, Nodes::NodeGraphManager& graph_manager);

    inline double get_seconds() const { return m_seconds; }

private:
    double m_seconds;
    Core::Scheduler::TaskScheduler& m_scheduler;
    Nodes::NodeGraphManager& m_graph_manager;

    friend void operator>>(std::shared_ptr<Nodes::Node>, const TimeOperation&);
};

class DAC {
public:
    static DAC& instance()
    {
        static DAC dac;
        return dac;
    }

    unsigned int channel = 0;

private:
    DAC() = default;
};

void operator>>(std::shared_ptr<Nodes::Node> node, DAC& dac);

void operator>>(std::shared_ptr<Nodes::Node> node, const TimeOperation& time_op);

TimeOperation Time(double seconds);

}
