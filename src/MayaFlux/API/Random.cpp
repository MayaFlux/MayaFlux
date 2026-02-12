#include "Random.hpp"

#include "Core.hpp"
#include "MayaFlux/Core/Engine.hpp"
#include "MayaFlux/Kinesis/Stochastic.hpp"

namespace MayaFlux {

Kinesis::Stochastic::Stochastic& get_random_engine()
{
    return *get_context().get_stochastic_engine();
}

double get_uniform_random(double start, double end)
{
    return get_random_engine()(start, end);
}

double get_gaussian_random(double start, double end)
{
    auto& engine = get_random_engine();
    engine.set_algorithm(Kinesis::Stochastic::Algorithm::NORMAL);
    return engine(start, end);
}

double get_exponential_random(double start, double end)
{
    auto& engine = get_random_engine();
    engine.set_algorithm(Kinesis::Stochastic::Algorithm::EXPONENTIAL);
    return engine(start, end);
}

double get_poisson_random(double start, double end)
{
    auto& engine = get_random_engine();
    engine.set_algorithm(Kinesis::Stochastic::Algorithm::POISSON);
    return engine(start, end);
}

double get_brownian_motion(double start, double end)
{
    auto& engine = get_random_engine();
    engine.set_algorithm(Kinesis::Stochastic::Algorithm::BROWNIAN);
    return engine(start, end);
}

}
