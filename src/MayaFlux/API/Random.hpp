#pragma once

namespace MayaFlux {

namespace Kinesis::Stochastic {
    class Stochastic;
}

//-------------------------------------------------------------------------
// Random Number Generation
//-------------------------------------------------------------------------

/**
 * @brief Generates a uniform random number
 * @param start Lower bound (inclusive)
 * @param end Upper bound (exclusive)
 * @return Random number with uniform distribution
 *
 * Uses the random number generator from the default engine.
 */
MAYAFLUX_API double get_uniform_random(double start = 0, double end = 1);

/**
 * @brief Generates a gaussian (normal) random number
 * @param start Lower bound for scaling
 * @param end Upper bound for scaling
 * @return Random number with gaussian distribution
 *
 * Uses the random number generator from the default engine.
 */
MAYAFLUX_API double get_gaussian_random(double start = 0, double end = 1);

/**
 * @brief Generates an exponential random number
 * @param start Lower bound for scaling
 * @param end Upper bound for scaling
 * @return Random number with exponential distribution
 *
 * Uses the random number generator from the default engine.
 */
MAYAFLUX_API double get_exponential_random(double start = 0, double end = 1);

/**
 * @brief Generates a poisson random number
 * @param start Lower bound for scaling
 * @param end Upper bound for scaling
 * @return Random number with poisson distribution
 *
 * Uses the random number generator from the default engine.
 */
MAYAFLUX_API double get_poisson_random(double start = 0, double end = 1);

/**
 * @brief Get reference to the default random engine
 * @return Reference to the Stochastic engine instance
 *
 * Provides access to the default random engine for advanced usage and configuration.
 */
MAYAFLUX_API Kinesis::Stochastic::Stochastic& get_random_engine();

/**
 * @brief Generates a Brownian motion value
 * @param start Starting value for Brownian motion
 * @param end Upper bound for scaling
 * @return Next value in the Brownian motion sequence
 *
 * Uses the random number generator from the default engine configured for Brownian motion.
 * Successive calls will evolve the internal state of the Brownian motion generator, creating a random walk effect.
 * The "start" parameter can be used to set the initial position of the Brownian motion,
 * while the "end" parameter can be used to scale the output range.
 * The generator will maintain its state across calls, allowing for continuous evolution of the Brownian motion sequence.
 */
MAYAFLUX_API double get_brownian_motion(double start, double end);

}
