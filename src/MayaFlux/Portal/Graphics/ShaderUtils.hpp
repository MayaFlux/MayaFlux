#pragma once

#include "vulkan/vulkan.hpp"

#include "GraphicsUtils.hpp"

#include "MayaFlux/Kakshya/NDData/VertexLayout.hpp"

namespace MayaFlux::Core {
struct VertexBinding;
struct VertexAttribute;
}

namespace MayaFlux::Portal::Graphics {

using ShaderID = uint64_t;
using DescriptorSetID = uint64_t;
using CommandBufferID = uint64_t;
using FenceID = uint64_t;
using SemaphoreID = uint64_t;
using RenderPipelineID = uint64_t;
using FramebufferID = uint64_t;

constexpr ShaderID INVALID_SHADER = 0;
constexpr DescriptorSetID INVALID_DESCRIPTOR_SET = 0;
constexpr CommandBufferID INVALID_COMMAND_BUFFER = 0;
constexpr FenceID INVALID_FENCE = 0;
constexpr SemaphoreID INVALID_SEMAPHORE = 0;
constexpr RenderPipelineID INVALID_RENDER_PIPELINE = 0;
constexpr FramebufferID INVALID_FRAMEBUFFER = 0;

/**
 * @struct ShaderCompilerConfig
 * @brief Configuration for shader compilation
 */
struct ShaderCompilerConfig {
    bool enable_optimization = true;
    bool enable_debug_info = false; ///< Include debug symbols (line numbers, variable names)
    bool enable_reflection = true; ///< Extract descriptor bindings and metadata
    bool enable_validation = true; ///< Validate SPIR-V after compilation
    std::vector<std::string> include_directories; ///< Paths for #include resolution
    std::unordered_map<std::string, std::string> defines; ///< Preprocessor macros

    ShaderCompilerConfig() = default;
};

/**
 * @struct ShaderSource
 * @brief Shader source descriptor for compilation
 */
struct ShaderSource {
    std::string content; ///< Shader source code or SPIR-V path
    ShaderStage stage;
    std::string entry_point = "main";

    enum class SourceType : uint8_t {
        GLSL_STRING, ///< In-memory GLSL source
        GLSL_FILE, ///< Path to .comp/.vert/.frag/etc
        SPIRV_FILE ///< Path to .spv file
    } type
        = SourceType::GLSL_FILE;

    ShaderSource() = default;
    ShaderSource(std::string content_, ShaderStage stage_, SourceType type_)
        : content(std::move(content_))
        , stage(stage_)
        , type(type_)
    {
    }
};

/**
 * @struct DescriptorBindingConfig
 * @brief Portal-level descriptor binding configuration
 */
struct DescriptorBindingInfo {
    uint32_t set {};
    uint32_t binding {};
    vk::DescriptorType type = vk::DescriptorType::eStorageBuffer;
    vk::DescriptorBufferInfo buffer_info;
    std::string name;
};

/**
 * @struct PushConstantRangeInfo
 * @brief Extracted push constant range from shader reflection
 */
struct PushConstantRangeInfo {
    uint32_t offset;
    uint32_t size;
};

/**
 * @struct ShaderReflectionInfo
 * @brief Extracted reflection information from compiled shader
 */
struct ShaderReflectionInfo {
    ShaderStage stage;
    std::string entry_point;
    std::optional<std::array<uint32_t, 3>> workgroup_size;
    std::vector<DescriptorBindingInfo> descriptor_bindings;
    std::vector<PushConstantRangeInfo> push_constant_ranges;
};

/**
 * @struct RasterizationConfig
 * @brief Rasterization state configuration
 */
struct RasterizationConfig {
    PolygonMode polygon_mode = PolygonMode::FILL;
    CullMode cull_mode = CullMode::BACK;
    bool front_face_ccw = true;
    float line_width = 1.0F;
    bool depth_clamp = false;
    bool depth_bias = false;

    RasterizationConfig() = default;
};

/**
 * @struct DepthStencilConfig
 * @brief Depth and stencil test configuration
 */
struct DepthStencilConfig {
    bool depth_test_enable = true;
    bool depth_write_enable = true;
    CompareOp depth_compare_op = CompareOp::NEVER;
    bool stencil_test_enable = false;

    DepthStencilConfig() = default;
};

/**
 * @struct BlendAttachmentConfig
 * @brief Per-attachment blend configuration
 */
struct BlendAttachmentConfig {
    bool blend_enable = false;
    BlendFactor src_color_factor = BlendFactor::ONE;
    BlendFactor dst_color_factor = BlendFactor::ZERO;
    BlendOp color_blend_op = BlendOp::ADD;
    BlendFactor src_alpha_factor = BlendFactor::ONE;
    BlendFactor dst_alpha_factor = BlendFactor::ZERO;
    BlendOp alpha_blend_op = BlendOp::ADD;

    BlendAttachmentConfig() = default;

    /// @brief Create standard alpha blending configuration
    static BlendAttachmentConfig alpha_blend()
    {
        BlendAttachmentConfig config;
        config.blend_enable = true;
        config.src_color_factor = BlendFactor::SRC_ALPHA;
        config.dst_color_factor = BlendFactor::ONE_MINUS_SRC_ALPHA;
        config.src_alpha_factor = BlendFactor::ONE;
        config.dst_alpha_factor = BlendFactor::ZERO;
        return config;
    }
};

/**
 * @struct RenderPassAttachment
 * @brief Render pass attachment configuration
 */
struct RenderPassAttachment {
    vk::Format format = vk::Format::eB8G8R8A8Unorm;
    vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1;
    vk::AttachmentLoadOp load_op = vk::AttachmentLoadOp::eClear;
    vk::AttachmentStoreOp store_op = vk::AttachmentStoreOp::eStore;
    vk::ImageLayout initial_layout = vk::ImageLayout::eUndefined;
    vk::ImageLayout final_layout = vk::ImageLayout::ePresentSrcKHR;

    RenderPassAttachment() = default;
};

/**
 * @struct RenderPipelineConfig
 * @brief Complete render pipeline configuration
 */
struct RenderPipelineConfig {
    // Shader stages
    ShaderID vertex_shader = INVALID_SHADER;
    ShaderID fragment_shader = INVALID_SHADER;
    ShaderID geometry_shader = INVALID_SHADER; ///< Optional
    ShaderID tess_control_shader = INVALID_SHADER; ///< Optional
    ShaderID tess_eval_shader = INVALID_SHADER; ///< Optional

    // Vertex input
    std::vector<Core::VertexBinding> vertex_bindings;
    std::vector<Core::VertexAttribute> vertex_attributes;

    // Input assembly
    PrimitiveTopology topology = PrimitiveTopology::TRIANGLE_LIST;

    // Optional semantic vertex layout
    std::optional<Kakshya::VertexLayout> semantic_vertex_layout;

    // Use reflection to auto-configure from vertex shader
    bool use_vertex_shader_reflection = true;

    // Rasterization
    RasterizationConfig rasterization;

    // Depth/stencil
    DepthStencilConfig depth_stencil;

    // Blend
    std::vector<BlendAttachmentConfig> blend_attachments;

    // Descriptor sets (similar to compute)
    std::vector<std::vector<DescriptorBindingInfo>> descriptor_sets;

    // Push constants
    size_t push_constant_size {};

    RenderPipelineConfig() = default;
};

} // namespace MayaFlux::Portal::Graphics
