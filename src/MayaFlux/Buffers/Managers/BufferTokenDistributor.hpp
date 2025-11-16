#pragma once

#include "MayaFlux/Buffers/Buffer.hpp"
#include "MayaFlux/Core/ProcessingTokens.hpp"

namespace MayaFlux::Buffers {

class AudioBuffer;
class VKBuffer;
class TransferProcessor;

/**
 * @enum DistributionResult
 * @brief Outcome of token distribution decision
 */
enum class DistributionResult : uint8_t {
    DIRECT_ROOT, // Buffer goes directly to root (normal audio)
    TRANSFER_TO_ROOT, // Buffer transfers to root (GPU→Audio)
    TRANSFER_ONLY, // Buffer transfers to another domain (Audio→GPU, no root)
    INTERNAL_ONLY, // Buffer marked internal, no root aggregation (AUDIO_PARALLEL)
    REJECTED // Invalid combination, error
};

/**
 * @struct DistributionDecision
 * @brief Routing decision for a buffer with a given token
 */
struct DistributionDecision {
    DistributionResult result;

    // If TRANSFER_ONLY or TRANSFER_TO_ROOT:
    std::shared_ptr<TransferProcessor> transfer_processor;
    ProcessingToken transfer_direction; // Which buffer gets the processor

    // Diagnostic
    std::string reason; // Error message if REJECTED
};

/**
 * @class BufferTokenDistributor
 * @brief Determines routing for buffers based on type + token combination
 *
 * Decision tree:
 * 1) DIRECT_ROOT - Buffer goes to normal root aggregation
 * 2) TRANSFER_ONLY - Create transfer processor, attach to target, no root
 * 3) INTERNAL_ONLY - Mark internal, no root aggregation
 * 4) REJECTED - Invalid combination
 */
class BufferTokenDistributor {
public:
    /**
     * @brief Distribute a buffer based on its type and requested token
     * @param buffer The buffer to distribute
     * @param requested_token The token being requested
     * @return Distribution decision with routing instructions
     */
    static DistributionDecision distribute(
        const std::shared_ptr<Buffer>& buffer,
        ProcessingToken requested_token);

    /**
     * @brief Distribute with optional transfer target (for cross-domain routing)
     * @param buffer Source buffer
     * @param requested_token Token for the source buffer
     * @param transfer_target Optional target for cross-domain transfer
     * @param transfer_target_token Token for the target (if transferring)
     * @return Distribution decision with transfer processor if applicable
     */
    static DistributionDecision distribute_with_transfer(
        const std::shared_ptr<Buffer>& buffer,
        ProcessingToken requested_token,
        const std::shared_ptr<Buffer>& transfer_target,
        ProcessingToken transfer_target_token);

private:
    // Helper: Validate buffer type matches token
    static bool is_valid_audio_token(ProcessingToken token);
    static bool is_valid_vk_token(ProcessingToken token);

    // Helper: Extract token components
    static bool has_sample_rate(ProcessingToken token);
    static bool has_frame_rate(ProcessingToken token);
    static bool has_cpu(ProcessingToken token);
    static bool has_gpu(ProcessingToken token);

    // Decision tree functions
    static DistributionDecision decide_audio_buffer(
        const std::shared_ptr<AudioBuffer>& audio,
        ProcessingToken token);

    static DistributionDecision decide_vk_buffer(
        const std::shared_ptr<VKBuffer>& vk,
        ProcessingToken token);

    static DistributionDecision decide_transfer(
        const std::shared_ptr<Buffer>& source,
        ProcessingToken src_token,
        const std::shared_ptr<Buffer>& target,
        ProcessingToken tgt_token);

    static bool can_transfer(
        const std::shared_ptr<Buffer>& source,
        ProcessingToken src_token,
        const std::shared_ptr<Buffer>& target,
        ProcessingToken tgt_token);
};

} // namespace MayaFlux::Buffers
