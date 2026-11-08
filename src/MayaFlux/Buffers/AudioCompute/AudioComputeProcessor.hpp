#pragma once

#include "MayaFlux/Buffers/Shaders/ComputeProcessor.hpp"

namespace MayaFlux::Buffers {

class AudioBuffer;

/**
 * @class AudioComputeProcessor
 * @brief Runs a compute shader over one audio block on the GPU
 *
 * Install as the default processor of a child AudioBuffer under AUDIO_PARALLEL.
 * Each cycle the child's samples are written to an owned host-visible storage
 * buffer, bound as "block" at set 0, binding 0. The shader is dispatched
 * synchronously and the result is read back into the child.
 *
 * A dispatch that does not run leaves the child cleared.
 *
 * Usage:
 *   auto proc = std::make_shared<AudioComputeProcessor>("shaders/gain.comp");
 *   child->set_default_processor(proc);
 */
class MAYAFLUX_API AudioComputeProcessor : public ComputeProcessor {
public:
    /**
     * @param shader_path Path to a compute shader (.comp or .spv)
     * @param native_f64 True if the shader reads samples as double. Requires
     *        shaderFloat64 on the selected device. False reads them as uvec2.
     * @param workgroup_x Workgroup size X, must match the shader
     */
    explicit AudioComputeProcessor(
        const std::string& shader_path,
        bool native_f64,
        uint32_t workgroup_x = 256);

    /**
     * @param spec Generated shader specification
     * @param native_f64 True if the shader reads samples as double. Requires
     *        shaderFloat64 on the selected device. False reads them as uvec2.
     */
    explicit AudioComputeProcessor(
        const Portal::Graphics::ShaderSpec& spec,
        bool native_f64);

    void processing_function(const std::shared_ptr<Buffer>& buffer) override;
    void on_attach(const std::shared_ptr<Buffer>& buffer) override;
    void on_detach(const std::shared_ptr<Buffer>& buffer) override;
    [[nodiscard]] bool is_compatible_with(const std::shared_ptr<Buffer>& buffer) const override;

    [[nodiscard]] bool is_native_f64() const { return m_native_f64; }

protected:
    /**
     * @brief Dispatch size from the block's frame count, not its byte size
     */
    std::array<uint32_t, 3> calculate_dispatch_size(const std::shared_ptr<VKBuffer>& buffer) override;

private:
    bool m_native_f64 {};
    std::atomic<bool> m_attached { false }; ///< Set on attach, cleared on detach.
    std::atomic<bool> m_in_flight { false }; ///< Set while processing_function runs on the processing thread.
    std::atomic<bool> m_holds_block { false }; ///< True while m_block is allocated and bound.
    std::shared_ptr<VKBuffer> m_block;
    uint32_t m_frames {};

    void configure();
    void resize_block(uint32_t frames);
    void process_block(const std::shared_ptr<Buffer>& buffer);
    void release(const std::shared_ptr<Buffer>& buffer);
};

} // namespace MayaFlux::Buffers
