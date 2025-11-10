#pragma once

#include "MayaFlux/Buffers/VKBuffer.hpp"

namespace MayaFlux::Buffers {

/**
 * @class BufferUploadProcessor
 * @brief Transfers data from CPU source buffer to GPU VKBuffer
 *
 * This processor can be attached to multiple VKBuffers and configured with
 * different source buffers for each target. The mapping is maintained internally,
 * adhering to the n-to-n processor-buffer relationship.
 *
 * Usage:
 *   auto upload = std::make_shared<BufferUploadProcessor>();
 *   upload->configure_source(gpu_buffer1, cpu_source1);
 *   upload->configure_source(gpu_buffer2, cpu_source2);
 *
 *   chain->add_processor(upload, gpu_buffer1);
 *   chain->add_processor(upload, gpu_buffer2);
 *
 * Each process() call uploads data from the configured source for that specific buffer.
 */
class MAYAFLUX_API BufferUploadProcessor : public VKBufferProcessor {
public:
    BufferUploadProcessor();
    ~BufferUploadProcessor() override;

    void processing_function(std::shared_ptr<Buffer> buffer) override;
    void on_attach(std::shared_ptr<Buffer> buffer) override;
    void on_detach(std::shared_ptr<Buffer> buffer) override;

    [[nodiscard]] bool is_compatible_with(std::shared_ptr<Buffer> buffer) const override;

    /**
     * @brief Configure source buffer for a specific target
     * @param target VKBuffer that will receive uploads
     * @param source CPU-side buffer to read from (AudioBuffer, etc.)
     */
    void configure_source(const std::shared_ptr<Buffer>& target, std::shared_ptr<Buffer> source);

    /**
     * @brief Remove source configuration for a target
     * @param target VKBuffer to remove configuration for
     */
    void remove_source(const std::shared_ptr<Buffer>& target);

    /**
     * @brief Get configured source for a target
     * @param target VKBuffer to query
     * @return Source buffer, or nullptr if not configured
     */
    [[nodiscard]] std::shared_ptr<Buffer> get_source(const std::shared_ptr<Buffer>& target) const;

private:
    // Maps target VKBuffer -> source Buffer
    std::unordered_map<std::shared_ptr<Buffer>, std::shared_ptr<Buffer>> m_source_map;

    // Maps target VKBuffer -> staging buffer (for device-local transfers)
    std::unordered_map<std::shared_ptr<Buffer>, std::shared_ptr<VKBuffer>> m_staging_buffers;

    void ensure_staging_buffer(const std::shared_ptr<VKBuffer>& target);
    void upload_device_local(const std::shared_ptr<VKBuffer>& target, const Kakshya::DataVariant& data);
};

}
