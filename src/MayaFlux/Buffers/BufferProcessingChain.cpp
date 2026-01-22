#include "BufferProcessingChain.hpp"

#include "Buffer.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Buffers {

bool BufferProcessingChain::add_processor(const std::shared_ptr<BufferProcessor>& processor, const std::shared_ptr<Buffer>& buffer, std::string* rejection_reason)
{
    if (m_is_processing.load(std::memory_order_acquire) || processor->m_active_processing.load(std::memory_order_acquire) > 0) {
        return queue_pending_processor_op(processor, buffer, true, rejection_reason);
    }

    return add_processor_direct(processor, buffer, rejection_reason);
}

bool BufferProcessingChain::add_processor_direct(const std::shared_ptr<BufferProcessor>& processor, const std::shared_ptr<Buffer>& buffer, std::string* rejection_reason)
{
    auto processor_token = processor->get_processing_token();

    switch (m_enforcement_strategy) {
    case TokenEnforcementStrategy::STRICT:
        if (processor_token != m_token_filter_mask) {
            if (rejection_reason) {
                *rejection_reason = "Processor token (" + std::to_string(static_cast<uint32_t>(processor_token)) + ") does not exactly match chain's preferred token (" + std::to_string(static_cast<uint32_t>(m_token_filter_mask)) + ") in STRICT mode";
            }
            return false;
        }
        break;

    case TokenEnforcementStrategy::FILTERED:
        if (!are_tokens_compatible(m_token_filter_mask, processor_token)) {
            if (rejection_reason) {
                *rejection_reason = "Processor token (" + std::to_string(static_cast<uint32_t>(processor_token)) + ") is not compatible with chain's preferred token (" + std::to_string(static_cast<uint32_t>(m_token_filter_mask)) + ") in FILTERED mode";
            }
            return false;
        }
        break;

    case TokenEnforcementStrategy::OVERRIDE_SKIP:
        if (!are_tokens_compatible(m_token_filter_mask, processor_token)) {
            m_conditional_processors[buffer].insert(processor);
        }
        break;

    case TokenEnforcementStrategy::OVERRIDE_REJECT:
        if (!are_tokens_compatible(m_token_filter_mask, processor_token)) {
            m_pending_removal[buffer].insert(processor);
        }
        break;

    case TokenEnforcementStrategy::IGNORE:
        break;
    }

    auto& processors = m_buffer_processors[buffer];

    auto it = std::ranges::find(processors, processor);

    if (it != processors.end()) {
        if (rejection_reason) {
            *rejection_reason = "Processor already exists in chain for this buffer";
        }
        return false;
    }

    processors.push_back(processor);
    processor->on_attach(buffer);

    return true;
}

void BufferProcessingChain::remove_processor(const std::shared_ptr<BufferProcessor>& processor, const std::shared_ptr<Buffer>& buffer)
{
    if (m_is_processing.load(std::memory_order_acquire)) {
        queue_pending_processor_op(processor, buffer, false);
        return;
    }

    remove_processor_direct(processor, buffer);
}

void BufferProcessingChain::remove_processor_direct(const std::shared_ptr<BufferProcessor>& processor, const std::shared_ptr<Buffer>& buffer)
{
    auto& processors = m_buffer_processors[buffer];
    auto it = std::ranges::find(processors, processor);

    if (it != processors.end()) {
        processor->on_detach(buffer);
        processors.erase(it);

        m_conditional_processors[buffer].erase(processor);
        m_pending_removal[buffer].erase(processor);
    }
}

void BufferProcessingChain::process_non_owning(const std::shared_ptr<Buffer>& buffer)
{
    if (m_pending_count.load(std::memory_order_relaxed) > 0) {
        process_pending_processor_operations();
    }

    auto it = m_buffer_processors.find(buffer);
    if (it == m_buffer_processors.end() || it->second.empty()) {
        return;
    }

    for (auto& processor : it->second) {
        bool should_process = true;

        if (m_enforcement_strategy == TokenEnforcementStrategy::OVERRIDE_SKIP) {
            auto processor_token = processor->get_processing_token();
            should_process = are_tokens_compatible(m_token_filter_mask, processor_token);
        }

        if (should_process) {
            processor->process_non_owning(buffer);
        }
    }

    if (m_enforcement_strategy == TokenEnforcementStrategy::OVERRIDE_REJECT) {
        cleanup_rejected_processors(buffer);
    }
}

