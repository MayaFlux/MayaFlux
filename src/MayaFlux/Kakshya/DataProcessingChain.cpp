#include "DataProcessingChain.hpp"

namespace MayaFlux::Kakshya {

void DataProcessingChain::add_processor(const std::shared_ptr<DataProcessor>& processor, const std::shared_ptr<SignalSourceContainer>& container, const std::string& tag)
{
    processor->on_attach(container);
    m_container_processors[container].emplace_back(processor);
    if (!tag.empty()) {
        m_processor_tags[processor] = tag;
    }
}

void DataProcessingChain::add_processor_at(const std::shared_ptr<DataProcessor>& processor,
    const std::shared_ptr<SignalSourceContainer>& container,
    size_t position)
{
    processor->on_attach(container);
    auto& processors = m_container_processors[container];

    if (position >= processors.size()) {
        processors.push_back(processor);
    } else {
        processors.insert(processors.begin() + position, processor);
    }
}

void DataProcessingChain::remove_processor(const std::shared_ptr<DataProcessor>& processor, const std::shared_ptr<SignalSourceContainer>& container)
{
    auto it = m_container_processors.find(container);
    if (it != m_container_processors.end()) {
        auto& processors = it->second;
        auto proc_it = std::find(processors.begin(), processors.end(), processor);

        if (proc_it != processors.end()) {
            processor->on_detach(container);
            processors.erase(proc_it);

            if (processors.empty()) {
                m_container_processors.erase(it);
            }
        }
    }
}

void DataProcessingChain::process(const std::shared_ptr<SignalSourceContainer>& container)
{
    auto it = m_container_processors.find(container);
    if (it != m_container_processors.end()) {
        for (auto& processor : it->second) {
            processor->process(container);
        }
    }
}

void DataProcessingChain::process_filtered(const std::shared_ptr<SignalSourceContainer>& container, const std::function<bool(const std::shared_ptr<DataProcessor>&)>& filter)
{
    auto it = m_container_processors.find(container);
    if (it != m_container_processors.end()) {
        for (auto& processor : it->second) {
            if (filter(processor)) {
                processor->process(container);
            }
        }
    }
}

void DataProcessingChain::process_tagged(const std::shared_ptr<SignalSourceContainer>& container, const std::string& tag)
{
    auto it = m_container_processors.find(container);
    if (it != m_container_processors.end()) {
        for (auto& processor : it->second) {
            auto tag_it = m_processor_tags.find(processor);
            if (tag_it != m_processor_tags.end() && tag_it->second == tag) {
                processor->process(container);
            }
        }
    }
}
}
