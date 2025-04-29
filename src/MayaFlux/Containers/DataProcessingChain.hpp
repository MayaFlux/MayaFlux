#pragma once

#include "DataProcessor.hpp"
#include "SignalSourceContainer.hpp"

namespace MayaFlux::Containers {

/**
 * @class DataProcessingChain
 * @brief Manages collections of data processors that operate on signal containers.
 *
 * The DataProcessingChain provides a flexible framework for applying multiple processing
 * operations to signal data. Rather than modeling analog signal chains, it implements a
 * data-driven approach where processors can be dynamically added, removed, and selectively
 * applied to specific signal containers.
 *
 * Key features:
 * - Container-specific processing chains
 * - Type-based processor filtering
 * - Tag-based processor organization
 * - Custom filtering of processors during execution
 *
 * This design enables complex signal processing workflows where different types of
 * operations can be applied conditionally based on data characteristics rather than
 * fixed signal paths.
 */
class DataProcessingChain {
public:
    /**
     * @brief Adds a processor to the chain for a specific container.
     * @param processor The data processor to add
     * @param container The signal container the processor will operate on
     * @param tag Optional tag for categorizing processors
     */
    void add_processor(std::shared_ptr<DataProcessor> processor, std::shared_ptr<SignalSourceContainer> container, const std::string& tag = "");

    /**
     * @brief Adds a processor at a specific position in the chain.
     * @param processor The data processor to add
     * @param container The signal container the processor will operate on
     * @param position The position in the processing sequence
     */
    void add_processor_at(std::shared_ptr<DataProcessor> processor,
        std::shared_ptr<SignalSourceContainer> container,
        size_t position);

    /**
     * @brief Removes a processor from a container's chain.
     * @param processor The data processor to remove
     * @param container The signal container to remove the processor from
     */
    void remove_processor(std::shared_ptr<DataProcessor> processor, std::shared_ptr<SignalSourceContainer> container);

    /**
     * @brief Processes a container with all its associated processors.
     * @param container The signal container to process
     */
    void process(std::shared_ptr<SignalSourceContainer> container);

    /**
     * @brief Processes a container with processors of a specific type.
     * @tparam ProcessorType The type of processors to apply
     * @param container The signal container to process
     *
     * This template method allows for selective processing based on processor types,
     * enabling specialized processing paths for different data transformations.
     */
    template <typename ProcessorType>
    inline void process_typed(std::shared_ptr<SignalSourceContainer> container)
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
     * Enables dynamic, runtime filtering of processors based on arbitrary criteria.
     */
    void process_filtered(std::shared_ptr<SignalSourceContainer> container,
        std::function<bool(std::shared_ptr<DataProcessor>)> filter);

    /**
     * @brief Processes a container with processors that have a specific tag.
     * @param container The signal container to process
     * @param tag The tag to filter processors by
     *
     * Allows for logical grouping and selective application of processors.
     */
    void process_tagged(std::shared_ptr<SignalSourceContainer> container, const std::string& tag);

private:
    /**
     * Maps containers to their associated processors in sequence order.
     * This structure enables container-specific processing chains.
     */
    std::unordered_map<std::shared_ptr<SignalSourceContainer>,
        std::vector<std::shared_ptr<DataProcessor>>>
        m_container_processors;

    /**
     * Maps processors to their associated tags for categorization.
     */
    std::unordered_map<std::shared_ptr<DataProcessor>, std::string> m_processor_tags;
};
}
