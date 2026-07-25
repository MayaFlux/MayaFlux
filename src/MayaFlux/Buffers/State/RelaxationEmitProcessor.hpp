#pragma once

#include "MayaFlux/Buffers/Shaders/ComputeProcessor.hpp"

namespace MayaFlux::Buffers {

class RelaxationGridBuffer;

/**
 * @class RelaxationEmitProcessor
 * @brief ComputeProcessor that reads the current front generation's raw
 *        state buffer from a RelaxationGridBuffer and writes vertex data
 *        directly into the owning VKBuffer's own storage (Usage::VERTEX).
 *
 * Mirrors SDFMeshProcessor's role in ComputeMeshBuffer: dispatches into
 * the attached buffer itself, bypassing all CPU readback. The "cell_state"
 * binding is written directly via ShaderFoundry::update_descriptor_buffer
 * against the grid's raw front-generation handle, since that buffer has no
 * VKBuffer wrapper. The "vertices" binding targets the attached buffer's
 * own vk::Buffer and is written the same way, for symmetry and to avoid
 * mixing bind_buffer with direct descriptor writes within one processor.
 *
 * Runs as a flat processor after RelaxationStepProcessor in the chain, so
 * each cycle emits vertices from whichever generation the step just
 * finished producing.
 */
class MAYAFLUX_API RelaxationEmitProcessor : public ComputeProcessor {
public:
    /**
     * @brief Construct an emit processor for the given shader file.
     * @param shader_path Path to the compute shader that reads cell_state
     *        and writes vertices.
     * @param workgroup_x Local workgroup size along X, forwarded to ComputeProcessor.
     */
    explicit RelaxationEmitProcessor(const std::string& shader_path, uint32_t workgroup_x = 16);

    /**
     * @brief Construct an emit processor from a generated ShaderSpec.
     * @param spec ShaderSpec implementing the state-to-vertex mapping,
     *        typically an Elementwise spec built from a factory such as
     *        the binary/scalar-ramp/rgba emit shaders.
     */
    explicit RelaxationEmitProcessor(const Portal::Graphics::ShaderSpec& spec);

protected:
    /**
     * @brief Write the cell_state and vertices descriptor bindings for
     *        this cycle's dispatch.
     * @param cmd_id Command buffer this cycle's dispatch will be recorded into.
     * @param buffer The attached RelaxationGridBuffer, received as VKBuffer.
     * @return True unconditionally; emission always runs when the buffer
     *         is a valid RelaxationGridBuffer.
     */
    bool on_before_execute(Portal::Graphics::CommandBufferID cmd_id, const std::shared_ptr<VKBuffer>& buffer) override;

    /**
     * @brief When attached to a RelaxationGridBuffer, sets up the dispatch
     *        configuration to cover the entire grid with a single workgroup
     *        dimension along Y and Z, and a workgroup size along X that is
     *        either the default 16 or the value supplied at construction.
     * @param buffer The attached buffer, expected to be a RelaxationGridBuffer.
     */
    void on_attach(const std::shared_ptr<Buffer>& buffer) override;

    /**
     * @brief Write the cell_state and vertices descriptors for the grid's
     *        current front generation, then run the normal shader
     *        processing path.
     */
    void processing_function(const std::shared_ptr<Buffer>& buffer) override;
};

} // namespace MayaFlux::Buffers
