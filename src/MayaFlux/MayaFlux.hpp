#pragma once

#include "MayaFlux/API/Core.hpp"

#include "MayaFlux/API/Graph.hpp"

#include "MayaFlux/API/Chronie.hpp"

/**
 * @namespace MayaFlux
 * @brief Main namespace for the Maya Flux audio engine
 *
 * This namespace provides convenience wrappers around the core functionality of the Maya Flux
 * audio engine. These wrappers simplify access to the centrally managed components and common
 * operations, making it easier to work with the engine without directly managing the Engine
 * instance.
 *
 * All functions in this namespace operate on the default Engine instance and its managed
 * components. For custom or non-default components, use their specific handles and methods
 * directly rather than these wrappers.
 */
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
double get_uniform_random(double start = 0, double end = 1);

/**
 * @brief Generates a gaussian (normal) random number
 * @param start Lower bound for scaling
 * @param end Upper bound for scaling
 * @return Random number with gaussian distribution
 *
 * Uses the random number generator from the default engine.
 */
double get_gaussian_random(double start = 0, double end = 1);

/**
 * @brief Generates an exponential random number
 * @param start Lower bound for scaling
 * @param end Upper bound for scaling
 * @return Random number with exponential distribution
 *
 * Uses the random number generator from the default engine.
 */
double get_exponential_random(double start = 0, double end = 1);

/**
 * @brief Generates a poisson random number
 * @param start Lower bound for scaling
 * @param end Upper bound for scaling
 * @return Random number with poisson distribution
 *
 * Uses the random number generator from the default engine.
 */
double get_poisson_random(double start = 0, double end = 1);

}
