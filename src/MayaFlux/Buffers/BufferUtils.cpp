#include "BufferUtils.hpp"

#include "MayaFlux/Nodes/Node.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Buffers {

bool are_tokens_compatible(ProcessingToken preferred, ProcessingToken current)
{
    bool preferred_sample = preferred & SAMPLE_RATE;
    bool preferred_frame = preferred & FRAME_RATE;
    bool current_sample = current & SAMPLE_RATE;
    bool current_frame = current & FRAME_RATE;

    // If preferred is FRAME_RATE, only FRAME_RATE is compatible
    if (preferred_frame && !current_frame)
        return false;
    // If preferred is SAMPLE_RATE, FRAME_RATE can be compatible (sample can "delay" to frame)
    if (preferred_sample && current_frame)
        return true;
    // If both are SAMPLE_RATE or both are FRAME_RATE, compatible
    if ((preferred_sample && current_sample) || (preferred_frame && current_frame))
        return true;

    // Device compatibility: SAMPLE_RATE can't run on GPU, but FRAME_RATE can run on CPU
    bool preferred_cpu = preferred & CPU_PROCESS;
    bool preferred_gpu = preferred & GPU_PPOCESS;
    bool current_cpu = current & CPU_PROCESS;
    bool current_gpu = current & GPU_PPOCESS;

    if (preferred_sample && current_gpu)
        return false; // Can't run sample rate on GPU
    if (preferred_gpu && current_cpu)
        return false; // If preferred is GPU, but current is CPU, not compatible
    // If preferred is CPU, but current is GPU, allow only for FRAME_RATE
    if (preferred_cpu && current_gpu && !current_frame)
        return false;

    // Sequential/Parallel compatibility: allow if rates align
    bool preferred_seq = preferred & SEQUENTIAL;
    bool preferred_par = preferred & PARALLEL;
    bool current_seq = current & SEQUENTIAL;
    bool current_par = current & PARALLEL;

    if ((preferred_seq && current_par) || (preferred_par && current_seq)) {
        // Allow if rates align (already checked above)
        if ((preferred_sample && current_sample) || (preferred_frame && current_frame)) {
            return true;
        }
        // If preferred is SAMPLE_RATE and current is FRAME_RATE, already handled above
        // Otherwise, not compatible
        return false;
    }

    // If all checks pass, compatible
    return true;
}

void validate_token(ProcessingToken token)
{
    if ((token & SAMPLE_RATE) && (token & FRAME_RATE)) {
        error<std::invalid_argument>(
            Journal::Component::Buffers,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "SAMPLE_RATE and FRAME_RATE are mutually exclusive.");
    }

    if ((token & CPU_PROCESS) && (token & GPU_PPOCESS)) {
        error<std::invalid_argument>(Journal::Component::Buffers,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "CPU_PROCESS and GPU_PROCESS are mutually exclusive.");
    }

    if ((token & SEQUENTIAL) && (token & PARALLEL)) {
        error<std::invalid_argument>(Journal::Component::Buffers,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "SEQUENTIAL and PARALLEL are mutually exclusive.");
    }
}

ProcessingToken get_optimal_token(const std::string& buffer_type, uint32_t system_capabilities)
{
    if (buffer_type == "audio") {
        return (system_capabilities & 0x1) ? AUDIO_PARALLEL : AUDIO_BACKEND;
    }

    if (buffer_type == "video" || buffer_type == "texture") {
        return GRAPHICS_BACKEND;
    }
    return AUDIO_BACKEND;
}

bool wait_for_snapshot_completion(
    const std::shared_ptr<Nodes::Node>& node,
    uint64_t active_context_id,
    int max_spins)
{
    int spin_count = 0;

    while (node->is_in_snapshot_context(active_context_id) && spin_count < max_spins) {
        if (spin_count < 10) {
            for (int i = 0; i < (1 << spin_count); ++i) {
                MF_PAUSE_INSTRUCTION();
            }
        } else {
            std::this_thread::yield();
        }
        ++spin_count;
    }

    if (spin_count >= max_spins) {
        MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Timeout waiting for node snapshot to complete. "
            "Possible deadlock or very long processing time.");
        return false;
    }

    return true;
}

