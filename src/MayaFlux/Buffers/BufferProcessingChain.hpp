#pragma once

#include "BufferProcessor.hpp"

namespace MayaFlux::Buffers {

/**
 * @class BufferProcessingChain
 * @brief Advanced pipeline manager for multi-stage buffer transformations with backend optimization
 *
 * BufferProcessingChain organizes multiple BufferProcessor objects into sophisticated transformation
 * pipelines for one or more buffers. This system enables complex multi-stage computational processes
 * to be applied to data in a controlled, deterministic order while leveraging the expanded capabilities
 * of modern BufferProcessors for optimal performance and backend utilization.
 *
 * The chain implements an intelligent directed acyclic graph (DAG) of transformations, maintaining
 * separate processor sequences for each buffer while enabling advanced features:
 *
 * **Backend-Aware Processing:**
 * - Automatic backend optimization based on processor recommendations
 * - Intelligent batching of compatible processors for parallel execution
 * - Dynamic backend switching to optimize processing pipelines
 * - Resource-aware scheduling to prevent backend conflicts
 *
 * **Multi-Modal Data Support:**
 * - Seamless processing of different data types (audio, video, texture) within unified chains
 * - Type-safe processor assignment and validation
 * - Cross-domain transformations between different buffer types
 * - Unified interface for heterogeneous data processing
 *
 * **Performance Optimization:**
 * - Processor compatibility validation and automatic optimization
 * - Complexity-based scheduling for optimal resource utilization
 * - Parallel execution of independent processing stages
 * - Memory layout optimization for improved cache performance
 *
 * Key features:
 * - Enables construction of complex computational pipelines with backend optimization
 * - Supports both parallel and sequential transformation patterns with automatic selection
 * - Preserves transformation order while optimizing execution strategy
 * - Provides special "final" processors for guaranteed post-processing operations
 * - Allows dynamic reconfiguration of transformation pipelines at runtime
 * - Leverages processor agency for optimal backend selection and resource utilization
 */
class MAYAFLUX_API BufferProcessingChain {
public:
    friend class BufferProcessor;

    /**
     * @brief Adds a processor to the transformation pipeline for a specific buffer
     * @param processor Processor to add
     * @param buffer Buffer to associate with this processor
     * @param rejection_reason Optional reason for rejection if the processor cannot be added
     * @return true if the processor was successfully added, false if it was rejected
     *
     * The processor is added to the end of the transformation sequence for the specified buffer.
     * If this is the first processor for the buffer, a new sequence is created. The chain
     * performs intelligent validation and optimization:
     *
     * - **Compatibility Validation**: Ensures the processor can handle the buffer's data type
     * - **Backend Analysis**: Analyzes processor backend preferences for optimization opportunities
     * - **Pipeline Optimization**: May reorder or batch processors for improved performance
     * - **Resource Planning**: Allocates necessary resources for the processor's execution
     */
    bool add_processor(const std::shared_ptr<BufferProcessor>& processor, const std::shared_ptr<Buffer>& buffer, std::string* rejection_reason = nullptr);

    /**
     * @brief Removes a processor from the pipeline for a specific buffer
     * @param processor Processor to remove
     * @param buffer Buffer to remove the processor from
     *
     * If the processor is found in the buffer's transformation sequence, it is removed
     * and its on_detach method is called. The chain also performs cleanup optimization:
     *
     * - **Resource Cleanup**: Ensures all processor resources are properly released
     * - **Pipeline Reoptimization**: Rebuilds optimization plans without the removed processor
     * - **Backend Restoration**: Restores default backends if the processor was overriding them
     */
    void remove_processor(const std::shared_ptr<BufferProcessor>& processor, const std::shared_ptr<Buffer>& buffer);

    /**
     * @brief Applies the transformation pipeline to a buffer with intelligent execution
     * @param buffer Buffer to transform
     *
     * Applies each processor in the buffer's sequence using an optimized execution strategy.
     * The chain leverages processor capabilities for maximum performance:
     *
     * - **Backend Optimization**: Uses processor-recommended backends when beneficial
     * - **Parallel Execution**: Executes compatible processors in parallel when possible
     * - **Resource Management**: Optimally allocates CPU, GPU, and memory resources
     * - **Error Handling**: Provides robust error recovery and fallback mechanisms
     *
     * This does not include the final processor, which must be applied separately with
     * process_final() to ensure proper pipeline completion.
     */
    void process(const std::shared_ptr<Buffer>& buffer);

