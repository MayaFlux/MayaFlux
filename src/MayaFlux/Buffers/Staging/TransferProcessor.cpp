#include "TransferProcessor.hpp"

#include "MayaFlux/Buffers/AudioBuffer.hpp"
#include "MayaFlux/Buffers/VKBuffer.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "StagingUtils.hpp"

namespace MayaFlux::Buffers {

TransferProcessor::TransferProcessor()
{
    m_processing_token = ProcessingToken::GRAPHICS_BACKEND;
}

TransferProcessor::TransferProcessor(
    const std::shared_ptr<AudioBuffer>& source,
    const std::shared_ptr<VKBuffer>& target)
    : TransferProcessor()
{
    connect_audio_to_gpu(source, target);
    m_direction = TransferDirection::AUDIO_TO_GPU;
}

TransferProcessor::TransferProcessor(
    const std::shared_ptr<AudioBuffer>& audio_buffer,
    const std::shared_ptr<VKBuffer>& gpu_buffer,
    TransferDirection direction)
    : TransferProcessor()
{
    m_direction = direction;

    if (direction == TransferDirection::AUDIO_TO_GPU) {
        connect_audio_to_gpu(audio_buffer, gpu_buffer);
    } else if (direction == TransferDirection::GPU_TO_AUDIO) {
        connect_gpu_to_audio(gpu_buffer, audio_buffer);
    }
}

void TransferProcessor::connect_audio_to_gpu(
    const std::shared_ptr<AudioBuffer>& source,
    const std::shared_ptr<VKBuffer>& target)
{
    if (!source || !target) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "TransferProcessor::connect_audio_to_gpu: null buffer provided");
        return;
    }

    m_audio_to_gpu_map[source] = target;
    m_direction = TransferDirection::AUDIO_TO_GPU;
}

void TransferProcessor::connect_gpu_to_audio(
    const std::shared_ptr<VKBuffer>& source,
    const std::shared_ptr<AudioBuffer>& target)
{
    if (!source || !target) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "TransferProcessor::connect_gpu_to_audio: null buffer provided");
        return;
    }

    m_gpu_to_audio_map[source] = target;
    m_direction = TransferDirection::GPU_TO_AUDIO;
}

void TransferProcessor::setup_staging(
    const std::shared_ptr<VKBuffer>& target,
    std::shared_ptr<VKBuffer> staging_buffer)
{
    if (!target) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "TransferProcessor::setup_staging: null target provided");
        return;
    }

    m_staging_map[target] = std::move(staging_buffer);
}

bool TransferProcessor::validate_audio_to_gpu(const std::shared_ptr<VKBuffer>& target) const
{
    return std::ranges::any_of(m_audio_to_gpu_map,
        [&target](const auto& pair) { return pair.second == target; });
}

bool TransferProcessor::validate_gpu_to_audio(const std::shared_ptr<AudioBuffer>& target) const
{
    return std::ranges::any_of(m_gpu_to_audio_map,
        [&target](const auto& pair) { return pair.second == target; });
}

void TransferProcessor::on_attach(const std::shared_ptr<Buffer>& buffer)
{
    if (m_direction == TransferDirection::AUDIO_TO_GPU) {
        if (auto vk_buffer = std::dynamic_pointer_cast<VKBuffer>(buffer)) {
            if (!validate_audio_to_gpu(vk_buffer)) {
                MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                    "TransferProcessor not configured for the attached VKBuffer instance (audio→gpu).");
                return;
            }

            if (!vk_buffer->is_initialized()) {
                MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                    "VKBuffer not initialized - register with BufferManager first.");
                return;
            }

            if (!vk_buffer->is_host_visible()) {
                auto it = m_staging_map.find(vk_buffer);
                if (it == m_staging_map.end()) {
                    MF_WARN(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                        "No staging buffer configured for device-local VKBuffer. Create one for efficient transfers.");
                }
            }
        } else {
            error<std::invalid_argument>(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                std::source_location::current(),
                "TransferProcessor (audio→gpu) requires VKBuffer attachment");
        }

    } else if (m_direction == TransferDirection::GPU_TO_AUDIO) {
        if (auto audio_buffer = std::dynamic_pointer_cast<AudioBuffer>(buffer)) {
            if (!validate_gpu_to_audio(audio_buffer)) {
                MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                    "TransferProcessor not configured for the attached AudioBuffer instance (gpu→audio).");
                return;
            }
        } else {
            error<std::invalid_argument>(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                std::source_location::current(),
                "TransferProcessor (gpu→audio) requires AudioBuffer attachment");
        }
    }
}

