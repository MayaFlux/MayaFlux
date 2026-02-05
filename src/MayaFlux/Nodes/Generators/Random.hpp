#pragma once
#include "Generator.hpp"

#include "MayaFlux/Kinesis/Stochastic.hpp"

namespace MayaFlux::Nodes::Generator {

/**
 * @class RandomContext
 * @brief Specialized context for stochastic generator callbacks
 *
 * RandomContext extends the base NodeContext to provide detailed information
 * about a stochastic generator's current state to callbacks. It includes the
 * current distribution type, amplitude scaling, range parameters, and statistical
 * configuration values.
 *
 * This rich context enables callbacks to perform sophisticated analysis and
 * monitoring of stochastic behavior, such as:
 * - Tracking statistical properties of generated sequences
 * - Implementing adaptive responses to emergent patterns
 * - Visualizing probability distributions in real-time
 * - Creating cross-domain mappings based on stochastic properties
 * - Detecting and responding to specific statistical conditions
 */
class MAYAFLUX_API RandomContext : public NodeContext {
public:
    /**
     * @brief Constructs a RandomContext with the current generator state
     * @param value Current output sample value
     * @param type Current probability distribution algorithm
     * @param amplitude Current scaling factor for output values
     * @param range_start Lower bound of the current output range
     * @param range_end Upper bound of the current output range
     * @param normal_spread Variance parameter for normal distribution
     *
     * Creates a context object that provides a complete snapshot of the
     * stochastic generator's current state, including its most recent output
     * value and all parameters that define its statistical behavior.
     */
    RandomContext(double value, Kinesis::Stochastic::Algorithm type, double amplitude,
        double range_start, double range_end, double normal_spread)
        : NodeContext(value, typeid(RandomContext).name())
        , distribution_type(type)
        , amplitude(amplitude)
        , range_start(range_start)
        , range_end(range_end)
        , normal_spread(normal_spread)
    {
    }

    /**
     * @brief Current distribution type
     *
     * Identifies which probability algorithm is currently active in the
     * generator (uniform, normal, exponential, etc.). This determines the
     * fundamental statistical properties of the generated sequence.
     */
    Kinesis::Stochastic::Algorithm distribution_type;

    /**
     * @brief Current amplitude scaling factor
     */
    double amplitude;

    /**
     * @brief Current lower bound of the range
     */
    double range_start;

    /**
     * @brief Current upper bound of the range
     */
    double range_end;

    /**
     * @brief Current variance parameter for normal distribution
     */
    double normal_spread;
};

/**
 * @class RandomContextGpu
 */
class MAYAFLUX_API RandomContextGpu : public RandomContext, public GpuVectorData {
public:
    RandomContextGpu(
        double value,
        Kinesis::Stochastic::Algorithm type,
        double amplitude,
        double range_start,
        double range_end,
        double normal_spread,
        std::span<const float> gpu_data)
        : RandomContext(value, type, amplitude, range_start, range_end, normal_spread)
        , GpuVectorData(gpu_data)
    {
        type_id = typeid(RandomContextGpu).name();
    }
};

/**
 * @class Random
 * @brief Node wrapper for Kinesis::Stochastic - signal-rate stochastic generation
 *
 * Provides continuous stochastic signal generation integrated with the node graph system.
 * This is a thin adapter that connects the core Kinesis::Stochastic infrastructure to
 * the processing graph, adding amplitude scaling, callbacks, and GPU context support.
 *
 * For direct mathematical usage outside the node system, use Kinesis::Stochastic::Stochastic directly.
 */
class MAYAFLUX_API Random : public Generator {
public:
    /**
     * @brief Constructor for the stochastic generator
     * @param type Distribution type to use (default: uniform distribution)
     *
     * Creates a stochastic generator with the specified probability distribution.
     * The generator is initialized with entropy from the system's
     * random device for non-deterministic behavior across program executions.
     */
    Random(Kinesis::Stochastic::Algorithm type = Kinesis::Stochastic::Algorithm::UNIFORM);

    ~Random() override = default;

    /**
     * @brief Changes the probability distribution type
     * @param type New distribution type to use
     */
    void set_type(Kinesis::Stochastic::Algorithm type);

    /**
     * @brief Configures distribution parameters
     * @param key Parameter name
     * @param value Parameter value
     *
     * Allows dynamic adjustment of distribution-specific parameters,
     * such as mean and standard deviation for a normal distribution,
     * or lambda for an exponential distribution.
     */
    inline void configure(const std::string& key, std::any value) { m_generator.configure(key, std::move(value)); }

    /**
     * @brief Generates a single stochastic value
     * @param input Input value (can be used for distribution modulation)
     * @return Generated stochastic value
     *
     * This method generates a single value according to the current
     * distribution settings. The input parameter can be used to modulate
     * or influence the distribution in advanced applications.
     */
    double process_sample(double input = 0.) override;

    /**
     * @brief Generates multiple stochastic values at once
     * @param num_samples Number of values to generate
     * @return Vector of generated values
     *
     * This method efficiently generates multiple values in a single operation,
     * useful for batch processing or filling buffers.
     */
    std::vector<double> process_batch(unsigned int num_samples) override;

    /**
     * @brief Sets the variance parameter for normal distribution
     * @param spread New variance value
     *
     * For normal (Gaussian) distribution, this controls the statistical
     * variance of the distribution. Higher values create greater entropy
     * with more values in the extremes, while lower values concentrate
     * values toward the center of the distribution.
     */
    void set_normal_spread(double spread);

    /**
     * @brief Sets the output value range
     * @param start Lower bound of the range
     * @param end Upper bound of the range
     *
     * Defines the minimum and maximum values that the generator can produce.
     * The actual output values will be scaled to fit within this specified range.
     */
    void set_range(double start, double end);

    void printGraph() override { }
    void printCurrent() override { }
    void save_state() override { }
    void restore_state() override { }

protected:
    /**
     * @brief Updates the context object with the current node state
     * @param value The current sample value
     *
     * This method is responsible for updating the NodeContext object
     * with the latest state information from the node. It is called
     * internally whenever a new output value is produced, ensuring that
     * the context reflects the current state of the node for use in callbacks.
     */
    void update_context(double value) override;

    /**
     * @brief Notifies all registered callbacks about a new value
     * @param value The newly generated value
     *
     * This method is called internally whenever a new value is generated,
     * creating the appropriate context and invoking all registered callbacks
     * that should receive notification about this value.
     */
    void notify_tick(double value) override;

    NodeContext& get_last_context() override;

private:
    /**
     * @brief Core stochastic generator instance
     */
    Kinesis::Stochastic::Stochastic m_generator;
    /**
     * @brief Current probability distribution algorithm
     */
    Kinesis::Stochastic::Algorithm m_type;

    /**
     * @brief Lower bound of the current output range
     */
    double m_current_start { -1.0 };

    /**
     * @brief Upper bound of the current output range
     */
    double m_current_end { 1.0 };

    /**
     * @brief Variance parameter for normal distribution
     *
     * Controls the statistical spread in normal distribution.
     * Higher values increase entropy and distribution width.
     */
    double m_normal_spread { 1.0 };

    RandomContext m_context;
    RandomContextGpu m_context_gpu;
};

} // namespace MayaFlux::Nodes::Generator
