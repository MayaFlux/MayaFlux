#pragma once

#include <vulkan/vulkan.hpp>

namespace MayaFlux::Core {

/**
 * @enum ImageAccess
 * @brief How a range of an image is used, from which a layout and a barrier follow.
 *
 * A transition names the use it leaves and the use it enters. The layout, pipeline
 * stages and access masks of each side come from image_access_info(), so no
 * pair of uses needs its own barrier code.
 */
enum class ImageAccess : uint8_t {
    UNDEFINED, ///< Contents not needed, or not yet written
    COLOR_ATTACHMENT, ///< Written or blended as a color attachment
    DEPTH_ATTACHMENT, ///< Depth or stencil attachment
    SHADER_READ, ///< Sampled or read by vertex, fragment or compute shaders
    STORAGE, ///< Read and written by shaders as a storage image
    TRANSFER_SRC, ///< Source of a copy or blit
    TRANSFER_DST, ///< Destination of a copy, blit or clear
    PRESENT ///< Handed to the presentation engine
};

/**
 * @struct ImageAccessInfo
 * @brief Layout, stages and access masks that describe one ImageAccess.
 */
struct ImageAccessInfo {
    vk::ImageLayout layout { vk::ImageLayout::eUndefined };
    vk::PipelineStageFlags2 stages {};
    vk::AccessFlags2 access {};
};

/**
 * @brief Layout, stages and access masks for an ImageAccess.
 *
 * The same masks serve as the source and the destination of a barrier: the
 * source side waits for every access the use can make, the destination side
 * makes them visible.
 */
[[nodiscard]] inline ImageAccessInfo image_access_info(ImageAccess access) noexcept
{
    using Layout = vk::ImageLayout;
    using Stage = vk::PipelineStageFlagBits2;
    using Access = vk::AccessFlagBits2;

    switch (access) {
    case ImageAccess::COLOR_ATTACHMENT:
        return { Layout::eColorAttachmentOptimal,
            Stage::eColorAttachmentOutput,
            Access::eColorAttachmentRead | Access::eColorAttachmentWrite };
    case ImageAccess::DEPTH_ATTACHMENT:
        return { Layout::eDepthStencilAttachmentOptimal,
            Stage::eEarlyFragmentTests | Stage::eLateFragmentTests,
            Access::eDepthStencilAttachmentRead | Access::eDepthStencilAttachmentWrite };
    case ImageAccess::SHADER_READ:
        return { Layout::eShaderReadOnlyOptimal,
            Stage::eAllGraphics | Stage::eComputeShader,
            Access::eShaderRead };
    case ImageAccess::STORAGE:
        return { Layout::eGeneral,
            Stage::eAllGraphics | Stage::eComputeShader,
            Access::eShaderRead | Access::eShaderWrite };
    case ImageAccess::TRANSFER_SRC:
        return { Layout::eTransferSrcOptimal, Stage::eAllTransfer, Access::eTransferRead };
    case ImageAccess::TRANSFER_DST:
        return { Layout::eTransferDstOptimal, Stage::eAllTransfer, Access::eTransferWrite };
    case ImageAccess::PRESENT:
        return { Layout::ePresentSrcKHR, Stage::eNone, Access::eNone };
    case ImageAccess::UNDEFINED:
        break;
    }
    return { Layout::eUndefined, Stage::eNone, Access::eNone };
}

/**
 * @brief The ImageAccess an image layout stands for.
 * @return std::nullopt for a layout no ImageAccess describes.
 */
[[nodiscard]] inline std::optional<ImageAccess> image_access_of(vk::ImageLayout layout) noexcept
{
    switch (layout) {
    case vk::ImageLayout::eUndefined:
        return ImageAccess::UNDEFINED;
    case vk::ImageLayout::eColorAttachmentOptimal:
        return ImageAccess::COLOR_ATTACHMENT;
    case vk::ImageLayout::eDepthStencilAttachmentOptimal:
        return ImageAccess::DEPTH_ATTACHMENT;
    case vk::ImageLayout::eShaderReadOnlyOptimal:
        return ImageAccess::SHADER_READ;
    case vk::ImageLayout::eGeneral:
        return ImageAccess::STORAGE;
    case vk::ImageLayout::eTransferSrcOptimal:
        return ImageAccess::TRANSFER_SRC;
    case vk::ImageLayout::eTransferDstOptimal:
        return ImageAccess::TRANSFER_DST;
    case vk::ImageLayout::ePresentSrcKHR:
        return ImageAccess::PRESENT;
    default:
        return std::nullopt;
    }
}

/**
 * @brief Record a barrier moving a subresource range between two uses of an image.
 *
 * Source and destination masks and layouts come from image_access_info(). Records
 * only; it does not track the image's layout.
 */
inline void record_image_barrier(
    vk::CommandBuffer cmd,
    vk::Image image,
    const vk::ImageSubresourceRange& range,
    ImageAccess from,
    ImageAccess to)
{
    const ImageAccessInfo source = image_access_info(from);
    const ImageAccessInfo destination = image_access_info(to);

    vk::ImageMemoryBarrier2 barrier {};
    barrier.srcStageMask = source.stages;
    barrier.srcAccessMask = source.access;
    barrier.dstStageMask = destination.stages;
    barrier.dstAccessMask = destination.access;
    barrier.oldLayout = source.layout;
    barrier.newLayout = destination.layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange = range;

    vk::DependencyInfo dependency {};
    dependency.imageMemoryBarrierCount = 1;
    dependency.pImageMemoryBarriers = &barrier;
    cmd.pipelineBarrier2(dependency);
}

/**
 * @struct ImageRange
 * @brief A range of mip levels and array layers of one image.
 *
 * The defaults cover everything from the first mip level and layer onward.
 */
struct ImageRange {
    uint32_t base_mip { 0 };
    uint32_t mip_count { VK_REMAINING_MIP_LEVELS };
    uint32_t base_layer { 0 };
    uint32_t layer_count { VK_REMAINING_ARRAY_LAYERS };

    bool operator==(const ImageRange&) const = default;
};

/**
 * @struct ImageViewKey
 * @brief What a view of an image covers.
 *
 * An unset type is chosen from the image and the layer count, and an empty
 * aspect is the image's own. Name the aspect when the image holds depth and
 * stencil, since a view that is sampled or used as an attachment takes one.
 */
struct ImageViewKey {
    std::optional<vk::ImageViewType> type;
    ImageRange range;
    vk::ImageAspectFlags aspect {};

    bool operator==(const ImageViewKey&) const = default;
};

} // namespace MayaFlux::Core
