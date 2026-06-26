#pragma once

#include "DataProcessor.hpp"

namespace MayaFlux::Kakshya {

/**
 * @class DataProcessingChain
 * @brief Manages collections of DataProcessor objects as composable, container-specific pipelines.
 *
 * Each SignalSourceContainer maintains its own ordered processor sequence. Processors can be
 * added, removed, or reordered at runtime. All process variants guard against iterator
 * invalidation via an atomic processing flag; removal requests that arrive mid-iteration
 * are deferred and drained after the loop completes.
 */
class MAYAFLUX_API DataProcessingChain {
public:
    DataProcessingChain() = default;

    /**
     * @brief Adds a processor to the end of the chain for a specific container.
     * @param processor The data processor to add.
     * @param container The signal container the processor will operate on.
     * @param tag Optional tag for selective execution via process_tagged().
     */
    void add_processor(
        const std::shared_ptr<DataProcessor>& processor,
        const std::shared_ptr<SignalSourceContainer>& container,
        const std::string& tag = "");

    /**
     * @brief Adds a processor at a specific position in the chain.
     * @param processor The data processor to add.
     * @param container The signal container the processor will operate on.
     * @param position Zero-based insertion index; appends if out of range.
     */
    void add_processor_at(
        const std::shared_ptr<DataProcessor>& processor,
        const std::shared_ptr<SignalSourceContainer>& container,
        size_t position);

    /**
     * @brief Removes a processor from a container's chain.
     * @param processor The data processor to remove.
     * @param container The signal container to remove the processor from.
     *
     * If called while a process variant is iterating, the removal is deferred
     * until the current iteration completes.
     */
    void remove_processor(
        const std::shared_ptr<DataProcessor>& processor,
        const std::shared_ptr<SignalSourceContainer>& container);

    /**
     * @brief Processes a container with all its associated processors in sequence.
     * @param container The signal container to process.
     */
    void process(const std::shared_ptr<SignalSourceContainer>& container);

    /**
     * @brief Processes a container with only the processors matching a filter predicate.
     * @param container The signal container to process.
     * @param filter Predicate returning true for processors that should run.
     */
    void process_filtered(
        const std::shared_ptr<SignalSourceContainer>& container,
        const std::function<bool(const std::shared_ptr<DataProcessor>&)>& filter);

    /**
     * @brief Processes a container with only the processors carrying a specific tag.
     * @param container The signal container to process.
     * @param tag Tag string to match against registered processor tags.
     */
    void process_tagged(
        const std::shared_ptr<SignalSourceContainer>& container,
        const std::string& tag);

    /**
     * @brief Processes a container with only processors of a specific derived type.
     * @tparam ProcessorType Concrete DataProcessor subtype to dispatch to.
     * @param container The signal container to process.
     */
    template <typename ProcessorType>
    void process_typed(const std::shared_ptr<SignalSourceContainer>& container)
    {
        bool expected = false;
        if (!m_is_processing.compare_exchange_strong(expected, true,
                std::memory_order_acquire, std::memory_order_relaxed)) {
            return;
        }

        auto it = m_container_processors.find(container);
        if (it != m_container_processors.end()) {
            for (auto& processor : it->second) {
                if (auto typed = std::dynamic_pointer_cast<ProcessorType>(processor)) {
                    typed->process(container);
                }
            }
        }

        m_is_processing.store(false, std::memory_order_release);
        drain_pending_removals();
    }

private:
    /**
     * @brief Performs immediate removal of a processor from the chain.
     * @param processor The processor to remove.
     * @param container The container whose chain is modified.
     */
    void remove_processor_direct(
        const std::shared_ptr<DataProcessor>& processor,
        const std::shared_ptr<SignalSourceContainer>& container);

    /**
     * @brief Flushes all removals deferred during the most recent process iteration.
     */
    void drain_pending_removals();

    /**
     * @brief Maps each container to its ordered processor sequence.
     */
    std::unordered_map<
        std::shared_ptr<SignalSourceContainer>,
        std::vector<std::shared_ptr<DataProcessor>>>
        m_container_processors;

    /**
     * @brief Maps processors to their optional tag strings.
     */
    std::unordered_map<std::shared_ptr<DataProcessor>, std::string> m_processor_tags;

    /**
     * @brief Removals deferred because they arrived during active iteration.
     *
     * Each entry is a (processor, container) pair drained by drain_pending_removals()
     * after the enclosing process variant returns.
     */
    std::vector<std::pair<std::shared_ptr<DataProcessor>,
        std::shared_ptr<SignalSourceContainer>>>
        m_pending_removal;

    /**
     * @brief Guards all process variants against concurrent or re-entrant iteration.
     *
     * Set to true at the start of any process variant via CAS; cleared after the
     * loop and pending-removal drain complete. remove_processor() checks this flag
     * before deciding whether to act immediately or defer.
     */
    std::atomic<bool> m_is_processing { false };
};

} // namespace MayaFlux::Kakshya
