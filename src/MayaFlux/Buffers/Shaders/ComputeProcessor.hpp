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
    } mode = DispatchMode::ELEMENT_COUNT;

    // Manual dispatch (MANUAL mode)
    uint32_t group_count_x = 1;
    uint32_t group_count_y = 1;
    uint32_t group_count_z = 1;
    uint32_t iteration_count = 1; ///< Dispatches recorded per execute cycle.

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

    /**
     * @brief Construct processor from a generated ShaderSpec.
     * @param spec ShaderSpec produced by ShaderSpec::Assemble::build().
     *
     * Delegates to ShaderProcessor(ShaderConfig(spec)) for compilation,
     * then applies spec.workgroup_size to this processor's own
     * ShaderDispatchConfig — workgroup sizing is a ComputeProcessor
     * concern the shared ShaderProcessor base has no knowledge of.
     */
    explicit ComputeProcessor(const Portal::Graphics::ShaderSpec& spec);

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
     * @brief Set how many dispatches are recorded per execute cycle.
     * @param count Dispatch count. Values below 1 are clamped to 1.
     *
     * All iterations record into one command buffer and submit once.
     * on_iteration runs before each; on_iteration_barrier runs between
     * consecutive iterations. At the default of 1 the recorded command
     * stream is identical to a single-dispatch cycle.
     */
    void set_iteration_count(uint32_t count);

    /** @brief Dispatches recorded per execute cycle. */
    [[nodiscard]] uint32_t get_iteration_count() const { return m_dispatch_config.iteration_count; }

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

    /**
     * @brief Called before each iteration's push constants and dispatch.
     * @param cmd_id Command buffer being recorded into.
     * @param buffer Buffer under processing.
     * @param index Zero-based iteration index.
     * @return False to skip this iteration's dispatch.
     *
     * The place to rewrite descriptor bindings for ping-pong resources and
     * to update m_push_constant_data, since push constants are re-pushed
     * after every successful return.
     */
    virtual bool on_iteration(
        Portal::Graphics::CommandBufferID cmd_id,
        const std::shared_ptr<VKBuffer>& buffer,
        uint32_t index);

    /**
     * @brief Called after each iteration except the last.
     * @param cmd_id Command buffer being recorded into.
     * @param buffer Buffer under processing.
     * @param index Zero-based index of the iteration just recorded.
     *
     * Default issues a compute-to-compute buffer_barrier on the attached
     * buffer's own handle. Override when the hazard is on resources the
     * attached buffer does not own, such as raw double-buffered state.
     */
    virtual void on_iteration_barrier(
        Portal::Graphics::CommandBufferID cmd_id,
        const std::shared_ptr<VKBuffer>& buffer,
        uint32_t index);

    void initialize_pipeline(const std::shared_ptr<VKBuffer>& buffer) override;

    void initialize_descriptors(const std::shared_ptr<VKBuffer>& buffer) override;

    void cleanup() override;

private:
    ShaderDispatchConfig m_dispatch_config;

    std::vector<uint8_t> m_push_constant_scratch; ///< Coalesced push constant bindings, reused across iterations.

    void execute_shader(const std::shared_ptr<VKBuffer>& buffer) override;

    Portal::Graphics::ComputePipelineID m_pipeline_id = Portal::Graphics::INVALID_COMPUTE_PIPELINE;
};

} // namespace MayaFlux::Buffers
