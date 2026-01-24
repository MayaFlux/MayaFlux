#pragma once

#include "MayaFlux/Buffers/VKBuffer.hpp"

namespace MayaFlux::Buffers {

class AudioBuffer;

/**
 * @enum TransferDirection
 * @brief Specifies the direction of data transfer
 */
enum class TransferDirection : uint8_t {
    AUDIO_TO_GPU = 0, // Upload: AudioBuffer → VKBuffer
    GPU_TO_AUDIO = 1, // Download: VKBuffer → AudioBuffer
    BIDIRECTIONAL = 2 // Both directions (future, complex)
};

class TransferProcessor : public VKBufferProcessor {
public:
    TransferProcessor();

    /**
     * @brief Create transfer from audio to GPU
     * @param source Source AudioBuffer
     * @param target Target VKBuffer
     */
    TransferProcessor(
        const std::shared_ptr<AudioBuffer>& source,
        const std::shared_ptr<VKBuffer>& target);

    /**
     * @brief Create transfer with explicit direction
     * @param audio_buffer AudioBuffer for transfer
     * @param gpu_buffer VKBuffer for transfer
     * @param direction Direction of transfer (AUDIO_TO_GPU or GPU_TO_AUDIO)
     */
    TransferProcessor(
        const std::shared_ptr<AudioBuffer>& audio_buffer,
        const std::shared_ptr<VKBuffer>& gpu_buffer,
        TransferDirection direction);

    ~TransferProcessor() override = default;

    /**
     * @brief Configure audio→GPU transfer
     */
    void connect_audio_to_gpu(
        const std::shared_ptr<AudioBuffer>& source,
        const std::shared_ptr<VKBuffer>& target);

    /**
     * @brief Configure GPU→audio transfer
     */
    void connect_gpu_to_audio(
        const std::shared_ptr<VKBuffer>& source,
        const std::shared_ptr<AudioBuffer>& target);

    /**
     * @brief Set up staging buffer for device-local GPU buffer
     */
    void setup_staging(
        const std::shared_ptr<VKBuffer>& target,
        std::shared_ptr<VKBuffer> staging_buffer);

    /**
     * @brief Get current transfer direction
     */
    TransferDirection get_direction() const { return m_direction; }

    /**
     * @brief Set transfer direction
     */
    void set_direction(TransferDirection direction) { m_direction = direction; }

    void on_attach(const std::shared_ptr<Buffer>& buffer) override;
    void on_detach(const std::shared_ptr<Buffer>& buffer) override;

    void processing_function(std::shared_ptr<Buffer> buffer) override;

private:
    // Audio→GPU transfers
    std::unordered_map<std::shared_ptr<AudioBuffer>, std::shared_ptr<VKBuffer>> m_audio_to_gpu_map;

    // GPU→Audio transfers
    std::unordered_map<std::shared_ptr<VKBuffer>, std::shared_ptr<AudioBuffer>> m_gpu_to_audio_map;

    // Staging buffers (for device-local GPU memory)
    std::unordered_map<std::shared_ptr<VKBuffer>, std::shared_ptr<VKBuffer>> m_staging_map;

    // Current transfer direction
    TransferDirection m_direction = TransferDirection::AUDIO_TO_GPU;

    bool validate_audio_to_gpu(const std::shared_ptr<VKBuffer>& target) const;
    bool validate_gpu_to_audio(const std::shared_ptr<AudioBuffer>& target) const;

    void process_audio_to_gpu(const std::shared_ptr<Buffer>& gpu_buffer);
    void process_gpu_to_audio(const std::shared_ptr<Buffer>& audio_buffer);
};

} // namespace MayaFlux::Buffers
