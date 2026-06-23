#pragma once

#include "GpuResourceManager.hpp"
#include "MayaFlux/Portal/Graphics/ShaderFoundry.hpp"
#include "MayaFlux/Portal/Graphics/ShaderSpec.hpp"

namespace MayaFlux::Yantra {

/**
 * @brief Translate a BindingDirection to a GpuBufferBinding::Direction.
 */
[[nodiscard]] inline GpuBufferBinding::Direction to_binding_direction(
    Portal::Graphics::BindingDirection d) noexcept
{
    switch (d) {
    case Portal::Graphics::BindingDirection::Input:
        return GpuBufferBinding::Direction::INPUT;
    case Portal::Graphics::BindingDirection::Output:
        return GpuBufferBinding::Direction::OUTPUT;
    default:
        return GpuBufferBinding::Direction::INPUT_OUTPUT;
    }
}

/**
 * @brief Translate a BindingSlot modality to a GpuBufferBinding::ElementType.
 */
[[nodiscard]] inline GpuBufferBinding::ElementType to_element_type(
    Kakshya::DataModality m) noexcept
{
    switch (m) {
    case Kakshya::DataModality::TEXTURE_2D:
        return GpuBufferBinding::ElementType::IMAGE_SAMPLED;
    case Kakshya::DataModality::IMAGE_2D:
        return GpuBufferBinding::ElementType::IMAGE_STORAGE;
    default:
        return GpuBufferBinding::ElementType::FLOAT32;
    }
}

/**
 * @brief Derive a GpuBufferBinding list from a ShaderSpec.
 *
 * Translates ShaderSpec binding declarations to the GpuBufferBinding
 * vocabulary understood by GpuResourceManager and any GpuExecutionContext
 * subclass. The result is suitable for passing to declare_buffer_bindings()
 * overrides or directly to GpuResourceManager::initialise().
 */
[[nodiscard]] inline std::vector<GpuBufferBinding> bindings_from_spec(
    const Portal::Graphics::ShaderSpec& spec)
{
    std::vector<GpuBufferBinding> out;
    out.reserve(spec.bindings.size());
    for (const auto& b : spec.bindings) {
        out.push_back({
            .set = 0,
            .binding = b.binding_index,
            .direction = to_binding_direction(b.direction),
            .element_type = to_element_type(b.modality),
        });
    }
    return out;
}

/**
 * @brief Derive a GpuShaderConfig from a ShaderSpec.
 *
 * Compiles (or retrieves cached) the shader via ShaderFoundry and
 * packages the result with workgroup size and push constant size.
 * Returns a config with INVALID_SHADER on compilation failure.
 *
 * Callers use this alongside bindings_from_spec() to configure any
 * concrete GpuExecutionContext subclass without depending on a specific
 * executor type.
 *
 * @code
 * auto spec  = ShaderSpec::Assemble{}
 *     .ssbo("sig", BindingDirection::InOut, Kakshya::GpuDataFormat::FLOAT32)
 *     .pc("gain")
 *     .op(KernelOp::Scale)
 *     .build();
 *
 * auto cfg      = config_from_spec(spec);
 * auto bindings = bindings_from_spec(spec);
 *
 * auto exec = std::make_shared<ShaderExecutionContext<>>(cfg, bindings);
 * my_op->set_gpu_backend(exec);
 * @endcode
 */
[[nodiscard]] inline GpuShaderConfig config_from_spec(
    const Portal::Graphics::ShaderSpec& spec)
{
    return GpuShaderConfig {
        .workgroup_size = spec.workgroup_size,
        .push_constant_size = spec.push_constant_bytes,
        .shader_id = Portal::Graphics::get_shader_foundry().load_shader(spec),
    };
}

} // namespace MayaFlux::Yantra
