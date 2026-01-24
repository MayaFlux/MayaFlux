#pragma once

#include "DataProcessor.hpp"

namespace MayaFlux::Kakshya {

/**
 * @class DataProcessingChain
 * @brief Manages collection of data processors or flexible, composable pipelines.
 *
 * DataProcessingChain orchestrates collections of DataProcessor objects, enabling the construction
 * of modular, extensible, and container-specific processing pipelines. Unlike traditional analog
 * "signal chains," this class is designed for digital-first workflows, supporting dynamic, data-driven
 * processing scenarios unconstrained by fixed or linear analog metaphors.
 *
 * Key features:
 * - **Container-specific chains:** Each SignalSourceContainer can have its own unique sequence of processors,
 *   supporting heterogeneous and context-aware processing across the system.
 * - **Dynamic composition:** Processors can be added, removed, or reordered at runtime, enabling adaptive
 *   workflows and on-the-fly reconfiguration.
 * - **Type-based and tag-based filtering:** Processors can be grouped and selectively applied based on type
 *   or user-defined tags, supporting advanced routing, conditional processing, and logical grouping.
 * - **Custom filtering:** Arbitrary filter functions allow for runtime selection of processors based on
 *   data characteristics, state, or external criteria.
 * - **Composable with digital-first nodes, routines, and buffer systems:** Designed to integrate seamlessly
 *   with the broader Maya Flux ecosystem, enabling robust, data-driven orchestration of complex workflows.
 *
 * This abstraction enables advanced scenarios such as:
 * - Multi-stage, container-specific data transformation pipelines
 * - Selective or conditional processing based on data type, tag, or runtime state
 * - Integration of analysis, transformation, and feature extraction in a unified pipeline
 * - Dynamic adaptation to changing data, user interaction, or system state
 *
 * DataProcessingChain is foundational for building scalable, maintainable, and future-proof
 * data processing architectures in digital-first, data-driven applications.
 */
class MAYAFLUX_API DataProcessingChain {
public:
    DataProcessingChain() = default;

    /**
     * @brief Adds a processor to the chain for a specific container.
     * @param processor The data processor to add
     * @param container The signal container the processor will operate on
     * @param tag Optional tag for categorizing processors (enables logical grouping and selective execution)
     *
     * Processors are appended to the end of the container's processing sequence by default.
     */
    void add_processor(const std::shared_ptr<DataProcessor>& processor, const std::shared_ptr<SignalSourceContainer>& container, const std::string& tag = "");

    /**
     * @brief Adds a processor at a specific position in the chain.
     * @param processor The data processor to add
     * @param container The signal container the processor will operate on
     * @param position The position in the processing sequence (0-based index)
     *
     * Enables precise control over processing order for advanced workflows.
     */
    void add_processor_at(const std::shared_ptr<DataProcessor>& processor,
        const std::shared_ptr<SignalSourceContainer>& container,
        size_t position);

    /**
     * @brief Removes a processor from a container's chain.
     * @param processor The data processor to remove
     * @param container The signal container to remove the processor from
     *
     * Supports dynamic reconfiguration and resource management.
     */
    void remove_processor(const std::shared_ptr<DataProcessor>& processor, const std::shared_ptr<SignalSourceContainer>& container);

    /**
     * @brief Processes a container with all its associated processors, in sequence.
     * @param container The signal container to process
     *
     * Applies each processor in the container's chain, enabling multi-stage transformation,
     * analysis, or feature extraction.
     */
    void process(const std::shared_ptr<SignalSourceContainer>& container);

    /**
     * @brief Processes a container with processors of a specific type.
     * @tparam ProcessorType The type of processors to apply
     * @param container The signal container to process
     *
     * Enables selective processing based on processor type, supporting specialized
     * data transformations or analysis paths.
     */
    template <typename ProcessorType>
    inline void process_typed(const std::shared_ptr<SignalSourceContainer>& container)
    {
        auto it = m_container_processors.find(container);
        if (it != m_container_processors.end()) {
            for (auto& processor : it->second) {
                if (auto typed_proc = std::dynamic_pointer_cast<ProcessorType>(processor)) {
                    typed_proc->process(container);
                }
            }
        }
    }

    /**
     * @brief Processes a container with processors that match a filter function.
     * @param container The signal container to process
     * @param filter Function that determines which processors to apply
     *
     * Enables dynamic, runtime filtering of processors based on arbitrary criteria,
     * such as processor state, metadata, or external conditions.
     */
    void process_filtered(const std::shared_ptr<SignalSourceContainer>& container,
        const std::function<bool(const std::shared_ptr<DataProcessor>&)>& filter);

    /**
     * @brief Processes a container with processors that have a specific tag.
     * @param container The signal container to process
     * @param tag The tag to filter processors by
     *
     * Allows for logical grouping and selective application of processors, supporting
     * scenarios like feature extraction, effect routing, or conditional processing.
     */
    void process_tagged(const std::shared_ptr<SignalSourceContainer>& container, const std::string& tag);

private:
    /**
     * @brief Maps containers to their associated processors in sequence order.
     *
     * Enables each container to maintain its own independent, ordered processing pipeline.
     */
    std::unordered_map<std::shared_ptr<SignalSourceContainer>,
        std::vector<std::shared_ptr<DataProcessor>>>
        m_container_processors;

    /**
     * @brief Maps processors to their associated tags for categorization and filtering.
     */
    std::unordered_map<std::shared_ptr<DataProcessor>, std::string> m_processor_tags;
};

}