double extract_single_sample(const std::shared_ptr<Nodes::Node>& node)
{
    if (!node) {
        MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "extract_single_sample: null node");
        return 0.0;
    }

    static std::atomic<uint64_t> s_context_counter { 1 };
    uint64_t my_context_id = s_context_counter.fetch_add(1, std::memory_order_relaxed);

    const auto state = node->m_state.load(std::memory_order_acquire);

    if (state == Utils::NodeState::INACTIVE && !node->is_buffer_processed()) {
        double value = node->process_sample(0.F);
        node->mark_buffer_processed();
        return value;
    }

    bool claimed = node->try_claim_snapshot_context(my_context_id);

    if (claimed) {
        try {
            node->save_state();
            double value = node->process_sample(0.F);
            node->restore_state();

            if (node->is_buffer_processed()) {
                node->request_buffer_reset();
            }

            node->release_snapshot_context(my_context_id);
            return value;

        } catch (const std::exception& e) {
            node->release_snapshot_context(my_context_id);
            MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "Error processing node: {}", e.what());
            return 0.0;
        }
    } else {
        uint64_t active_context = node->get_active_snapshot_context();

        if (!wait_for_snapshot_completion(node, active_context)) {
            return 0.0;
        }

        node->save_state();
        double value = node->process_sample(0.F);
        node->restore_state();

        if (node->is_buffer_processed()) {
            node->request_buffer_reset();
        }

        return value;
    }
}

std::vector<double> extract_multiple_samples(
    const std::shared_ptr<Nodes::Node>& node,
    size_t num_samples)
{
    std::vector<double> output(num_samples);

    if (!node) {
        MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "extract_multiple_samples: null node");
        return output;
    }

    static std::atomic<uint64_t> s_context_counter { 1 };
    uint64_t my_context_id = s_context_counter.fetch_add(1, std::memory_order_relaxed);

    const auto state = node->m_state.load(std::memory_order_acquire);

    // Fast path: inactive node
    if (state == Utils::NodeState::INACTIVE && !node->is_buffer_processed()) {
        for (size_t i = 0; i < num_samples; i++) {
            output[i] = node->process_sample(0.F);
        }
        node->mark_buffer_processed();
        return output;
    }

    // Try to claim snapshot
    bool claimed = node->try_claim_snapshot_context(my_context_id);

    if (claimed) {
        try {
            node->save_state();

            for (size_t i = 0; i < num_samples; i++) {
                output[i] = node->process_sample(0.F);
            }

            node->restore_state();

            if (node->is_buffer_processed()) {
                node->request_buffer_reset();
            }

            node->release_snapshot_context(my_context_id);

        } catch (const std::exception& e) {
            node->release_snapshot_context(my_context_id);
            MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "Error processing node: {}", e.what());
            output.clear();
        }
    } else {
        uint64_t active_context = node->get_active_snapshot_context();

        if (!wait_for_snapshot_completion(node, active_context)) {
            output.clear();
            return output;
        }

        node->save_state();
        for (size_t i = 0; i < num_samples; i++) {
            output[i] = node->process_sample(0.F);
        }
        node->restore_state();

        if (node->is_buffer_processed()) {
            node->request_buffer_reset();
        }
    }

    return output;
}

void update_buffer_with_node_data(
    const std::shared_ptr<Nodes::Node>& node,
    std::span<double> buffer,
    double mix)
{
    if (!node) {
        MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "apply_to_buffer: null node");
        return;
    }

    static std::atomic<uint64_t> s_context_counter { 1 };
    uint64_t my_context_id = s_context_counter.fetch_add(1, std::memory_order_relaxed);

    const auto state = node->m_state.load(std::memory_order_acquire);

    // Fast path: inactive node
    if (state == Utils::NodeState::INACTIVE && !node->is_buffer_processed()) {
        for (double& sample : buffer) {
            sample += node->process_sample(0.F) * mix;
        }
        node->mark_buffer_processed();
        return;
    }

    // Try to claim snapshot
    bool claimed = node->try_claim_snapshot_context(my_context_id);

    if (claimed) {
        try {
            node->save_state();

            for (double& sample : buffer) {
                sample += node->process_sample(0.F) * mix;
            }

            node->restore_state();

            if (node->is_buffer_processed()) {
                node->request_buffer_reset();
            }

            node->release_snapshot_context(my_context_id);

        } catch (const std::exception& e) {
            node->release_snapshot_context(my_context_id);
            MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "Error processing node: {}", e.what());
        }
    } else {
        uint64_t active_context = node->get_active_snapshot_context();

        if (!wait_for_snapshot_completion(node, active_context)) {
            return;
        }

        node->save_state();
        for (double& sample : buffer) {
            sample += node->process_sample(0.F) * mix;
        }
        node->restore_state();

        if (node->is_buffer_processed()) {
            node->request_buffer_reset();
        }
    }
}

} // namespace MayaFlux::Buffers
