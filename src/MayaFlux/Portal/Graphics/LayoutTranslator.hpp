#pragma once

#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKGraphicsPipeline.hpp"
#include "MayaFlux/Kakshya/NDData/VertexLayout.hpp"

#include <vulkan/vulkan.hpp>

namespace MayaFlux::Portal::Graphics {

/**
 * @class VertexLayoutTranslator
 * @brief Translates semantic vertex layouts to Vulkan pipeline state
 *
 * Bridges the gap between Kakshya data semantics and Vulkan's
 * VkVertexInputBindingDescription / VkVertexInputAttributeDescription.
 *
 * All Vulkan type translation happens here. Keeps VKBuffer pure semantic.
 */
class MAYAFLUX_API VertexLayoutTranslator {
public:
    /**
     * @brief Convert semantic modality to Vulkan format
     * @param modality Kakshya::DataModality
     * @return Corresponding vk::Format
     */
    static vk::Format modality_to_vk_format(Kakshya::DataModality modality);

    /**
     * @brief Translate a semantic vertex layout to Vulkan binding/attribute descriptions
     * @param layout Semantic vertex layout from VKBuffer
     * @param binding_index Vulkan binding point (usually 0)
     * @return Pair of (bindings, attributes) ready for VkPipelineVertexInputStateCreateInfo
     */
    static std::pair<
        std::vector<Core::VertexBinding>,
        std::vector<Core::VertexAttribute>>
    translate_layout(
        const Kakshya::VertexLayout& layout,
        uint32_t binding_index = 0);

    /**
     * @brief Get size in bytes for a modality
     * Useful for computing strides, offsets, etc.
     */
    static uint32_t get_modality_size_bytes(Kakshya::DataModality modality);

    /**
     * @brief Describe a modality in human-readable form
     * e.g., "vec3" for VERTEX_POSITIONS_3D
     */
    static std::string_view describe_modality(Kakshya::DataModality modality);
};

} // namespace MayaFlux::Portal::Graphics
