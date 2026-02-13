#pragma once

namespace MayaFlux::Core {
class Window;
class VKImage;
}

namespace MayaFlux::Portal::Graphics {

//============================================================================
// Primitive Topology
//============================================================================

/**
 * @enum PrimitiveTopology
 * @brief Vertex assembly primitive topology
 *
 * Used to configure how vertices are assembled into primitives
 * before rasterization.
 */
enum class PrimitiveTopology : uint8_t {
    POINT_LIST,
    LINE_LIST,
    LINE_STRIP,
    TRIANGLE_LIST,
    TRIANGLE_STRIP,
    TRIANGLE_FAN
};

//============================================================================
// Rasterization
//============================================================================

/**
 * @enum PolygonMode
 * @brief Rasterization polygon mode
 */
enum class PolygonMode : uint8_t {
    FILL,
    LINE,
    POINT
};

/**
 * @enum CullMode
 * @brief Face culling mode
 */
enum class CullMode : uint8_t {
    NONE,
    FRONT,
    BACK,
    FRONT_AND_BACK
};

//============================================================================
// Depth/Stencil
//============================================================================

/**
 * @enum CompareOp
 * @brief Depth/stencil comparison operation
 */
enum class CompareOp : uint8_t {
    NEVER,
    LESS,
    EQUAL,
    LESS_OR_EQUAL,
    GREATER,
    NOT_EQUAL,
    GREATER_OR_EQUAL,
    ALWAYS
};

//============================================================================
// Blending
//============================================================================

/**
 * @enum BlendFactor
 * @brief Blending factor
 */
enum class BlendFactor : uint8_t {
    ZERO,
    ONE,
    SRC_COLOR,
    ONE_MINUS_SRC_COLOR,
    DST_COLOR,
    ONE_MINUS_DST_COLOR,
    SRC_ALPHA,
    ONE_MINUS_SRC_ALPHA,
    DST_ALPHA,
    ONE_MINUS_DST_ALPHA
};

/**
 * @enum BlendOp
 * @brief Blending operation
 */
enum class BlendOp : uint8_t {
    ADD,
    SUBTRACT,
    REVERSE_SUBTRACT,
    MIN,
    MAX
};

//============================================================================
// Shader Types
//============================================================================

/**
 * @enum ShaderStage
 * @brief User-friendly shader stage enum
 *
 * Abstracts vk::ShaderStageFlagBits for Portal API convenience.
 * Maps directly to Vulkan stages internally.
 */
enum class ShaderStage : uint8_t {
    COMPUTE,
    VERTEX,
    FRAGMENT,
    GEOMETRY,
    TESS_CONTROL,
    TESS_EVALUATION,
    MESH,
    TASK
};

//============================================================================
// Image Helpers
//============================================================================

/**
 * @enum ImageFormat
 * @brief User-friendly image format enum
 *
 * Abstracts Vulkan formats for Portal API convenience.
 * Maps to vk::Format internally.
 */
enum class ImageFormat : uint8_t {
    // Normalized formats
    R8, ///< Single channel 8-bit
    RG8, ///< Two channel 8-bit
    RGB8, ///< Three channel 8-bit
    RGBA8, ///< Four channel 8-bit
    RGBA8_SRGB, ///< Four channel 8-bit sRGB

    // Floating point formats
    R16F, ///< Single channel 16-bit float
    RG16F, ///< Two channel 16-bit float
    RGBA16F, ///< Four channel 16-bit float
    R32F, ///< Single channel 32-bit float
    RG32F, ///< Two channel 32-bit float
    RGBA32F, ///< Four channel 32-bit float

    // Depth/stencil formats
    DEPTH16, ///< 16-bit depth
    DEPTH24, ///< 24-bit depth
    DEPTH32F, ///< 32-bit float depth
    DEPTH24_STENCIL8 ///< 24-bit depth + 8-bit stencil
};

/**
 * @enum FilterMode
 * @brief Texture filtering mode
 */
enum class FilterMode : uint8_t {
    NEAREST, ///< Nearest neighbor (pixelated)
    LINEAR, ///< Bilinear filtering (smooth)
    CUBIC ///< Bicubic filtering (high quality, slower)
};

/**
 * @enum AddressMode
 * @brief Texture addressing mode (wrapping)
 */
enum class AddressMode : uint8_t {
    REPEAT, ///< Repeat texture
    MIRRORED_REPEAT, ///< Mirror and repeat
    CLAMP_TO_EDGE, ///< Clamp to edge color
    CLAMP_TO_BORDER ///< Clamp to border color
};

/**
 * @struct RenderConfig
 * @brief Unified rendering configuration for graphics buffers
 *
 * This is the persistent state that processors query and react to.
 * All rendering parameters in one place, independent of buffer type.
 * Child buffer classes populate it with context-specific defaults
 * during construction, then expose it to their processors.
 *
 * Design:
 * - Owned by VKBuffer as persistent state
 * - Processors query and react to changes
 * - Child classes have their own convenience RenderConfig with defaults
 *   that bridge to this Portal-level config
 */
struct RenderConfig {
    std::shared_ptr<Core::Window> target_window;
    std::string vertex_shader;
    std::string fragment_shader;
    std::string geometry_shader;
    std::string default_texture_binding;
    PrimitiveTopology topology { PrimitiveTopology::POINT_LIST };
    PolygonMode polygon_mode { PolygonMode::FILL };
    CullMode cull_mode { CullMode::NONE };

    std::vector<std::pair<std::string, std::shared_ptr<Core::VKImage>>> additional_textures;

    ///< For child-specific fields
    std::unordered_map<std::string, std::string> extra_string_params;

    bool operator==(const RenderConfig& other) const = default;
};

}
