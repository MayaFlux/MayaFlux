#include "DataProcessingChain.hpp"

namespace MayaFlux::Kakshya {

void DataProcessingChain::add_processor(
    const std::shared_ptr<DataProcessor>& processor,
    const std::shared_ptr<SignalSourceContainer>& container,
    const std::string& tag)
{
    m_container_processors[container].emplace_back(processor);
    if (!tag.empty()) {
        m_processor_tags[processor] = tag;
    }
    processor->on_attach(container);
}

void DataProcessingChain::add_processor_at(
    const std::shared_ptr<DataProcessor>& processor,
    const std::shared_ptr<SignalSourceContainer>& container,
    size_t position)
{
    auto& processors = m_container_processors[container];

    if (position >= processors.size()) {
        processors.push_back(processor);
    } else {
        processors.insert(processors.begin() + static_cast<std::ptrdiff_t>(position), processor);
    }

    processor->on_attach(container);
}

void DataProcessingChain::remove_processor(
    const std::shared_ptr<DataProcessor>& processor,
    const std::shared_ptr<SignalSourceContainer>& container)
{
    if (m_is_processing.load(std::memory_order_acquire)) {
        m_pending_removal.emplace_back(processor, container);
        return;
    }

    remove_processor_direct(processor, container);
}

void DataProcessingChain::remove_processor_direct(
    const std::shared_ptr<DataProcessor>& processor,
    const std::shared_ptr<SignalSourceContainer>& container)
{
    auto it = m_container_processors.find(container);
    if (it == m_container_processors.end()) {
        return;
    }

    auto& processors = it->second;
    auto proc_it = std::ranges::find(processors, processor);

    if (proc_it != processors.end()) {
        processor->on_detach(container);
        processors.erase(proc_it);
        m_processor_tags.erase(processor);

        if (processors.empty()) {
            m_container_processors.erase(it);
        }
    }
}

void DataProcessingChain::drain_pending_removals()
{
    for (auto& [processor, container] : m_pending_removal) {
        remove_processor_direct(processor, container);
    }
    m_pending_removal.clear();
}

void DataProcessingChain::process(const std::shared_ptr<SignalSourceContainer>& container)
{
    bool expected = false;
    if (!m_is_processing.compare_exchange_strong(expected, true,
            std::memory_order_acquire, std::memory_order_relaxed)) {
        return;
    }

    auto it = m_container_processors.find(container);
    if (it != m_container_processors.end()) {
        for (auto& processor : it->second) {
            processor->process(container);
        }
    }

    m_is_processing.store(false, std::memory_order_release);
    drain_pending_removals();
}

void DataProcessingChain::process_filtered(
    const std::shared_ptr<SignalSourceContainer>& container,
    const std::function<bool(const std::shared_ptr<DataProcessor>&)>& filter)
{
    bool expected = false;
    if (!m_is_processing.compare_exchange_strong(expected, true,
            std::memory_order_acquire, std::memory_order_relaxed)) {
        return;
    }

    auto it = m_container_processors.find(container);
    if (it != m_container_processors.end()) {
        for (auto& processor : it->second) {
            if (filter(processor)) {
                processor->process(container);
            }
        }
    }

    m_is_processing.store(false, std::memory_order_release);
    drain_pending_removals();
}

void DataProcessingChain::process_tagged(
    const std::shared_ptr<SignalSourceContainer>& container,
    const std::string& tag)
{
    bool expected = false;
    if (!m_is_processing.compare_exchange_strong(expected, true,
            std::memory_order_acquire, std::memory_order_relaxed)) {
        return;
    }

    auto it = m_container_processors.find(container);
    if (it != m_container_processors.end()) {
        for (auto& processor : it->second) {
            auto tag_it = m_processor_tags.find(processor);
            if (tag_it != m_processor_tags.end() && tag_it->second == tag) {
                processor->process(container);
            }
        }
    }

    m_is_processing.store(false, std::memory_order_release);
    drain_pending_removals();
}

} // namespace MayaFlux::Kakshya
