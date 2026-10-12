#include "gtest/gtest.h"

#include <algorithm>

#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKEnumUtils.hpp"
#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKImage.hpp"

namespace MayaFlux::Test {
namespace {

    using Core::VKImage;
    using Kakshya::DataDimension;

    bool has_flag(vk::ImageUsageFlags flags, vk::ImageUsageFlagBits bit)
    {
        return (flags & bit) == bit;
    }

    const DataDimension* find_role(const VKImage& image, DataDimension::Role role)
    {
        const auto& dims = image.get_dimensions();
        auto it = std::ranges::find_if(dims, [role](const DataDimension& d) { return d.role == role; });
        return it == dims.end() ? nullptr : &*it;
    }

    struct FormatBytes {
        vk::Format format;
        uint32_t bytes;
    };

    class VkFormatBytesPerPixelTest : public ::testing::TestWithParam<FormatBytes> { };

    struct UsageFlags {
        VKImage::Usage usage;
        vk::ImageUsageFlags expected;
        vk::ImageUsageFlags forbidden;
    };

    class VKImageUsageFlagsTest : public ::testing::TestWithParam<UsageFlags> { };

    struct ChannelCount {
        vk::Format format;
        uint64_t channels;
    };

    class VKImageChannelDimensionTest : public ::testing::TestWithParam<ChannelCount> { };

}

TEST_P(VkFormatBytesPerPixelTest, ReportsTexelSize)
{
    const auto [format, bytes] = GetParam();
    EXPECT_EQ(Core::vk_format_bytes_per_pixel(format), bytes) << vk::to_string(format);
}

INSTANTIATE_TEST_SUITE_P(Formats, VkFormatBytesPerPixelTest,
    ::testing::Values(
        FormatBytes { vk::Format::eR8Unorm, 1 },
        FormatBytes { vk::Format::eR8G8Unorm, 2 },
        FormatBytes { vk::Format::eR16Sfloat, 2 },
        FormatBytes { vk::Format::eR8G8B8Unorm, 3 },
        FormatBytes { vk::Format::eR8G8B8A8Unorm, 4 },
        FormatBytes { vk::Format::eB8G8R8A8Srgb, 4 },
        FormatBytes { vk::Format::eR32Sfloat, 4 },
        FormatBytes { vk::Format::eR16G16B16A16Sfloat, 8 },
        FormatBytes { vk::Format::eR32G32Sfloat, 8 },
        FormatBytes { vk::Format::eR32G32B32Sfloat, 12 },
        FormatBytes { vk::Format::eR32G32B32A32Sfloat, 16 },
        FormatBytes { vk::Format::eUndefined, 4 }));

TEST(VKImageTest, ConstructorStoresDescription)
{
    VKImage image(8, 4, 1, vk::Format::eR8G8B8A8Unorm, VKImage::Usage::TEXTURE_2D,
        VKImage::Type::TYPE_2D, 3, 2, Kakshya::DataModality::IMAGE_COLOR);

    EXPECT_EQ(image.get_width(), 8U);
    EXPECT_EQ(image.get_height(), 4U);
    EXPECT_EQ(image.get_depth(), 1U);
    EXPECT_EQ(image.get_format(), vk::Format::eR8G8B8A8Unorm);
    EXPECT_EQ(image.get_usage(), VKImage::Usage::TEXTURE_2D);
    EXPECT_EQ(image.get_type(), VKImage::Type::TYPE_2D);
    EXPECT_EQ(image.get_mip_levels(), 3U);
    EXPECT_EQ(image.get_array_layers(), 2U);
    EXPECT_EQ(image.get_modality(), Kakshya::DataModality::IMAGE_COLOR);
}

TEST(VKImageTest, FreshImageHasNoResourcesAndUndefinedLayout)
{
    VKImage image(4, 4, 1, vk::Format::eR8G8B8A8Unorm, VKImage::Usage::TEXTURE_2D);

    EXPECT_FALSE(image.is_initialized());
    EXPECT_FALSE(image.get_image());
    EXPECT_FALSE(image.get_image_view());
    EXPECT_FALSE(image.get_memory());
    EXPECT_FALSE(image.get_sampler());
    EXPECT_EQ(image.get_current_layout(), vk::ImageLayout::eUndefined);
}

TEST(VKImageTest, LayoutTrackingIsStoredNotInferred)
{
    VKImage image(4, 4, 1, vk::Format::eR8G8B8A8Unorm, VKImage::Usage::TEXTURE_2D);
    image.set_current_layout(vk::ImageLayout::eShaderReadOnlyOptimal);
    EXPECT_EQ(image.get_current_layout(), vk::ImageLayout::eShaderReadOnlyOptimal);
}

TEST(VKImageTest, SizeBytesForSingleLevelImage)
{
    VKImage image(4, 2, 1, vk::Format::eR8G8B8A8Unorm, VKImage::Usage::TEXTURE_2D);
    EXPECT_EQ(image.get_size_bytes(), 4U * 2U * 4U);
}

TEST(VKImageTest, SizeBytesSumsHalvingMipChain)
{
    VKImage image(4, 4, 1, vk::Format::eR8G8B8A8Unorm, VKImage::Usage::TEXTURE_2D,
        VKImage::Type::TYPE_2D, 3);
    EXPECT_EQ(image.get_size_bytes(), (16U + 4U + 1U) * 4U);
}

TEST(VKImageTest, SizeBytesClampsMipExtentAtOne)
{
    VKImage image(2, 2, 1, vk::Format::eR8G8B8A8Unorm, VKImage::Usage::TEXTURE_2D,
        VKImage::Type::TYPE_2D, 4);
    EXPECT_EQ(image.get_size_bytes(), (4U + 1U + 1U + 1U) * 4U);
}

TEST(VKImageTest, SizeBytesMultipliesByArrayLayers)
{
    VKImage image(4, 4, 1, vk::Format::eR8G8B8A8Unorm, VKImage::Usage::TEXTURE_2D,
        VKImage::Type::TYPE_2D, 1, 3);
    EXPECT_EQ(image.get_size_bytes(), 4U * 4U * 4U * 3U);
}

TEST(VKImageTest, SizeBytesCountsDepthOfVolumes)
{
    VKImage image(2, 2, 2, vk::Format::eR8G8B8A8Unorm, VKImage::Usage::TEXTURE_2D,
        VKImage::Type::TYPE_3D);
    EXPECT_EQ(image.get_size_bytes(), 2U * 2U * 2U * 4U);
}

TEST(VKImageTest, SizeBytesUsesFormatTexelSize)
{
    VKImage image(4, 4, 1, vk::Format::eR32G32B32A32Sfloat, VKImage::Usage::STORAGE);
    EXPECT_EQ(image.get_size_bytes(), 4U * 4U * 16U);
}

TEST_P(VKImageUsageFlagsTest, UsageSelectsVulkanImageUsage)
{
    const auto& param = GetParam();
    VKImage image(4, 4, 1, vk::Format::eR8G8B8A8Unorm, param.usage);
    const auto flags = image.get_usage_flags();

    EXPECT_TRUE(has_flag(flags, vk::ImageUsageFlagBits::eTransferSrc));
    EXPECT_TRUE(has_flag(flags, vk::ImageUsageFlagBits::eTransferDst));
    EXPECT_EQ(flags & param.expected, param.expected);
    EXPECT_EQ(flags & param.forbidden, vk::ImageUsageFlags {});
}

INSTANTIATE_TEST_SUITE_P(Usages, VKImageUsageFlagsTest,
    ::testing::Values(
        UsageFlags { VKImage::Usage::TEXTURE_2D, vk::ImageUsageFlagBits::eSampled,
            vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eStorage },
        UsageFlags { VKImage::Usage::RENDER_TARGET,
            vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
            vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eDepthStencilAttachment },
        UsageFlags { VKImage::Usage::DEPTH_STENCIL,
            vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
            vk::ImageUsageFlagBits::eColorAttachment },
        UsageFlags { VKImage::Usage::STORAGE,
            vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled,
            vk::ImageUsageFlagBits::eColorAttachment },
        UsageFlags { VKImage::Usage::TRANSFER_SRC, {},
            vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage },
        UsageFlags { VKImage::Usage::TRANSFER_DST, {},
            vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage },
        UsageFlags { VKImage::Usage::STAGING, {},
            vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage }));

TEST(VKImageTest, OnlyStagingImagesAreHostVisible)
{
    for (auto usage : { VKImage::Usage::TEXTURE_2D, VKImage::Usage::RENDER_TARGET,
             VKImage::Usage::DEPTH_STENCIL, VKImage::Usage::STORAGE,
             VKImage::Usage::TRANSFER_SRC, VKImage::Usage::TRANSFER_DST }) {
        VKImage image(4, 4, 1, vk::Format::eR8G8B8A8Unorm, usage);
        EXPECT_FALSE(image.is_host_visible());
        EXPECT_EQ(image.get_memory_properties(), vk::MemoryPropertyFlagBits::eDeviceLocal);
    }

    VKImage staging(4, 4, 1, vk::Format::eR8G8B8A8Unorm, VKImage::Usage::STAGING);
    EXPECT_TRUE(staging.is_host_visible());
    EXPECT_EQ(staging.get_memory_properties(),
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
}

TEST(VKImageTest, ColorUsagesUseColorAspect)
{
    VKImage image(4, 4, 1, vk::Format::eR8G8B8A8Unorm, VKImage::Usage::RENDER_TARGET);
    EXPECT_EQ(image.get_aspect_flags(), vk::ImageAspectFlagBits::eColor);
}

TEST(VKImageTest, DepthFormatOnNonDepthUsageStillUsesColorAspect)
{
    VKImage image(4, 4, 1, vk::Format::eD32Sfloat, VKImage::Usage::TEXTURE_2D);
    EXPECT_EQ(image.get_aspect_flags(), vk::ImageAspectFlagBits::eColor);
}

TEST(VKImageTest, DepthOnlyFormatsUseDepthAspect)
{
    for (auto format : { vk::Format::eD16Unorm, vk::Format::eD32Sfloat, vk::Format::eX8D24UnormPack32 }) {
        VKImage image(4, 4, 1, format, VKImage::Usage::DEPTH_STENCIL);
        EXPECT_EQ(image.get_aspect_flags(), vk::ImageAspectFlagBits::eDepth) << vk::to_string(format);
    }
}

TEST(VKImageTest, CombinedDepthStencilFormatsUseBothAspects)
{
    for (auto format : { vk::Format::eD16UnormS8Uint, vk::Format::eD24UnormS8Uint, vk::Format::eD32SfloatS8Uint }) {
        VKImage image(4, 4, 1, format, VKImage::Usage::DEPTH_STENCIL);
        EXPECT_EQ(image.get_aspect_flags(),
            vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil)
            << vk::to_string(format);
    }
}

TEST(VKImageTest, StencilOnlyFormatUsesStencilAspect)
{
    VKImage image(4, 4, 1, vk::Format::eS8Uint, VKImage::Usage::DEPTH_STENCIL);
    EXPECT_EQ(image.get_aspect_flags(), vk::ImageAspectFlagBits::eStencil);
}

TEST(VKImageTest, DepthStencilUsageWithColorFormatFallsBackToDepthAspect)
{
    VKImage image(4, 4, 1, vk::Format::eR8G8B8A8Unorm, VKImage::Usage::DEPTH_STENCIL);
    EXPECT_EQ(image.get_aspect_flags(), vk::ImageAspectFlagBits::eDepth);
}

TEST_P(VKImageChannelDimensionTest, FormatDeterminesChannelDimension)
{
    const auto [format, channels] = GetParam();
    VKImage image(4, 4, 1, format, VKImage::Usage::TEXTURE_2D);

    const auto* channel = find_role(image, DataDimension::Role::CHANNEL);
    ASSERT_NE(channel, nullptr) << vk::to_string(format);
    EXPECT_EQ(channel->size, channels) << vk::to_string(format);
}

INSTANTIATE_TEST_SUITE_P(Formats, VKImageChannelDimensionTest,
    ::testing::Values(
        ChannelCount { vk::Format::eR8Unorm, 1 },
        ChannelCount { vk::Format::eR32Sfloat, 1 },
        ChannelCount { vk::Format::eR8G8Unorm, 2 },
        ChannelCount { vk::Format::eR32G32Sfloat, 2 },
        ChannelCount { vk::Format::eR8G8B8Unorm, 3 },
        ChannelCount { vk::Format::eB8G8R8Unorm, 3 },
        ChannelCount { vk::Format::eR8G8B8A8Unorm, 4 },
        ChannelCount { vk::Format::eR8G8B8A8Srgb, 4 },
        ChannelCount { vk::Format::eR32G32B32A32Sfloat, 4 },
        ChannelCount { vk::Format::eUndefined, 4 }));

TEST(VKImageTest, SpatialDimensionsFollowImageType)
{
    VKImage line(8, 1, 1, vk::Format::eR8Unorm, VKImage::Usage::TEXTURE_2D, VKImage::Type::TYPE_1D);
    EXPECT_NE(find_role(line, DataDimension::Role::SPATIAL_X), nullptr);
    EXPECT_EQ(find_role(line, DataDimension::Role::SPATIAL_Y), nullptr);

    VKImage plane(8, 4, 1, vk::Format::eR8Unorm, VKImage::Usage::TEXTURE_2D, VKImage::Type::TYPE_2D);
    EXPECT_NE(find_role(plane, DataDimension::Role::SPATIAL_Y), nullptr);
    EXPECT_EQ(find_role(plane, DataDimension::Role::SPATIAL_Z), nullptr);

    VKImage volume(8, 4, 2, vk::Format::eR8Unorm, VKImage::Usage::TEXTURE_2D, VKImage::Type::TYPE_3D);
    EXPECT_NE(find_role(volume, DataDimension::Role::SPATIAL_Z), nullptr);
}

TEST(VKImageTest, MipDimensionPresentOnlyAboveOneLevel)
{
    VKImage single(4, 4, 1, vk::Format::eR8G8B8A8Unorm, VKImage::Usage::TEXTURE_2D);
    EXPECT_EQ(find_role(single, DataDimension::Role::MIP_LEVEL), nullptr);

    VKImage chain(4, 4, 1, vk::Format::eR8G8B8A8Unorm, VKImage::Usage::TEXTURE_2D,
        VKImage::Type::TYPE_2D, 3);
    const auto* mips = find_role(chain, DataDimension::Role::MIP_LEVEL);
    ASSERT_NE(mips, nullptr);
    EXPECT_EQ(mips->size, 3U);
}

TEST(VKImageTest, SetModalityKeepsDimensionsConsistent)
{
    VKImage image(4, 4, 1, vk::Format::eR8G8B8A8Unorm, VKImage::Usage::TEXTURE_2D);
    const auto before = image.get_dimensions().size();

    image.set_modality(Kakshya::DataModality::IMAGE_COLOR);

    EXPECT_EQ(image.get_modality(), Kakshya::DataModality::IMAGE_COLOR);
    EXPECT_EQ(image.get_dimensions().size(), before);
}

TEST(VKImageTest, MoveKeepsDescriptionAndResources)
{
    VKImage image(4, 2, 1, vk::Format::eR8G8B8A8Unorm, VKImage::Usage::STORAGE);
    image.set_current_layout(vk::ImageLayout::eGeneral);

    VKImage moved(std::move(image));

    EXPECT_EQ(moved.get_width(), 4U);
    EXPECT_EQ(moved.get_height(), 2U);
    EXPECT_EQ(moved.get_usage(), VKImage::Usage::STORAGE);
    EXPECT_EQ(moved.get_current_layout(), vk::ImageLayout::eGeneral);
    EXPECT_EQ(moved.get_size_bytes(), 4U * 2U * 4U);
}

}