void BufferProcessingChain::process(const std::shared_ptr<Buffer>& buffer)
{
    bool expected = false;
    if (!m_is_processing.compare_exchange_strong(expected, true,
            std::memory_order_acquire, std::memory_order_relaxed)) {
        return;
    }

    if (m_pending_count.load(std::memory_order_relaxed) > 0) {
        process_pending_processor_operations();
    }

    auto it = m_buffer_processors.find(buffer);
    if (it == m_buffer_processors.end() || it->second.empty()) {
        m_is_processing.store(false, std::memory_order_release);
        return;
    }

    for (auto& processor : it->second) {
        bool should_process = true;

        if (m_enforcement_strategy == TokenEnforcementStrategy::OVERRIDE_SKIP) {
            auto processor_token = processor->get_processing_token();
            should_process = are_tokens_compatible(m_token_filter_mask, processor_token);
        }

        if (should_process) {
            processor->process(buffer);
        }
    }

    if (m_enforcement_strategy == TokenEnforcementStrategy::OVERRIDE_REJECT) {
        cleanup_rejected_processors(buffer);
    }

    m_is_processing.store(false, std::memory_order_release);
}

void BufferProcessingChain::process_complete(const std::shared_ptr<Buffer>& buffer)
{
    preprocess(buffer);
    process(buffer);
    postprocess(buffer);
    process_final(buffer);
}

void BufferProcessingChain::process_pending_processor_operations()
{
    for (auto& m_pending_op : m_pending_ops) {
        if (m_pending_op.active.load(std::memory_order_acquire)) {
            auto& op = m_pending_op;

            if (op.is_addition) {
                add_processor_direct(op.processor, op.buffer);
            } else {
                remove_processor_direct(op.processor, op.buffer);
            }

            op.processor.reset();
            op.buffer.reset();
            op.active.store(false, std::memory_order_release);
            m_pending_count.fetch_sub(1, std::memory_order_relaxed);
        }
    }
}

void BufferProcessingChain::add_preprocessor(const std::shared_ptr<BufferProcessor>& processor, const std::shared_ptr<Buffer>& buffer)
{
    processor->on_attach(buffer);
    m_preprocessors[buffer] = processor;
}

void BufferProcessingChain::add_postprocessor(const std::shared_ptr<BufferProcessor>& processor, const std::shared_ptr<Buffer>& buffer)
{
    processor->on_attach(buffer);
    m_postprocessors[buffer] = processor;
}

void BufferProcessingChain::add_final_processor(const std::shared_ptr<BufferProcessor>& processor, const std::shared_ptr<Buffer>& buffer)
{
    processor->on_attach(buffer);
    m_final_processors[buffer] = processor;
}

bool BufferProcessingChain::has_processors(const std::shared_ptr<Buffer>& buffer) const
{
    auto it = m_buffer_processors.find(buffer);
    return it != m_buffer_processors.end() && !it->second.empty();
}

const std::vector<std::shared_ptr<BufferProcessor>>& BufferProcessingChain::get_processors(const std::shared_ptr<Buffer>& buffer) const
{
    static const std::vector<std::shared_ptr<BufferProcessor>> empty_vector;

    auto it = m_buffer_processors.find(buffer);
    if (it != m_buffer_processors.end()) {
        return it->second;
    }

    return empty_vector;
}

void BufferProcessingChain::preprocess(const std::shared_ptr<Buffer>& buffer)
{
    auto pre_it = m_preprocessors.find(buffer);
    if (pre_it != m_preprocessors.end()) {
        pre_it->second->process(buffer);
    }
}

void BufferProcessingChain::postprocess(const std::shared_ptr<Buffer>& buffer)
{
    auto post_it = m_postprocessors.find(buffer);
    if (post_it != m_postprocessors.end()) {
        post_it->second->process(buffer);
    }
}

void BufferProcessingChain::process_final(const std::shared_ptr<Buffer>& buffer)
{
    auto final_it = m_final_processors.find(buffer);
    if (final_it != m_final_processors.end()) {
        final_it->second->process(buffer);
    }
}

void BufferProcessingChain::merge_chain(const std::shared_ptr<BufferProcessingChain>& other)
{
    for (const auto& [buffer, processors] : other->get_chain()) {
        if (!m_buffer_processors.count(buffer)) {
            m_buffer_processors.try_emplace(buffer, processors);
        } else {
            auto& target_processors = m_buffer_processors[buffer];
            target_processors.reserve(target_processors.size() + processors.size());

            for (const auto& processor : processors) {
                if (std::ranges::find(target_processors, processor) == target_processors.end()) {
                    target_processors.push_back(processor);
                }
            }
        }
    }
}

