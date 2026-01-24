#pragma once

#include "BufferUtils.hpp"

namespace MayaFlux::Buffers {

class Buffer;

/**
 * @class BufferProcessor
 * @brief Central computational transformation interface for continuous buffer processing
 *
 * BufferProcessor defines the interface for components that transform data stored in buffer objects
 * within the continuous processing domain of the MayaFlux engine. This design follows the Strategy
 * pattern, allowing different algorithmic transformations to be applied to buffers without changing
 * the underlying buffer implementation, while providing processors with significant agency over
 * processing backend selection and execution strategies.
 *
 * Unlike Kakshya::DataProcessor which operates on-demand for arbitrary data sources, BufferProcessor
 * is designed for continuous, cycle-based processing within the engine's real-time processing pipeline.
 * Processors have expanded capabilities including:
 *
 * **Backend Influence and Override Capabilities:**
 * - Processors can influence or override which processing backend a buffer uses
 * - Support for both sequential (CPU) and parallel (GPU) execution strategies
 * - Dynamic backend selection based on data characteristics and processing requirements
 * - Ability to leverage specialized hardware acceleration when available
 *
 * **Data Type Agnostic Processing:**
 * - Can work on any data type supported by the attached buffer (audio, video, texture, etc.)
 * - Automatic adaptation to buffer-specific data formats and layouts
 * - Type-safe processing through buffer interface abstraction
 * - Support for multi-modal data processing within unified pipelines
 *
 * **Advanced Processing Capabilities:**
 * - Mathematical transformations (FFT, convolution, statistical analysis)
 * - Signal processing algorithms (filtering, modulation, synthesis, effects)
 * - Data manipulation (normalization, scaling, interpolation, format conversion)
 * - Cross-domain mappings and transformations between different data types
 * - Real-time analysis and feature extraction
 * - Hardware-accelerated compute operations
 *
 * The processor system provides a flexible framework for block-based computational operations,
 * complementing the sample-by-sample processing of the node system while enabling efficient
 * parallel transformations and backend-specific optimizations. Processors maintain full
 * compatibility with the buffer architecture while providing the agency to optimize
 * processing strategies based on runtime conditions and hardware capabilities.
 */
class MAYAFLUX_API BufferProcessor {
public:
    friend class BufferProcessingChain;

    /**
     * @brief Virtual destructor for proper cleanup of derived classes
     *
     * Ensures proper cleanup of processor resources, including any backend-specific
     * allocations, GPU contexts, or hardware acceleration resources that may have
     * been acquired during the processor's lifetime.
     */
    virtual ~BufferProcessor() = default;

    /**
     * @brief Applies a computational transformation to the data in the provided buffer
     * @param buffer Buffer to transform
     *
     * This is the main transformation method that must be implemented by all concrete
     * processor classes. It applies the processor's algorithm to the data in the buffer,
     * potentially modifying it in-place or generating derived information. The processor
     * has full agency over how the transformation is executed, including:
     *
     * - **Backend Selection**: Choosing between CPU, GPU, or specialized hardware backends
     * - **Execution Strategy**: Sequential vs parallel processing based on data characteristics
     * - **Memory Management**: Optimizing data layout and access patterns for performance
     * - **Resource Utilization**: Leveraging available hardware acceleration when beneficial
     *
     * The method works seamlessly with any data type supported by the buffer interface,
     * automatically adapting to audio samples, video frames, texture data, or other
     * specialized buffer contents while maintaining type safety through the buffer abstraction.
     * This method calls the core processing function defined in derived classes, with atomic
     * thread-safe management of processing state to ensure that concurrent access does
     * not lead to race conditions or data corruption.
     */
    void process(const std::shared_ptr<Buffer>& buffer);

    /**
     * @brief The core processing function that must be implemented by derived classes
     * @param buffer Buffer to process
     *
     * This method is where the actual transformation logic is implemented. It should
     * contain the algorithmic details of how the buffer's data is transformed, analyzed,
     * or processed. The implementation can utilize any backend capabilities available
     * to the processor, including:
     *
     * - **Parallel Processing**: Using multi-threading or GPU compute for large datasets
     * - **Data Transformations**: Applying mathematical operations, filters, or effects
     * - **Feature Extraction**: Analyzing data characteristics for further processing
     *
     * Derived classes must override this method to provide specific processing behavior.
     */
    virtual void processing_function(std::shared_ptr<Buffer> buffer) = 0;

    /**
     * @brief Called when this processor is attached to a buffer
     * @param buffer Buffer this processor is being attached to
     *
     * Provides an opportunity for the processor to initialize buffer-specific state,
     * allocate resources, or perform validation. With expanded processor capabilities,
     * this method can also:
     *
     * - **Analyze Buffer Characteristics**: Examine data type, size, and format requirements
     * - **Select Optimal Backend**: Choose the most appropriate processing backend for the buffer
     * - **Initialize Hardware Resources**: Set up GPU contexts, CUDA streams, or other acceleration
     * - **Configure Processing Parameters**: Adapt algorithm parameters to buffer characteristics
     * - **Establish Processing Strategy**: Determine whether to use sequential or parallel execution
     * - **Validate Compatibility**: Ensure the processor can handle the buffer's data type and format
     *
     * Default implementation does nothing, but derived classes should override this method
     * to leverage the full capabilities of the expanded processor architecture.
     */
    virtual void on_attach(const std::shared_ptr<Buffer>&) { };

