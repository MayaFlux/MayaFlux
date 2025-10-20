#pragma once

#include "MayaFlux/Nodes/Node.hpp"

namespace MayaFlux::Nodes::Generator {

/**
 * @class GeneratorContext
 * @brief Specialized context for generator node callbacks
 *
 * GeneratorContext extends the base NodeContext to provide detailed information
 * about a generator's current state to callbacks. It includes fundamental
 * oscillator parameters such as frequency, amplitude, and phase that define
 * the generator's behavior at the moment a sample is produced.
 *
 * This rich context enables callbacks to perform sophisticated analysis and
 * monitoring of generator behavior, such as:
 * - Tracking parameter changes over time
 * - Implementing frequency-dependent processing
 * - Creating visualizations of generator state
 * - Synchronizing multiple generators based on phase relationships
 * - Implementing adaptive processing based on generator characteristics
 */
class MAYAFLUX_API GeneratorContext : public NodeContext {
public:
    /**
     * @brief Constructs a GeneratorContext with the current generator state
     * @param value Current output sample value
     * @param frequency Current oscillation frequency in Hz
     * @param amplitude Current scaling factor for output values
     * @param phase Current phase position in radians
     *
     * Creates a context object that provides a complete snapshot of the
     * generator's current state, including its most recent output value
     * and all parameters that define its oscillation behavior.
     */
    GeneratorContext(double value, double frequency, float amplitude, double phase)
        : NodeContext(value, typeid(GeneratorContext).name())
        , frequency(frequency)
        , amplitude(amplitude)
        , phase(phase)
    {
    }

    /**
     * @brief Current frequency of the generator
     *
     * Represents the oscillation rate in Hertz (cycles per second).
     * This parameter determines the pitch or periodicity of the
     * generated signal.
     */
    double frequency;

    /**
     * @brief Current amplitude of the generator
     */
    float amplitude;

    /**
     * @brief Current phase of the generator
     */
    double phase;
};

/**
 * @class Generator
 * @brief Base class for all signal and pattern generators in Maya Flux
 *
 * Generators are specialized nodes that create numerical sequences from mathematical principles,
 * rather than processing existing signals. They form the foundation of the
 * computational graph by providing the initial patterns that other nodes
 * can then transform, filter, or combine.
 *
 * Unlike transformation nodes that modify input signals, generators typically:
 * - Create sequences based on mathematical formulas or algorithms
 * - Maintain internal state to track progression, phase, or other parameters
 * - Can operate autonomously without any input (though they may accept modulation inputs)
 * - Serve as the origin points in computational processing networks
 *
 * Common types of generators include:
 * - Oscillators (sine, square, sawtooth, triangle waves)
 * - Stochastic generators (various probability distributions)
 * - Sample-based generators (playing back recorded sequences)
 * - Envelope generators (creating amplitude contours)
 *
 * Generators integrate with the node graph system, allowing them to be:
 * - Connected to other nodes using operators like '>>' (chain)
 * - Combined with other nodes using operators like '+' (mix)
 * - Registered with a RootNode for processing
 * - Used as modulation sources for other generators or transformations
 *
 * The Generator class extends the base Node interface with additional
 * methods for visualization and analysis of the generated patterns.
 */
class MAYAFLUX_API Generator : public Node {
public:
    /**
     * @brief Virtual destructor for proper cleanup
     */
    virtual ~Generator() = default;

    /**
     * @brief Sets the generator's amplitude
     * @param amplitude New amplitude value
     *
     * This method updates the generator's amplitude setting,
     * which controls the overall scaling of the generated values.
     */
    inline virtual void set_amplitude(double amplitude) { m_amplitude = amplitude; }

    /**
     * @brief Gets the current base amplitude
     * @return Current amplitude
     */
    inline virtual double get_amplitude() const { return m_amplitude; }

    /**
     * @brief Allows RootNode to process the Generator without using the processed sample
     * @param bMock_process True to mock process, false to process normally
     *
     * NOTE: This has no effect on the behaviour of process_sample (or process_batch).
     * This is ONLY used by the RootNode when processing the node graph.
     * If the output of the Generator needs to be ignored elsewhere, simply discard the return value.
     *
     * Calling process manually can be cumbersome. Using a coroutine just to call process
     * is overkill. This method allows the RootNode to process the Generator without
     * using the processed sample, which is useful for mocking processing.
     */
    inline virtual void enable_mock_process(bool mock_process)
    {
        if (mock_process) {
            atomic_add_flag(m_state, Utils::NodeState::MOCK_PROCESS);
        } else {
            atomic_remove_flag(m_state, Utils::NodeState::MOCK_PROCESS);
        }
    }

    /**
     * @brief Checks if the generator should mock process
     * @return True if the generator should mock process, false otherwise
     */
    inline virtual bool should_mock_process() const
    {
        return m_state.load() & Utils::NodeState::MOCK_PROCESS;
    }

    /**
     * @brief Prints a visual representation of the generated pattern
     *
     * This method should output a visual representation of the
     * generator's output over time, useful for analysis and
     * understanding the pattern characteristics.
     */
    virtual void printGraph() = 0;

    /**
     * @brief Prints the current state and parameters of the generator
     *
     * This method should output the current configuration and state
     * of the generator, including parameters like frequency, amplitude,
     * phase, or any other generator-specific settings.
     */
    virtual void printCurrent() = 0;

protected:
    /**
     * @brief Base amplitude of the generator
     */
    double m_amplitude { 1.0f };
};

}
