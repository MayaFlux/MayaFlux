#pragma once

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
    TESS_EVALUATION
};

}
