#pragma once

#include "ShaderProcessor.hpp"

namespace MayaFlux::Buffers {

/**
 * @struct ShaderDispatchConfig
 * @brief Configuration for compute shader dispatch
 */
struct ShaderDispatchConfig {
    uint32_t workgroup_x = 256; ///< Workgroup size X (should match shader)
    uint32_t workgroup_y = 1;
    uint32_t workgroup_z = 1;

    enum class DispatchMode : uint8_t {
        ELEMENT_COUNT, ///< Calculate from buffer element count
        MANUAL, ///< Use explicit group counts
        BUFFER_SIZE, ///< Calculate from buffer byte size
        CUSTOM ///< User-provided calculation function
    } mode
        = DispatchMode::ELEMENT_COUNT;

    // Manual dispatch (MANUAL mode)
    uint32_t group_count_x = 1;
    uint32_t group_count_y = 1;
    uint32_t group_count_z = 1;

    std::function<std::array<uint32_t, 3>(const std::shared_ptr<VKBuffer>&)> custom_calculator;

    ShaderDispatchConfig() = default;
};

/**
 * @class ComputeProcessor
 * @brief Specialized ShaderProcessor for Compute Pipelines
 *
 * ComputeProcessor extends ShaderProcessor to handle the specifics of compute shader execution:
 * - **Pipeline Creation:** Creates and manages `VKComputePipeline`.
 * - **Dispatch Logic:** Calculates workgroup counts based on buffer size or manual configuration.
 * - **Execution:** Records `vkCmdDispatch` commands.
 *
 * It inherits all shader resource management (descriptors, push constants, bindings) from
 * ShaderProcessor, adding only what is necessary for compute dispatch.
 *
 * Dispatch Modes:
 * - **ELEMENT_COUNT:** (Default) Calculates groups based on buffer element count / workgroup size.
 * - **BUFFER_SIZE:** Calculates groups based on total buffer bytes / workgroup size.
 * - **MANUAL:** Uses fixed group counts (x, y, z).
 * - **CUSTOM:** Uses a user-provided lambda to calculate dispatch dimensions.
 *
 * Usage:
 *   // Simple usage - single buffer processor
 *   auto processor = std::make_shared<ComputeProcessor>("shaders/kernel.comp");
 *   processor->bind_buffer("input_buffer", my_buffer);
 *   my_buffer->set_default_processor(processor);
 *
 *   // Advanced - multi-buffer with explicit bindings
 *   ComputeProcessorConfig config("shaders/complex.comp");
 *   config.bindings["input"] = ShaderBinding(0, 0);
 *   config.bindings["output"] = ShaderBinding(0, 1);
 *   config.dispatch.workgroup_x = 512;
 *
 *   auto processor = std::make_shared<ComputeProcessor>(config);
 *   processor->bind_buffer("input", input_buffer);
 *   processor->bind_buffer("output", output_buffer);
 *
 *   chain->add_processor(processor, input_buffer);
 *   chain->add_processor(processor, output_buffer);
 *
 *   // With push constants
 *   struct Params { float scale; uint32_t iterations; };
 *   processor->set_push_constant_size<Params>();
 *   processor->set_push_constant_data(Params{2.0f, 100});
 *
 * Specialized Processors:
 *   class FFTProcessor : public ComputeProcessor {
 *       FFTProcessor() : ComputeProcessor("shaders/fft.comp") {
 *           configure_fft_bindings();
 *       }
 *
 *       void on_attach(std::shared_ptr<Buffer> buffer) override {
 *           ComputeProcessor::on_attach(buffer);
 *           // FFT-specific setup
 *       }
 *   };
 */
class MAYAFLUX_API ComputeProcessor : public ShaderProcessor {
public:
    /**
     * @brief Construct processor with shader path
     * @param shader_path Path to compute shader (.comp or .spv)
     * @param workgroup_x Workgroup size X (default 256)
     */
    explicit ComputeProcessor(const std::string& shader_path, uint32_t workgroup_x = 256);

    //==========================================================================
    // Dispatch Configuration
    //==========================================================================

    /**
     * @brief Set workgroup size (should match shader local_size)
     * @param x Workgroup size X
     * @param y Workgroup size Y (default 1)
     * @param z Workgroup size Z (default 1)
     */
    void set_workgroup_size(uint32_t x, uint32_t y = 1, uint32_t z = 1);

    /**
     * @brief Set dispatch mode
     * @param mode Dispatch calculation mode
     */
    void set_dispatch_mode(ShaderDispatchConfig::DispatchMode mode);

    /**
     * @brief Set manual dispatch group counts
     * @param x Group count X
     * @param y Group count Y (default 1)
     * @param z Group count Z (default 1)
     */
    void set_manual_dispatch(uint32_t x, uint32_t y = 1, uint32_t z = 1);

    /**
     * @brief Set custom dispatch calculator
     * @param calculator Function that calculates dispatch from buffer
     */
    void set_custom_dispatch(std::function<std::array<uint32_t, 3>(const std::shared_ptr<VKBuffer>&)> calculator);

    /**
     * @brief Get current dispatch configuration
     */
    [[nodiscard]] const ShaderDispatchConfig& get_dispatch_config() const { return m_dispatch_config; }

    /**
     * @brief Check if pipeline is created
     */
    bool is_pipeline_ready() const { return m_pipeline_id != Portal::Graphics::INVALID_COMPUTE_PIPELINE; }

protected:
    /**
     * @brief Calculate dispatch size from buffer
     * @param buffer Buffer to process
     * @return {group_count_x, group_count_y, group_count_z}
     *
     * Override for custom dispatch calculation logic.
     * Default implementation uses m_config.dispatch settings.
     */
    virtual std::array<uint32_t, 3> calculate_dispatch_size(const std::shared_ptr<VKBuffer>& buffer);

    void initialize_pipeline(const std::shared_ptr<VKBuffer>& buffer) override;

    void initialize_descriptors(const std::shared_ptr<VKBuffer>& buffer) override;

    void cleanup() override;

private:
    ShaderDispatchConfig m_dispatch_config;

    void execute_shader(const std::shared_ptr<VKBuffer>& buffer) override;

    Portal::Graphics::ComputePipelineID m_pipeline_id = Portal::Graphics::INVALID_COMPUTE_PIPELINE;
};

} // namespace MayaFlux::Buffers
