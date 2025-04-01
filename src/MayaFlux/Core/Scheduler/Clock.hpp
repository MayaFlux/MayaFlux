#pragma once

#include "Promise.hpp"

namespace MayaFlux::Core::Scheduler {

class SampleClock {
public:
    SampleClock(unsigned int sample_rate = 48000);

    void tick(unsigned int samples = 1);
    unsigned int current_sample() const;
    double current_time() const;
    unsigned int sample_rate() const;

private:
    unsigned int m_sample_rate;
    u_int64_t m_current_sample;
};

}
