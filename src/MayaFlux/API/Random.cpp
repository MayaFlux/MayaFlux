#include "Random.hpp"

#include "Core.hpp"
#include "MayaFlux/Core/Engine.hpp"
#include "MayaFlux/Nodes/Generators/Stochastic.hpp"

namespace MayaFlux {

//-------------------------------------------------------------------------
// Random Number Generation
//-------------------------------------------------------------------------

double get_uniform_random(double start, double end)
{
    get_context().get_random_engine()->set_type(Kinesis::distribution::UNIFORM);
    return get_context().get_random_engine()->random_sample(start, end);
}

double get_gaussian_random(double start, double end)
{
    get_context().get_random_engine()->set_type(Kinesis::distribution::NORMAL);
    return get_context().get_random_engine()->random_sample(start, end);
}

double get_exponential_random(double start, double end)
{
    get_context().get_random_engine()->set_type(Kinesis::distribution::EXPONENTIAL);
    return get_context().get_random_engine()->random_sample(start, end);
}

double get_poisson_random(double start, double end)
{
    get_context().get_random_engine()->set_type(Kinesis::distribution::POISSON);
    return get_context().get_random_engine()->random_sample(start, end);
}
}