    /**
     * @brief Applies preprocessors, processing chain, post processors and final processors sequentially to a buffer
     * @param buffer Buffer to transform
     *
     * Use this when explicit control of order is not needed, and you want to ensure that all stages
     * of the processing pipeline are applied in a strict sequence. This method guarantees that
     * preprocessors, main processors, postprocessors, and final processors are executed one after
     * the other, maintaining the exact order of operations as defined in the chain.
     */
    void process_complete(const std::shared_ptr<Buffer>& buffer);

    /**
     * @brief Sets a preprocessor to be applied before the main pipeline
     * @param processor Preprocessor to add
     * @param buffer Buffer to associate with this preprocessor
     *
     * The preprocessor is applied before all regular processors when process() is called.
     * This is useful for initial data preparation steps that must occur prior to the
     * main transformation sequence, such as format conversion, normalization, or validation.
     * NOTE: This runs after the buffer's own default processor. If you wish this to be the
     * preprocessor, remove the default processor first.
     * This is done to allow buffers to configure their own default processing behavior.
     * i.e NodeBuffer WILL acquire node data using its default processor before any processing chain preprocessor.
     */
    void add_preprocessor(const std::shared_ptr<BufferProcessor>& processor, const std::shared_ptr<Buffer>& buffer);

    /**
     * @brief Sets a postprocessor to be applied after the main pipeline
     * @param processor Postprocessor to add
     * @param buffer Buffer to associate with this postprocessor
     *
     * The postprocessor is applied after all regular processors when process() is called.
     * This is useful for final data adjustments that must occur immediately after the
     * main transformation sequence, such as clamping values, applying effects, or cleanup.
     * NOTE: This is different from the final processor, and runs before it.
     */
    void add_postprocessor(const std::shared_ptr<BufferProcessor>& processor, const std::shared_ptr<Buffer>& buffer);

    /**
     * @brief Sets a special processor to be applied after the main pipeline
     * @param processor Final processor to add
     * @param buffer Buffer to associate with this final processor
     *
     * The final processor is applied after all regular processors when process_final() is called.
     * This is essential for operations like normalization, boundary enforcement, format conversion,
     * or validation that must be applied as the last step in a transformation pipeline, regardless
     * of the optimization strategies used for the main processing sequence.
     */
    void add_final_processor(const std::shared_ptr<BufferProcessor>& processor, const std::shared_ptr<Buffer>& buffer);

    /**
     * @brief Checks if a buffer has any processors in its pipeline
     * @param buffer Buffer to check
     * @return true if the buffer has processors, false otherwise
     *
     * This method enables dynamic pipeline management and optimization decisions
     * based on the presence of processing stages for specific buffers.
     */
    bool has_processors(const std::shared_ptr<Buffer>& buffer) const;

    /**
     * @brief Gets all processors in a buffer's transformation pipeline
     * @param buffer Buffer to get processors for
     * @return Constant reference to the vector of processors
     *
     * Returns an empty vector if the buffer has no processors. This provides
     * access to the processor sequence for analysis, optimization, or debugging
     * purposes while maintaining the integrity of the processing pipeline.
     */
    const std::vector<std::shared_ptr<BufferProcessor>>& get_processors(const std::shared_ptr<Buffer>& buffer) const;

    /**
     * @brief Gets the entire transformation pipeline structure
     * @return Map of buffers to their processor sequences
     *
     * This provides access to the internal structure of the pipeline, mapping each
     * buffer to its sequence of transformation processors. Essential for pipeline
     * analysis, optimization planning, and system introspection capabilities.
     */
    inline std::unordered_map<std::shared_ptr<Buffer>, std::vector<std::shared_ptr<BufferProcessor>>> get_chain() const { return m_buffer_processors; }

    /**
     * @brief Combines another processing pipeline into this one with optimization
     * @param other Chain to merge into this one
     *
     * Adds all processors from the other chain to this one, preserving their buffer
     * associations and order while performing intelligent optimization. This enables
     * the composition of complex transformation pipelines from simpler, reusable components:
     *
     * - **Compatibility Analysis**: Validates that merged processors are compatible
     * - **Optimization Opportunities**: Identifies potential performance improvements in the combined pipeline
     * - **Resource Consolidation**: Optimizes resource usage across the merged processors
     * - **Backend Harmonization**: Resolves any backend conflicts between the chains
     */
    void merge_chain(const std::shared_ptr<BufferProcessingChain>& other);

    /**
     * @brief Applies the preprocessor to a buffer
     * @param buffer Buffer to preprocess
     *
     * If the buffer has a preprocessor, it is applied before the main processing sequence.
     * This is useful for initial data preparation steps that must occur prior to the
     * main transformation sequence, such as format conversion, normalization, or validation.
     */
    void preprocess(const std::shared_ptr<Buffer>& buffer);

