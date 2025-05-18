#pragma once

#include "MayaFlux/Buffers/BufferProcessor.hpp"
#include "MayaFlux/Nodes/Generators/Polynomial.hpp"
#include <memory>

namespace MayaFlux::Buffers {

/**
 * @class PolynomialProcessor
 * @brief Buffer processor that applies polynomial transformations to audio data
 *
 * This processor connects a Polynomial node to an AudioBuffer, allowing
 * polynomial functions to be applied to buffer data. It supports all three
 * polynomial modes (direct, recursive, and feedforward) and provides
 * configuration options for how the polynomial is applied.
 */
class PolynomialProcessor : public BufferProcessor {
public:
    /**
     * @brief Processing mode for the polynomial processor
     */
    enum class ProcessMode {
        SAMPLE_BY_SAMPLE, ///< Process each sample individually
        BATCH, ///< Process the entire buffer at once
        WINDOWED ///< Process using a sliding window
    };

    /**
     * @brief Creates a new processor that applies polynomial transformations
     * @param polynomial Polynomial node to use for processing
     * @param clear_before_process Whether to reset the buffer before processing
     * @param mode Processing mode (sample-by-sample, batch, or windowed)
     * @param window_size Size of the sliding window (for WINDOWED mode)
     */
    PolynomialProcessor(
        std::shared_ptr<Nodes::Generator::Polynomial> polynomial,
        ProcessMode mode = ProcessMode::SAMPLE_BY_SAMPLE,
        size_t window_size = 64);

    /**
     * @brief Processes an audio buffer using the polynomial function
     * @param buffer Buffer to process
     *
     * Applies the polynomial transformation to the buffer data according
     * to the configured processing mode and parameters.
     */
    void process(std::shared_ptr<AudioBuffer> buffer) override;

    /**
     * @brief Called when the processor is attached to a buffer
     * @param buffer Buffer being attached to
     */
    void on_attach(std::shared_ptr<AudioBuffer> buffer) override;

    /**
     * @brief Called when the processor is detached from a buffer
     * @param buffer Buffer being detached from
     */
    inline void on_detach(std::shared_ptr<AudioBuffer> buffer) override { }

    /**
     * @brief Sets the processing mode
     * @param mode New processing mode
     */
    inline void set_process_mode(ProcessMode mode) { m_process_mode = mode; }

    /**
     * @brief Gets the current processing mode
     * @return Current processing mode
     */
    inline ProcessMode get_process_mode() const { return m_process_mode; }

    /**
     * @brief Sets the window size for windowed processing
     * @param size New window size
     */
    inline void set_window_size(size_t size) { m_window_size = size; }

    /**
     * @brief Gets the current window size
     * @return Current window size
     */
    inline size_t get_window_size() const { return m_window_size; }

private:
    std::shared_ptr<Nodes::Generator::Polynomial> m_polynomial; ///< Polynomial node for processing
    ProcessMode m_process_mode; ///< Current processing mode
    size_t m_window_size; ///< Window size for windowed processing
};

} // namespace MayaFlux::Buffers
