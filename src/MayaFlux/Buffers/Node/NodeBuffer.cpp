#include "NodeBuffer.hpp"

#include "MayaFlux/Nodes/Node.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Buffers {

constexpr int MAX_SPINS = 1000;

NodeSourceProcessor::NodeSourceProcessor(std::shared_ptr<Nodes::Node> node, float mix, bool clear_before_process)
    : m_node(std::move(node))
    , m_mix(mix)
    , m_clear_before_process(clear_before_process)
{
}

void NodeSourceProcessor::on_attach(std::shared_ptr<Buffer> /*buffer*/)
{
    if (m_node) {
        m_node->add_buffer_reference();
    }
}

void NodeSourceProcessor::on_detach(std::shared_ptr<Buffer> /*buffer*/)
{
    if (m_node) {
        m_node->remove_buffer_reference();
    }
}

void NodeSourceProcessor::processing_function(std::shared_ptr<Buffer> buffer)
{
    if (!m_node) {
        MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing, "NodeSourceProcessor has null node. Skipping processing.");
        return;
    }

    try {
        auto& buffer_data = std::dynamic_pointer_cast<AudioBuffer>(buffer)->get_data();

        bool should_clear = m_clear_before_process;
        if (auto node_buffer = std::dynamic_pointer_cast<NodeBuffer>(buffer)) {
            should_clear = node_buffer->get_clear_before_process();
        }

        if (should_clear) {
            buffer->clear();
        }

        update_buffer(buffer_data);

    } catch (const std::exception& e) {
        error_rethrow(Journal::Component::Buffers, Journal::Context::BufferProcessing, std::source_location::current(), "Error processing node: {}", e.what());
    }
}

std::vector<double> NodeSourceProcessor::get_node_data(uint32_t num_samples)
{
    std::vector<double> output(num_samples);

    static std::atomic<uint64_t> s_context_counter { 1 };
    uint64_t my_context_id = s_context_counter.fetch_add(1, std::memory_order_relaxed);

    const auto& state = m_node->m_state.load();
    if (state == Utils::NodeState::INACTIVE && !m_node->is_buffer_processed()) {
        for (size_t i = 0; i < num_samples; i++) {
            output[i] = m_node->process_sample(0.F);
        }
        m_node->mark_buffer_processed();
        return output;
    }

    bool claimed_snapshot = m_node->try_claim_snapshot_context(my_context_id);

    if (claimed_snapshot) {

        try {
            m_node->save_state();

            for (size_t i = 0; i < num_samples; i++) {
                output[i] = m_node->process_sample(0.F);
            }

            m_node->restore_state();
        } catch (const std::exception& e) {
            m_node->release_snapshot_context(my_context_id);
            error_rethrow(Journal::Component::Buffers, Journal::Context::BufferProcessing, std::source_location::current(), "Error processing node: {}", e.what());
        }

        if (m_node->is_buffer_processed()) {
            m_node->request_buffer_reset();
        }
        m_node->release_snapshot_context(my_context_id);

    } else {
        uint64_t active_context = m_node->get_active_snapshot_context();

        int spin_count = 0;

        while (m_node->is_in_snapshot_context(active_context) && spin_count < MAX_SPINS) {
            if (spin_count < 10) {
                for (int i = 0; i < (1 << spin_count); ++i) {
                    MF_PAUSE_INSTRUCTION();
                }
            } else {
                std::this_thread::yield();
            }
            ++spin_count;
        }

        if (spin_count >= MAX_SPINS) {
            MF_RT_ERROR(Journal::Component::Buffers,
                Journal::Context::BufferProcessing,
                "Timeout waiting for node snapshot to complete. "
                "Possible deadlock or very long processing time.");
            return output;
        }

        m_node->save_state();
        for (size_t i = 0; i < num_samples; i++) {
            output[i] = m_node->process_sample(0.F);
        }
        m_node->restore_state();

        if (m_node->is_buffer_processed()) {
            m_node->request_buffer_reset();
        }
    }

    return output;
}

void NodeSourceProcessor::update_buffer(std::vector<double>& buffer_data)
{
    static std::atomic<uint64_t> s_context_counter { 1 };
    uint64_t my_context_id = s_context_counter.fetch_add(1, std::memory_order_relaxed);
    const auto& state = m_node->m_state.load();

    if (state == Utils::NodeState::INACTIVE && !m_node->is_buffer_processed()) {
        for (double& i : buffer_data) {
            i += m_node->process_sample(0.F) * m_mix;
        }
        m_node->mark_buffer_processed();
        return;
    }

    bool claimed_snapshot = m_node->try_claim_snapshot_context(my_context_id);

    if (claimed_snapshot) {
        try {
            m_node->save_state();

            for (double& sample : buffer_data) {
                sample += m_node->process_sample(0.F) * m_mix;
            }

            m_node->restore_state();

        } catch (const std::exception& e) {
            m_node->release_snapshot_context(my_context_id);
            error_rethrow(Journal::Component::Buffers, Journal::Context::BufferProcessing, std::source_location::current(), "Error processing node: {}", e.what());
        }

        if (m_node->is_buffer_processed()) {
            m_node->request_buffer_reset();
        }

        m_node->release_snapshot_context(my_context_id);

    } else {
        uint64_t active_context = m_node->get_active_snapshot_context();

        int spin_count = 0;

        while (m_node->is_in_snapshot_context(active_context) && spin_count < MAX_SPINS) {
            if (spin_count < 10) {
                for (int i = 0; i < (1 << spin_count); ++i) {
                    MF_PAUSE_INSTRUCTION();
                }
            } else {
                std::this_thread::yield();
            }
            ++spin_count;
        }

        if (spin_count >= MAX_SPINS) {
            MF_RT_ERROR(Journal::Component::Buffers,
                Journal::Context::BufferProcessing,
                "Timeout waiting for node snapshot to complete");
            return;
        }

        m_node->save_state();
        for (double& sample : buffer_data) {
            sample += m_node->process_sample(0.F) * m_mix;
        }
        m_node->restore_state();

        if (m_node->is_buffer_processed()) {
            m_node->request_buffer_reset();
        }
    }
}

NodeBuffer::NodeBuffer(uint32_t channel_id, uint32_t num_samples, std::shared_ptr<Nodes::Node> source, bool clear_before_process)
    : AudioBuffer(channel_id, num_samples)
    , m_source_node(std::move(source))
    , m_clear_before_process(clear_before_process)
{
}

void NodeBuffer::setup_processors(ProcessingToken /*token*/)
{
    m_default_processor = create_default_processor();
    m_default_processor->on_attach(shared_from_this());
}

void NodeBuffer::process_default()
{
    if (m_clear_before_process) {
        clear();
    }
    m_default_processor->process(shared_from_this());
}

std::shared_ptr<BufferProcessor> NodeBuffer::create_default_processor()
{
    return std::make_shared<NodeSourceProcessor>(m_source_node);
}

}
