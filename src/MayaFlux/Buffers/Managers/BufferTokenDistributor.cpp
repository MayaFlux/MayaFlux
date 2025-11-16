#include "BufferTokenDistributor.hpp"

#include "MayaFlux/Buffers/AudioBuffer.hpp"
#include "MayaFlux/Buffers/Staging/TransferProcessor.hpp"
#include "MayaFlux/Buffers/VKBuffer.hpp"
// #include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Buffers {

// ============================================================================
// Public API
// ============================================================================

DistributionDecision BufferTokenDistributor::distribute(
    const std::shared_ptr<Buffer>& buffer,
    ProcessingToken requested_token)
{
    if (!buffer) {
        return DistributionDecision {
            .result = DistributionResult::REJECTED,
            .transfer_processor = nullptr,
            .transfer_direction = ProcessingToken::AUDIO_BACKEND,
            .reason = "Null buffer provided"
        };
    }

    if (auto audio = std::dynamic_pointer_cast<AudioBuffer>(buffer)) {
        return decide_audio_buffer(audio, requested_token);
    }

    if (auto vk = std::dynamic_pointer_cast<VKBuffer>(buffer)) {
        return decide_vk_buffer(vk, requested_token);
    }

    return DistributionDecision {
        .result = DistributionResult::REJECTED,
        .transfer_processor = nullptr,
        .transfer_direction = ProcessingToken::AUDIO_BACKEND,
        .reason = "Unknown buffer type"
    };
}

DistributionDecision BufferTokenDistributor::distribute_with_transfer(
    const std::shared_ptr<Buffer>& buffer,
    ProcessingToken requested_token,
    const std::shared_ptr<Buffer>& transfer_target,
    ProcessingToken transfer_target_token)
{
    auto source_decision = distribute(buffer, requested_token);
    if (source_decision.result == DistributionResult::REJECTED) {
        return source_decision;
    }

    return decide_transfer(buffer, requested_token, transfer_target, transfer_target_token);
}

// ============================================================================
// Helpers: Token Parsing
// ============================================================================

bool BufferTokenDistributor::has_sample_rate(ProcessingToken token)
{
    return !(token & ProcessingToken::FRAME_RATE);
}

bool BufferTokenDistributor::has_frame_rate(ProcessingToken token)
{
    return token & ProcessingToken::FRAME_RATE;
}

bool BufferTokenDistributor::has_cpu(ProcessingToken token)
{
    return token & ProcessingToken::CPU_PROCESS;
}

bool BufferTokenDistributor::has_gpu(ProcessingToken token)
{
    return token & ProcessingToken::GPU_PPOCESS;
}

// ============================================================================
// Validation: Token Validity
// ============================================================================

bool BufferTokenDistributor::is_valid_audio_token(ProcessingToken token)
{
    if (has_frame_rate(token)) {
        return false;
    }

    return has_cpu(token) || has_gpu(token);
}

bool BufferTokenDistributor::is_valid_vk_token(ProcessingToken token)
{
    return has_gpu(token) && !has_cpu(token);
}

// ============================================================================
// Decision Trees
// ============================================================================

DistributionDecision BufferTokenDistributor::decide_audio_buffer(
    const std::shared_ptr<AudioBuffer>& audio,
    ProcessingToken token)
{
    if (!is_valid_audio_token(token)) {
        return DistributionDecision {
            .result = DistributionResult::REJECTED,
            .transfer_processor = nullptr,
            .transfer_direction = ProcessingToken::AUDIO_BACKEND,
            .reason = "AudioBuffer requires SAMPLE_RATE token with CPU or GPU device"
        };
    }

    // Decision 1: AUDIO_PARALLEL (GPU audio) → INTERNAL_ONLY
    if (has_sample_rate(token) && has_gpu(token)) {
        audio->mark_internal_only(true);
        return DistributionDecision {
            .result = DistributionResult::INTERNAL_ONLY,
            .transfer_processor = nullptr,
            .transfer_direction = ProcessingToken::AUDIO_BACKEND,
            .reason = "Audio buffer with GPU processing marked internal"
        };
    }

    // Decision 2: Normal audio (SAMPLE_RATE + CPU) → DIRECT_ROOT
    if (has_sample_rate(token) && has_cpu(token)) {
        audio->mark_internal_only(false);
        return DistributionDecision {
            .result = DistributionResult::DIRECT_ROOT,
            .transfer_processor = nullptr,
            .transfer_direction = ProcessingToken::AUDIO_BACKEND,
            .reason = "Audio buffer registered to root"
        };
    }

    return DistributionDecision {
        .result = DistributionResult::REJECTED,
        .transfer_processor = nullptr,
        .transfer_direction = ProcessingToken::AUDIO_BACKEND,
        .reason = "AudioBuffer token combination not handled"
    };
}

DistributionDecision BufferTokenDistributor::decide_vk_buffer(
    const std::shared_ptr<VKBuffer>& vk,
    ProcessingToken token)
{
    if (!is_valid_vk_token(token)) {
        return DistributionDecision {
            .result = DistributionResult::REJECTED,
            .transfer_processor = nullptr,
            .transfer_direction = ProcessingToken::AUDIO_BACKEND,
            .reason = "VKBuffer requires GPU_PPOCESS token without CPU_PROCESS"
        };
    }

    // Decision 1: GRAPHICS_BACKEND or FRAME_RATE GPU → DIRECT_ROOT (graphics domain)
    if (has_frame_rate(token) && has_gpu(token)) {
        return DistributionDecision {
            .result = DistributionResult::DIRECT_ROOT,
            .transfer_processor = nullptr,
            .transfer_direction = ProcessingToken::GRAPHICS_BACKEND,
            .reason = "GPU buffer registered to graphics root"
        };
    }

    // Decision 2: AUDIO_PARALLEL (SAMPLE_RATE + GPU) → TRANSFER_ONLY
    if (has_sample_rate(token) && has_gpu(token)) {
        vk->mark_internal_only(true);
        return DistributionDecision {
            .result = DistributionResult::TRANSFER_ONLY,
            .transfer_processor = nullptr,
            .transfer_direction = ProcessingToken::AUDIO_PARALLEL,
            .reason = "GPU buffer at audio rate marked internal (awaiting transfer)"
        };
    }

    return DistributionDecision {
        .result = DistributionResult::REJECTED,
        .transfer_processor = nullptr,
        .transfer_direction = ProcessingToken::AUDIO_BACKEND,
        .reason = "VKBuffer token combination not handled"
    };
}

