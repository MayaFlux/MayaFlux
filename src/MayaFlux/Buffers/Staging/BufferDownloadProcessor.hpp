#pragma once

#include "MayaFlux/Buffers/VKBuffer.hpp"

namespace MayaFlux::Buffers {

/**
 * @class BufferDownloadProcessor
 * @brief Transfers data from GPU VKBuffer to CPU target buffer
 *
 * Inverse of BufferUploadProcessor.
 * Handles staging and transfer from device-local buffers.
 *
 * Usage:
 *   auto download = std::make_shared<BufferDownloadProcessor>();
 *   download->configure_target(gpu_buffer1, cpu_target1);
 *   download->configure_target(gpu_buffer2, cpu_target2);
 *
 *   chain->add_processor(download, gpu_buffer1);
 *   chain->add_processor(download, gpu_buffer2);
 *
 * Each process() call downloads latest data to the configured target for that source.
 */
class MAYAFLUX_API BufferDownloadProcessor : public VKBufferProcessor {
public:
    BufferDownloadProcessor();
    ~BufferDownloadProcessor() override;

    void processing_function(const std::shared_ptr<Buffer>& buffer) override;
    void on_attach(const std::shared_ptr<Buffer>& buffer) override;
    void on_detach(const std::shared_ptr<Buffer>& buffer) override;

    [[nodiscard]] bool is_compatible_with(const std::shared_ptr<Buffer>& buffer) const override;

    /**
     * @brief Configure target buffer for a specific source
     * @param source VKBuffer to download from
     * @param target CPU-side buffer to write to (AudioBuffer, etc.)
     */
    void configure_target(const std::shared_ptr<Buffer>& source, std::shared_ptr<Buffer> target);

    /**
     * @brief Remove target configuration for a source
     * @param source VKBuffer to remove configuration for
     */
    void remove_target(const std::shared_ptr<Buffer>& source);

    /**
     * @brief Get configured target for a source
     * @param source VKBuffer to query
     * @return Target buffer, or nullptr if not configured
     */
    [[nodiscard]] std::shared_ptr<Buffer> get_target(const std::shared_ptr<Buffer>& source) const;

private:
    // Maps source VKBuffer -> target Buffer
    std::unordered_map<std::shared_ptr<Buffer>, std::shared_ptr<Buffer>> m_target_map;

    // Maps source VKBuffer -> staging buffer (for device-local transfers)
    std::unordered_map<std::shared_ptr<Buffer>, std::shared_ptr<VKBuffer>> m_staging_buffers;

    void ensure_staging_buffer(const std::shared_ptr<VKBuffer>& source);
    void download_host_visible(const std::shared_ptr<VKBuffer>& source);
    void download_device_local(const std::shared_ptr<VKBuffer>& source);
};

} // namespace MayaFlux::Buffers