    /**
     * @brief Applies the postprocessor to a buffer
     * @param buffer Buffer to postprocess
     *
     * If the buffer has a postprocessor, it is applied after the main processing sequence.
     * This is useful for final data adjustments that must occur immediately after the
     * main transformation sequence, such as clamping values, applying effects, or cleanup.
     */
    void postprocess(const std::shared_ptr<Buffer>& buffer);

    /**
     * @brief Applies the final processor to a buffer with guaranteed execution
     * @param buffer Buffer to process
     *
     * If the buffer has a final processor, it is applied with guaranteed execution regardless
     * of any optimization strategies or backend considerations. This is typically called after
     * process() to apply final-stage transformations like normalization, boundary enforcement,
     * or format validation that must complete successfully for pipeline integrity.
     */
    void process_final(const std::shared_ptr<Buffer>& buffer);

    /**
     * @brief Sets the preferred processing token for this chain
     * @param token Preferred processing token to set
     *
     * This method allows the chain to specify a preferred processing domain that influences
     * how processors are executed, including backend selection and execution strategy.
     * The token can be used to optimize the entire pipeline based on the expected data type,
     * processing requirements, and available hardware resources.
     */
    inline void set_preferred_token(ProcessingToken token) { m_token_filter_mask = token; }

    /** * @brief Gets the preferred processing token for this chain
     * @return Current preferred processing token
     *
     * Returns the currently set preferred processing token, which can be used by processors
     * to optimize their execution strategies and backend selections. This token represents the
     * overall processing domain that the chain aims to optimize for.
     */
    inline ProcessingToken get_preferred_token() const
    {
        return m_token_filter_mask;
    }

    /**
     * @brief Sets the token enforcement strategy for this chain
     * @param strategy Token enforcement strategy to set
     *
     * This method allows the chain to specify how the processing token is enforced across the pipeline,
     * including whether to filter processors based on their compatibility with the token. The default
     * strategy is FILTERED, which applies the token only to compatible processors.
     */
    inline void set_enforcement_strategy(TokenEnforcementStrategy strategy)
    {
        m_enforcement_strategy = strategy;
    }

    /**
     * @brief Gets the current token enforcement strategy for this chain
     * @return Current token enforcement strategy
     *
     * Returns the currently set token enforcement strategy, which determines how the processing token
     * is applied to processors in the pipeline. This can influence whether processors are filtered
     * based on their compatibility with the preferred processing token.
     */
    inline TokenEnforcementStrategy get_enforcement_strategy() const
    {
        return m_enforcement_strategy;
    }

    /**
     * @brief Optimizes the processing pipeline for improved performance
     * @param buffer Buffer to optimize the pipeline for
     *
     * Analyzes the current processor sequence and applies various optimization strategies:
     *
     * - **Backend Consolidation**: Groups processors by preferred backend for batched execution
     * - **Parallel Execution Planning**: Identifies processors that can run concurrently
     * - **Memory Layout Optimization**: Optimizes data access patterns for cache efficiency
     * - **Resource Balancing**: Balances processor load across available hardware resources
     */
    void optimize_for_tokens(const std::shared_ptr<Buffer>& buffer);

    /**
     * @brief Analyzes token compatibility across all processors in the chain
     * @return Vector of TokenCompatibilityReport objects summarizing processor compatibility
     *
     * This method generates a detailed report on how each processor in the chain
     * aligns with the preferred processing token, including compatibility status,
     * enforcement strategy, and any processors that will be skipped or pending removal.
     * Useful for debugging, optimization planning, and ensuring pipeline integrity.
     */
    std::vector<TokenCompatibilityReport> analyze_token_compatibility() const;

    /** * @brief Validates all processors in the chain against the preferred processing token
     * @param incompatibility_reasons Optional vector to store reasons for any incompatibilities
     * @return true if all processors are compatible with the preferred token, false otherwise
     *
     * This method checks each processor in the chain against the preferred processing token,
     * ensuring that all processors can execute under the current backend and execution strategy.
     * If any incompatibilities are found, they can be reported through the provided vector.
     */
    bool validate_all_processors(std::vector<std::string>* incompatibility_reasons = nullptr) const;

    /**
     * @brief Enforces the chain's preferred processing token on all processors
     *
     * This method ensures that all processors in the chain are compatible with the
     * preferred processing token, applying any necessary optimizations or removals
     * of incompatible processors. It is typically called after setting a new preferred
     * token or changing the enforcement strategy to ensure the pipeline remains valid.
     */
    void enforce_chain_token_on_processors();

    inline bool has_pending_operations() const
    {
        return m_pending_count.load(std::memory_order_relaxed) > 0;
    }

