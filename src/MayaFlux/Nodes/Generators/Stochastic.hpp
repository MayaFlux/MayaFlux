#pragma once
#include "Generator.hpp"

namespace MayaFlux::Nodes::Generator::Stochastics {

/**
 * @class NoiseEngine
 * @brief Computational stochastic signal generator with multiple probability distributions
 *
 * The NoiseEngine generates algorithmic signals based on mathematical probability
 * distributions, serving as a foundational component for generative composition,
 * procedural sound design, and data-driven audio transformation. Unlike deterministic
 * processes, stochastic generators introduce controlled mathematical randomness
 * into computational signal paths.
 *
 * Stochastic processes are fundamental in computational audio for:
 * - Procedural generation of complex timbral structures
 * - Algorithmic composition and generative music systems
 * - Data-driven environmental simulations
 * - Creating emergent sonic behaviors through probability fields
 * - Cross-domain control signal generation (audio influencing visual, haptic, etc.)
 *
 * This implementation supports multiple probability distributions:
 * - Uniform: Equal probability across the entire range
 * - Normal (Gaussian): Bell-shaped distribution centered around the midpoint
 * - Exponential: Higher probability near the start, decreasing exponentially
 *
 * The NoiseEngine can function at any rate - from audio-rate signal generation to
 * control-rate parameter modulation, to event-level algorithmic decision making.
 * It can be integrated with other computational domains (graphics, physics, data)
 * to create cross-domain generative systems.
 */
class NoiseEngine : public Generator {
public:
    /**
     * @brief Constructor for the stochastic generator
     * @param type Distribution type to use (default: uniform distribution)
     *
     * Creates a stochastic generator with the specified probability distribution.
     * The generator is initialized with entropy from the system's
     * random device for non-deterministic behavior across program executions.
     */
    NoiseEngine(Utils::distribution type = Utils::distribution::UNIFORM);

    /**
     * @brief Virtual destructor
     */
    ~NoiseEngine() = default;

    /**
     * @brief Changes the probability distribution type
     * @param type New distribution type to use
     *
     * This allows dynamic reconfiguration of the stochastic behavior,
     * enabling real-time transformation of the generator's mathematical properties.
     */
    inline void set_type(Utils::distribution type)
    {
        m_type = type;
    }

    /**
     * @brief Generates a single stochastic value
     * @param input Input value (can be used for distribution modulation)
     * @return Generated stochastic value
     *
     * This method generates a single value according to the current
     * distribution settings. The input parameter can be used to modulate
     * or influence the distribution in advanced applications.
     */
    virtual double process_sample(double input) override;

    /**
     * @brief Generates a stochastic value within a specified range
     * @param start Lower bound of the range
     * @param end Upper bound of the range
     * @return Value within the specified range based on current distribution
     *
     * This method provides precise control over the output range while
     * maintaining the statistical properties of the selected distribution.
     */
    double random_sample(double start, double end);

    /**
     * @brief Generates multiple stochastic values at once
     * @param num_samples Number of values to generate
     * @return Vector of generated values
     *
     * This method efficiently generates multiple values in a single operation,
     * useful for batch processing or filling buffers.
     */
    virtual std::vector<double> processFull(unsigned int num_samples) override;

    /**
     * @brief Generates an array of stochastic values within a specified range
     * @param start Lower bound of the range
     * @param end Upper bound of the range
     * @param num_samples Number of values to generate
     * @return Vector of values within the specified range
     *
     * Generates a collection of values following the current distribution,
     * mapped to the specified numerical range.
     */
    std::vector<double> random_array(double start, double end, unsigned int num_samples);

    /**
     * @brief Visualizes the distribution characteristics
     *
     * Outputs a data visualization of the distribution's statistical properties,
     * useful for understanding the mathematical behavior of the generator.
     */
    virtual void printGraph() override;

    /**
     * @brief Outputs the current configuration parameters
     *
     * Displays the current distribution type, scaling factor, and other
     * mathematical parameters of the generator.
     */
    virtual void printCurrent() override;

    /**
     * @brief Sets the scaling factor for the output values
     * @param amplitude New scaling value
     *
     * This controls the overall magnitude of the generated values.
     * Higher values produce larger numerical outputs, while lower values
     * produce smaller numerical outputs.
     */
    inline void set_amplitude(double amplitude)
    {
        m_amplitude = amplitude;
    }

    /**
     * @brief Sets the variance parameter for normal distribution
     * @param spread New variance value
     *
     * For normal (Gaussian) distribution, this controls the statistical
     * variance of the distribution. Higher values create greater entropy
     * with more values in the extremes, while lower values concentrate
     * values toward the center of the distribution.
     */
    inline void set_normal_spread(double spread)
    {
        m_normal_spread = spread;
    }

private:
    /**
     * @brief Generates a raw value according to the current distribution
     * @return Raw stochastic value before range transformation
     *
     * This internal method applies the selected probability algorithm
     * to generate a value with the appropriate statistical properties.
     */
    double generate_distributed_sample();

    /**
     * @brief Transforms a raw value to fit within the specified range
     * @param sample Raw value from the distribution
     * @param start Lower bound of the target range
     * @param end Upper bound of the target range
     * @return Transformed value within the specified range
     *
     * Different distributions require different mathematical transformations
     * to properly map their output while preserving their statistical properties.
     */
    double transform_sample(double sample, double start, double end) const;

    /**
     * @brief Validates that the specified range is mathematically valid
     * @param start Lower bound of the range
     * @param end Upper bound of the range
     * @throws std::invalid_argument if the range is invalid
     *
     * Ensures that the range parameters satisfy the mathematical constraints
     * of the selected distribution algorithm.
     */
    void validate_range(double start, double end) const;

    /**
     * @brief Mersenne Twister entropy generator
     *
     * A high-quality pseudo-random number algorithm that provides
     * excellent statistical properties for computational applications.
     */
    std::mt19937 m_random_engine;

    /**
     * @brief Current probability distribution algorithm
     */
    Utils::distribution m_type;

    /**
     * @brief Lower bound of the current output range
     */
    double m_current_start;

    /**
     * @brief Upper bound of the current output range
     */
    double m_current_end;

    /**
     * @brief Scaling factor for the output values
     */
    double m_amplitude;

    /**
     * @brief Variance parameter for normal distribution
     *
     * Controls the statistical spread in normal distribution.
     * Higher values increase entropy and distribution width.
     */
    double m_normal_spread;
};

}
