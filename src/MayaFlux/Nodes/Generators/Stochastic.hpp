#pragma once
#include "Generator.hpp"

namespace MayaFlux::Nodes::Generator::Stochastics {

class NoiseEngine : public Generator {
public:
    NoiseEngine(Utils::distribution type = Utils::distribution::UNIFORM);

    ~NoiseEngine() = default;

    inline void set_type(Utils::distribution type)
    {
        m_type = type;
    }

    virtual double process_sample(double input) override;

    double random_sample(double start, double end);

    virtual std::vector<double> processFull(unsigned int num_samples) override;

    std::vector<double> random_array(double start, double end, unsigned int num_samples);

    virtual void printGraph() override;

    virtual void printCurrent() override;

private:
    double generate_distributed_sample();
    double transform_sample(double sample, double start, double end) const;
    void validate_range(double start, double end) const;

    std::mt19937 m_random_engine;
    Utils::distribution m_type;
    double m_current_start = -1.0;
    double m_current_end = 1.0;
};

}