    /**
     * @brief Called when this processor is detached from a buffer
     * @param buffer Buffer this processor is being detached from
     *
     * Provides an opportunity for the processor to clean up buffer-specific state or
     * release resources. With expanded processor capabilities, this method should also:
     *
     * - **Release Hardware Resources**: Clean up GPU memory, CUDA contexts, or other acceleration resources
     * - **Finalize Backend Operations**: Ensure all pending backend operations are completed
     * - **Reset Processing State**: Clear any buffer-specific optimization parameters or cached data
     * - **Restore Default Backend**: Return to default processing backend if override was applied
     * - **Synchronize Operations**: Ensure all parallel processing operations have completed
     *
     * Default implementation does nothing, but proper resource management in derived classes
     * is crucial for optimal performance and preventing resource leaks.
     */
    virtual void on_detach(const std::shared_ptr<Buffer>&) { };

    /**
     * @brief Gets the preferred processing backend for this processor
     * @param buffer Buffer that will be processed
     * @return Preferred backend identifier, or default if no preference
     *
     * This method allows processors to influence or override the processing backend
     * used by the buffer. Processors can analyze the buffer's characteristics and
     * current system state to recommend the most appropriate backend:
     *
     * - **CPU_SEQUENTIAL**: For lightweight operations or when parallel overhead exceeds benefits
     * - **CPU_PARALLEL**: For CPU-intensive operations that benefit from multi-threading
     * - **GPU_COMPUTE**: For massively parallel operations suitable for GPU acceleration
     * - **SPECIALIZED_HARDWARE**: For operations optimized for specific hardware (DSP, FPGA, etc.)
     *
     * The buffer management system may use this recommendation to optimize processing
     * performance, though the final backend selection may depend on system availability
     * and resource constraints.
     */
    virtual void set_processing_token(ProcessingToken token)
    {
        validate_token(token);
        m_processing_token = token;
    }

    /**
     * @brief Gets the current processing token for this buffer
     * @return Current processing domain
     */
    virtual ProcessingToken get_processing_token() const { return m_processing_token; }

    /**
     * @brief Checks if this processor can handle the specified buffer type
     * @param buffer Buffer to check compatibility with
     * @return True if the processor can process this buffer type, false otherwise
     *
     * This method enables dynamic processor validation and selection based on buffer
     * characteristics. Processors can examine the buffer's data type, format, size,
     * and other properties to determine compatibility, enabling robust error handling
     * and automatic processor selection in complex processing pipelines.
     */
    virtual bool is_compatible_with(std::shared_ptr<Buffer>) const { return true; }

protected:
    ProcessingToken m_processing_token { ProcessingToken::AUDIO_BACKEND };

private:
    std::atomic<size_t> m_active_processing { 0 };

    /**
     * @brief Internal processing method for non-owning buffer contexts
     * @param buffer Buffer to process
     *
     * This method is used internally by BufferProcessingChain to process buffers
     * that are not owned by the chain itself. It ensures that the processor's
     * processing function is called in a thread-safe manner, managing the
     * active processing state to prevent concurrent access issues.
     */
    void process_non_owning(const std::shared_ptr<Buffer>& buffer);
};

/** * @struct ProcessorTokenInfo
 * @brief Holds information about a processor's compatibility with a buffer's processing token
 *
 * This structure encapsulates the relationship between a buffer processor and its processing token,
 * including whether the processor is compatible with the buffer's preferred processing token,
 * and how it will be handled based on the token enforcement strategy.
 */
struct ProcessorTokenInfo {
    std::shared_ptr<BufferProcessor> processor;
    ProcessingToken processor_token;
    bool is_compatible;
    bool will_be_skipped; // For OVERRIDE_SKIP strategy
    bool pending_removal; // For OVERRIDE_REJECT strategy
};

/** * @struct TokenCompatibilityReport
 * @brief Holds the results of token compatibility analysis for a buffer processing chain
 *
 * This structure provides a comprehensive report on the compatibility of processors within a
 * buffer processing chain, including the preferred processing token, enforcement strategy,
 * and detailed information about each processor's compatibility status.
 */
struct TokenCompatibilityReport {
    std::shared_ptr<Buffer> buffer;
    ProcessingToken chain_preferred_token;
    TokenEnforcementStrategy enforcement_strategy;
    std::vector<ProcessorTokenInfo> processor_infos;

    size_t total_processors() const { return processor_infos.size(); }
    size_t compatible_processors() const
    {
        return std::count_if(processor_infos.begin(), processor_infos.end(),
            [](const auto& info) { return info.is_compatible; });
    }
    size_t skipped_processors() const
    {
        return std::count_if(processor_infos.begin(), processor_infos.end(),
            [](const auto& info) { return info.will_be_skipped; });
    }
    size_t pending_removal_processors() const
    {
        return std::count_if(processor_infos.begin(), processor_infos.end(),
            [](const auto& info) { return info.pending_removal; });
    }
};

}
