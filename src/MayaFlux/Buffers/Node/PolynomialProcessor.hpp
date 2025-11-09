#pragma once

#include "MayaFlux/Buffers/BufferProcessor.hpp"
#include "MayaFlux/Nodes/Generators/Polynomial.hpp"

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
class MAYAFLUX_API PolynomialProcessor : public BufferProcessor {
public:
    /**
     * @brief Processing mode for the polynomial processor
     */
    enum class ProcessMode : uint8_t {
        SAMPLE_BY_SAMPLE, ///< Process each sample individually
        BATCH, ///< Process the entire buffer at once
        WINDOWED, ///< Process using a sliding window
        BUFFER_CONTEXT ///< Process each sample with access to buffer history
    };

    // TODO:: Temporary requirment on windows
    PolynomialProcessor() = default;

    /**
     * @brief Creates a new processor that applies polynomial transformations
     * @param mode Processing mode (sample-by-sample, batch, or windowed)
     * @param window_size Size of the sliding window (for WINDOWED mode)
     * @param args Arguments to pass to the Polynomial constructor
     */
    template <typename... Args>
        requires std::constructible_from<Nodes::Generator::Polynomial, Args...>
    PolynomialProcessor(ProcessMode mode = ProcessMode::SAMPLE_BY_SAMPLE, size_t window_size = 64, Args&&... args)
        : m_polynomial(std::make_shared<Nodes::Generator::Polynomial>(std::forward<Args>(args)...))
        , m_process_mode(mode)
        , m_window_size(window_size)
        , m_use_internal(true)
    {
    }

    /**
     * @brief Creates a new processor that applies polynomial transformations
     * @param polynomial Polynomial node to use for processing
     * @param mode Processing mode (sample-by-sample, batch, or windowed)
     * @param window_size Size of the sliding window (for WINDOWED mode)
     *
     * NOTE: Using external Polynomial node implies side effects of any progessing chain the node
     * is connected to. This could mean that the buffer data is not used as input when used node's cached value.
     */
    PolynomialProcessor(const std::shared_ptr<Nodes::Generator::Polynomial>& polynomial, ProcessMode mode = ProcessMode::SAMPLE_BY_SAMPLE, size_t window_size = 64);

    /**
     * @brief Processes an audio buffer using the polynomial function
     * @param buffer Buffer to process
     *
     * Applies the polynomial transformation to the buffer data according
     * to the configured processing mode and parameters.
     */
    void processing_function(std::shared_ptr<Buffer> buffer) override;

    /**
     * @brief Called when the processor is attached to a buffer
     * @param buffer Buffer being attached to
     */
    void on_attach(std::shared_ptr<Buffer> buffer) override;

    /**
     * @brief Called when the processor is detached from a buffer
     * @param buffer Buffer being detached from
     */
    inline void on_detach(std::shared_ptr<Buffer>) override { }

    /**
     * @brief Sets the processing mode
     * @param mode New processing mode
     */
    inline void set_process_mode(ProcessMode mode) { m_process_mode = mode; }

    /**
     * @brief Gets the current processing mode
     * @return Current processing mode
     */
    [[nodiscard]] inline ProcessMode get_process_mode() const { return m_process_mode; }

    /**
     * @brief Sets the window size for windowed processing
     * @param size New window size
     */
    inline void set_window_size(size_t size) { m_window_size = size; }

    /**
     * @brief Gets the current window size
     * @return Current window size
     */
    [[nodiscard]] inline size_t get_window_size() const { return m_window_size; }

    /**
     * @brief Gets the polynomial node used for processing
     * @return Polynomial node used for processing
     */
    [[nodiscard]] inline std::shared_ptr<Nodes::Generator::Polynomial> get_polynomial() const { return m_polynomial; }

    /**
     * @brief Checks if the processor is using the internal polynomial node
     * @return True if using internal polynomial node, false otherwise
     *
     * This is useful when the polynomial node is connected to other nodes
     * and we want to ensure that the processor uses its own internal
     * polynomial node instead of the one provided in the constructor.
     */
    [[nodiscard]] inline bool is_using_internal() const { return m_use_internal; }

    /**
     * @brief Forces the processor to use the internal polynomial node
     *
     * This is useful when the polynomial node is connected to other nodes
     * and we want to ensure that the processor uses its own internal
     * polynomial node instead of the one provided in the constructor.
     */
    template <typename... Args>
        requires std::constructible_from<Nodes::Generator::Polynomial, Args...>
    void force_use_internal(Args&&... args)
    {
        m_pending_polynomial = std::make_shared<Nodes::Generator::Polynomial>(std::forward<Args>(args)...);
    }

    /**
     * @brief Updates the polynomial node used for processing
     * @param polynomial New polynomial node to use
     *
     * NOTE: Using external Polynomial node implies side effects of any progessing chain the node
     * is connected to. This could mean that the buffer data is not used as input when used node's cached value.
     */
    inline void update_polynomial_node(const std::shared_ptr<Nodes::Generator::Polynomial>& polynomial)
    {
        m_pending_polynomial = polynomial;
    }

private:
    std::shared_ptr<Nodes::Generator::Polynomial> m_polynomial; ///< Polynomial node for processing
    ProcessMode m_process_mode {}; ///< Current processing mode
    size_t m_window_size {}; ///< Window size for windowed processing

    bool m_use_internal {}; ///< Whether to use the buffer's internal previous state
    std::shared_ptr<Nodes::Generator::Polynomial> m_pending_polynomial; ///< Internal polynomial node

    /**
     * @brief Processes a span of data using the polynomial function
     * @param data Span of data to process
     */
    void process_span(std::span<double> data);

    /**
     * @brief Processes a single sample using the polynomial function
     * @param sample Sample to process
     */
    void process_single_sample(double& sample);
};

} // namespace MayaFlux::Buffers
