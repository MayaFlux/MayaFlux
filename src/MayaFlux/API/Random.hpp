#pragma once

namespace MayaFlux {

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

}
