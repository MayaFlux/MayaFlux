#pragma once

#include "Timers.hpp"

namespace MayaFlux::Tasks {

class TimeOperation {
public:
    inline TimeOperation(double seconds)
        : m_seconds(seconds)
    {
    }

    inline double get_seconds() const { return m_seconds; }

private:
    double m_seconds;
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