void TransferProcessor::on_detach(const std::shared_ptr<Buffer>& buffer)
{
    if (m_direction == TransferDirection::AUDIO_TO_GPU) {
        if (auto vk_buffer = std::dynamic_pointer_cast<VKBuffer>(buffer)) {
            m_staging_map.erase(vk_buffer);
            for (auto it = m_audio_to_gpu_map.begin(); it != m_audio_to_gpu_map.end();) {
                if (it->second == vk_buffer) {
                    it = m_audio_to_gpu_map.erase(it);
                } else {
                    ++it;
                }
            }
        }
    } else if (m_direction == TransferDirection::GPU_TO_AUDIO) {
        if (auto audio_buffer = std::dynamic_pointer_cast<AudioBuffer>(buffer)) {
            for (auto it = m_gpu_to_audio_map.begin(); it != m_gpu_to_audio_map.end();) {
                if (it->second == audio_buffer) {
                    it = m_gpu_to_audio_map.erase(it);
                } else {
                    ++it;
                }
            }
        }
    }
}

void TransferProcessor::processing_function(std::shared_ptr<Buffer> buffer)
{
    if (m_direction == TransferDirection::AUDIO_TO_GPU) {
        process_audio_to_gpu(buffer);
    } else if (m_direction == TransferDirection::GPU_TO_AUDIO) {
        process_gpu_to_audio(buffer);
    }
}

void TransferProcessor::process_audio_to_gpu(const std::shared_ptr<Buffer>& gpu_buffer)
{
    auto vk_buffer = std::dynamic_pointer_cast<VKBuffer>(gpu_buffer);
    if (!vk_buffer) {
        MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "TransferProcessor (audio→gpu) requires VKBuffer.");
        return;
    }

    auto transfer_it = std::ranges::find_if(m_audio_to_gpu_map,
        [&vk_buffer](const auto& pair) { return pair.second == vk_buffer; });

    if (transfer_it == m_audio_to_gpu_map.end()) {
        MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "No source AudioBuffer configured for this VKBuffer.");
        return;
    }

    auto source_audio = transfer_it->first;

    if (vk_buffer->is_host_visible()) {
        upload_audio_to_gpu(source_audio, vk_buffer, nullptr);
    } else {
        auto staging_it = m_staging_map.find(vk_buffer);
        if (staging_it == m_staging_map.end()) {
            MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "No staging buffer configured for device-local VKBuffer.");
            return;
        }
        upload_audio_to_gpu(source_audio, vk_buffer, staging_it->second);
    }
}

void TransferProcessor::process_gpu_to_audio(const std::shared_ptr<Buffer>& audio_buffer)
{
    auto audio = std::dynamic_pointer_cast<AudioBuffer>(audio_buffer);
    if (!audio) {
        MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "TransferProcessor (gpu→audio) requires AudioBuffer.");
        return;
    }

    auto transfer_it = std::ranges::find_if(m_gpu_to_audio_map,
        [&audio](const auto& pair) { return pair.second == audio; });

    if (transfer_it == m_gpu_to_audio_map.end()) {
        MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "No source VKBuffer configured for this AudioBuffer.");
        return;
    }

    auto source_gpu = transfer_it->first;

    auto staging_it = m_staging_map.find(source_gpu);
    std::shared_ptr<VKBuffer> staging = (staging_it != m_staging_map.end()) ? staging_it->second : nullptr;

    download_audio_from_gpu(source_gpu, audio, staging);
}

} // namespace MayaFlux::Buffers