// ============================================================================
// Transfer Decision
// ============================================================================

bool BufferTokenDistributor::can_transfer(
    const std::shared_ptr<Buffer>& source,
    ProcessingToken src_token,
    const std::shared_ptr<Buffer>& target,
    ProcessingToken tgt_token)
{
    if (source == target && src_token == tgt_token) {
        return false;
    }

    auto audio_src = std::dynamic_pointer_cast<AudioBuffer>(source);
    auto audio_tgt = std::dynamic_pointer_cast<AudioBuffer>(target);
    auto vk_src = std::dynamic_pointer_cast<VKBuffer>(source);
    auto vk_tgt = std::dynamic_pointer_cast<VKBuffer>(target);

    // Audio (SAMPLE_RATE, CPU) → VK (SAMPLE_RATE or FRAME_RATE, GPU)
    if (audio_src && vk_tgt) {
        if (has_sample_rate(src_token) && has_cpu(src_token)) {
            if (has_gpu(tgt_token)) {
                return true;
            }
        }
    }

    // VK (any rate, GPU) → Audio (SAMPLE_RATE, CPU)
    if (vk_src && audio_tgt) {
        if (has_gpu(src_token)) {
            if (has_sample_rate(tgt_token) && has_cpu(tgt_token)) {
                return true;
            }
        }
    }

    // Audio (SAMPLE_RATE, GPU) → Audio (SAMPLE_RATE, CPU)
    if (audio_src && audio_tgt) {
        if (has_sample_rate(src_token) && has_gpu(src_token)) {
            if (has_sample_rate(tgt_token) && has_cpu(tgt_token)) {
                return true;
            }
        }
    }

    // VK → VK (GPU to GPU, rate-dependent)
    if (vk_src && vk_tgt) {
        if (has_gpu(src_token) && has_gpu(tgt_token)) {
            return true;
        }
    }

    return false;
}

DistributionDecision BufferTokenDistributor::decide_transfer(
    const std::shared_ptr<Buffer>& source,
    ProcessingToken src_token,
    const std::shared_ptr<Buffer>& target,
    ProcessingToken tgt_token)
{
    auto audio_src = std::dynamic_pointer_cast<AudioBuffer>(source);
    auto audio_tgt = std::dynamic_pointer_cast<AudioBuffer>(target);
    auto vk_src = std::dynamic_pointer_cast<VKBuffer>(source);
    auto vk_tgt = std::dynamic_pointer_cast<VKBuffer>(target);

    // Pattern: Audio(SAMPLE_RATE|CPU) → VK(GPU) → Audio(SAMPLE_RATE|CPU)
    if (audio_src && audio_tgt && has_sample_rate(src_token) && has_gpu(src_token) && has_sample_rate(tgt_token) && has_cpu(tgt_token)) {
        // This would be Audio→GPU→Audio, which is forbidden
        // But the distribute() call on each would catch this
        // Just prevent if someone tries to route them together
    }

    if (!can_transfer(source, src_token, target, tgt_token)) {
        return DistributionDecision {
            .result = DistributionResult::REJECTED,
            .transfer_processor = nullptr,
            .transfer_direction = ProcessingToken::AUDIO_BACKEND,
            .reason = "Transfer not supported between these buffer types and tokens"
        };
    }

    std::shared_ptr<TransferProcessor> processor;
    TransferDirection direction = TransferDirection::AUDIO_TO_GPU;

    if (audio_src && vk_tgt) {
        direction = TransferDirection::AUDIO_TO_GPU;
        processor = std::make_shared<TransferProcessor>(audio_src, vk_tgt, direction);
    } else if (vk_src && audio_tgt) {
        direction = TransferDirection::GPU_TO_AUDIO;
        processor = std::make_shared<TransferProcessor>(audio_tgt, vk_src, direction);
    } else if (audio_src && audio_tgt) {
        direction = TransferDirection::AUDIO_TO_GPU; // Reuse as "copy" semantics
        processor = std::make_shared<TransferProcessor>();
        processor->connect_audio_to_gpu(audio_src, nullptr); // Placeholder, will be copy
    } else if (vk_src && vk_tgt) {
        direction = TransferDirection::AUDIO_TO_GPU; // Reuse enum, semantically "copy"
        processor = std::make_shared<TransferProcessor>();
    }

    if (!processor) {
        return DistributionDecision {
            .result = DistributionResult::REJECTED,
            .transfer_processor = nullptr,
            .transfer_direction = ProcessingToken::AUDIO_BACKEND,
            .reason = "Failed to create transfer processor"
        };
    }

    if (audio_tgt) {
        audio_tgt->mark_internal_only(false);
    }

    return DistributionDecision {
        .result = DistributionResult::TRANSFER_ONLY,
        .transfer_processor = processor,
        .transfer_direction = tgt_token,
        .reason = "Transfer processor created"
    };
}

} // namespace MayaFlux::Buffers