    /**
     * @brief Gets a processor of a specific type from the buffer's processing pipeline
     * @tparam T Type of the processor to retrieve
     * @param buffer Buffer to get the processor from
     * @return Shared pointer to the processor if found, nullptr otherwise
     *
     * This method searches for a processor of the specified type in the buffer's
     * transformation sequence. If found, it returns a shared pointer to the processor,
     * allowing type-safe access to specialized functionality.
     */
    template <typename T>
    std::shared_ptr<T> get_processor(const std::shared_ptr<Buffer>& buffer) const
    {
        auto processors = get_processors(buffer);

        for (auto& processor : processors) {
            if (auto t_processor = std::dynamic_pointer_cast<T>(processor)) {
                return t_processor;
            }
        }
        return nullptr;
    }

protected:
    bool add_processor_direct(const std::shared_ptr<BufferProcessor>& processor, const std::shared_ptr<Buffer>& buffer, std::string* rejection_reason = nullptr);
    void remove_processor_direct(const std::shared_ptr<BufferProcessor>& processor, const std::shared_ptr<Buffer>& buffer);

private:
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

    /**
     * @brief Validates the processing token against the chain's preferred token
     */
    void cleanup_rejected_processors(const std::shared_ptr<Buffer>& buffer);

    /**
     * @brief Process pending processor operations
     */
    void process_pending_processor_operations();

    bool queue_pending_processor_op(const std::shared_ptr<BufferProcessor>& processor, const std::shared_ptr<Buffer>& buffer, bool is_addition, std::string* rejection_reason = nullptr);

    /**
     * @brief Map of buffers to their processor sequences
     *
     * Each buffer has its own vector of processors that are applied in order when
     * the buffer is processed. The sequence may be optimized for performance while
     * maintaining the logical order of transformations.
     */
    std::unordered_map<std::shared_ptr<Buffer>, std::vector<std::shared_ptr<BufferProcessor>>> m_buffer_processors;

    /**
     * @brief Map of buffers to processors that are conditionally applied
     */
    std::unordered_map<std::shared_ptr<Buffer>, std::unordered_set<std::shared_ptr<BufferProcessor>>> m_conditional_processors;

    /**
     * @brief Map of buffers to processors pending removal
     */
    std::unordered_map<std::shared_ptr<Buffer>, std::unordered_set<std::shared_ptr<BufferProcessor>>> m_pending_removal;

    /**
     * @brief Map of buffers to their preprocessors
     *
     * Each buffer can have one preprocessor that is applied before the main
     * processing sequence to prepare the data.
     */
    std::unordered_map<std::shared_ptr<Buffer>, std::shared_ptr<BufferProcessor>> m_preprocessors;

    /**
     * @brief Map of buffers to their postprocessors
     *
     * Each buffer can have one postprocessor that is applied after the main
     * processing sequence to finalize the data.
     */
    std::unordered_map<std::shared_ptr<Buffer>, std::shared_ptr<BufferProcessor>> m_postprocessors;

    /**
     * @brief Map of buffers to their final processors
     *
     * Each buffer can have one final processor that is applied after the main
     * processing sequence with guaranteed execution, regardless of optimization strategies.
     */
    std::unordered_map<std::shared_ptr<Buffer>, std::shared_ptr<BufferProcessor>> m_final_processors;

    /**
     * @brief Preferred processing token for this chain
     *
     * This token represents the preferred processing domain that influences how processors
     * are executed, including backend selection and execution strategy. It can be used to
     * optimize the entire pipeline based on the expected data type, processing requirements,
     * and available hardware resources.
     */
    mutable ProcessingToken m_token_filter_mask { ProcessingToken::AUDIO_BACKEND };

    /**
     * @brief Token enforcement strategy for this chain
     *
     * This strategy determines how the processing token is enforced across the pipeline,
     * including whether to filter processors based on their compatibility with the token.
     * The default strategy is FILTERED, which applies the token only to compatible processors.
     */
    TokenEnforcementStrategy m_enforcement_strategy { TokenEnforcementStrategy::FILTERED };

    static constexpr size_t MAX_PENDING_PROCESSORS = 32;

    struct PendingProcessorOp {
        std::atomic<bool> active { false };
        std::shared_ptr<BufferProcessor> processor;
        std::shared_ptr<Buffer> buffer;
        bool is_addition { true }; // true = add, false = remove
    };

    std::atomic<bool> m_is_processing;

    PendingProcessorOp m_pending_ops[MAX_PENDING_PROCESSORS];

    std::atomic<uint32_t> m_pending_count { 0 };
};
}