void BufferProcessingChain::optimize_for_tokens(const std::shared_ptr<Buffer>& buffer)
{
    auto& processors = m_buffer_processors[buffer];
    if (processors.empty()) {
        return;
    }

    std::vector<std::shared_ptr<BufferProcessor>> compatible_processors;
    std::vector<std::shared_ptr<BufferProcessor>> incompatible_processors;

    for (auto& processor : processors) {
        auto processor_token = processor->get_processing_token();
        if (are_tokens_compatible(m_token_filter_mask, processor_token)) {
            compatible_processors.push_back(processor);
        } else {
            incompatible_processors.push_back(processor);
        }
    }

    processors.clear();

    processors.insert(processors.end(), compatible_processors.begin(), compatible_processors.end());

    if (m_enforcement_strategy != TokenEnforcementStrategy::STRICT && m_enforcement_strategy != TokenEnforcementStrategy::FILTERED) {
        processors.insert(processors.end(), incompatible_processors.begin(), incompatible_processors.end());
    }
}

void BufferProcessingChain::cleanup_rejected_processors(const std::shared_ptr<Buffer>& buffer)
{
    auto& processors = m_buffer_processors[buffer];

    processors.erase(
        std::remove_if(processors.begin(), processors.end(),
            [this](const std::shared_ptr<BufferProcessor>& processor) {
                auto processor_token = processor->get_processing_token();
                return !are_tokens_compatible(m_token_filter_mask, processor_token);
            }),
        processors.end());

    m_pending_removal[buffer].clear();
}

std::vector<TokenCompatibilityReport> BufferProcessingChain::analyze_token_compatibility() const
{
    std::vector<TokenCompatibilityReport> reports;

    for (const auto& [buffer, processors] : m_buffer_processors) {
        TokenCompatibilityReport report;
        report.buffer = buffer;
        report.chain_preferred_token = m_token_filter_mask;
        report.enforcement_strategy = m_enforcement_strategy;

        for (const auto& processor : processors) {
            ProcessorTokenInfo info;
            info.processor = processor;
            info.processor_token = processor->get_processing_token();
            info.is_compatible = are_tokens_compatible(m_token_filter_mask, info.processor_token);
            info.will_be_skipped = (m_enforcement_strategy == TokenEnforcementStrategy::OVERRIDE_SKIP && !info.is_compatible);
            info.pending_removal = (m_enforcement_strategy == TokenEnforcementStrategy::OVERRIDE_REJECT && !info.is_compatible);

            report.processor_infos.push_back(info);
        }

        reports.push_back(report);
    }

    return reports;
}

bool BufferProcessingChain::validate_all_processors(std::vector<std::string>* incompatibility_reasons) const
{
    bool all_compatible = true;

    for (const auto& [buffer, processors] : m_buffer_processors) {
        for (const auto& processor : processors) {
            auto processor_token = processor->get_processing_token();
            if (!are_tokens_compatible(m_token_filter_mask, processor_token)) {
                all_compatible = false;
                if (incompatibility_reasons) {
                    incompatibility_reasons->push_back(
                        "Processor with token " + std::to_string(static_cast<uint32_t>(processor_token)) + " incompatible with chain preferred token " + std::to_string(static_cast<uint32_t>(m_token_filter_mask)));
                }
            }
        }
    }

    return all_compatible;
}

void BufferProcessingChain::enforce_chain_token_on_processors()
{
    for (const auto& [buffer, processors] : m_buffer_processors) {
        for (auto& processor : processors) {
            auto processor_token = processor->get_processing_token();
            if (processor_token != m_token_filter_mask && are_tokens_compatible(m_token_filter_mask, processor_token)) {

                try {
                    processor->set_processing_token(m_token_filter_mask);
                } catch (const std::exception& e) {
                    error_rethrow(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                        std::source_location::current(),
                        "Failed to enforce chain token on processor: " + std::string(e.what()));
                }
            }
        }
    }
}

bool BufferProcessingChain::queue_pending_processor_op(const std::shared_ptr<BufferProcessor>& processor, const std::shared_ptr<Buffer>& buffer, bool is_addition, std::string* rejection_reason)
{
    for (auto& m_pending_op : m_pending_ops) {
        bool expected = false;
        if (m_pending_op.active.compare_exchange_strong(
                expected, true,
                std::memory_order_acquire,
                std::memory_order_relaxed)) {

            m_pending_op.processor = processor;
            m_pending_op.buffer = buffer;
            m_pending_op.is_addition = is_addition;
            m_pending_count.fetch_add(1, std::memory_order_relaxed);
            return true;
        }
    }

    if (rejection_reason && is_addition) {
        *rejection_reason = "Processor operation queue full";
    }
    return false;
}

}
