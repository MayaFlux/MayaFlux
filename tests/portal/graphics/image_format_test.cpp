#include "gtest/gtest.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "MayaFlux/Portal/Graphics/TextureLoom.hpp"

namespace MayaFlux::Test {
namespace {

    using Kakshya::ImageFormat;
    using Portal::Graphics::TextureLoom;

    struct FormatCase {
        std::string_view name;
        ImageFormat format;
        vk::Format vulkan;
        size_t bytes_per_pixel;
        uint32_t channels;
    };

    std::vector<FormatCase> format_cases()
    {
        return {
            { "R8", ImageFormat::R8, vk::Format::eR8Unorm, 1, 1 },
            { "RG8", ImageFormat::RG8, vk::Format::eR8G8Unorm, 2, 2 },
            { "RGB8", ImageFormat::RGB8, vk::Format::eR8G8B8Unorm, 3, 3 },
            { "RGBA8", ImageFormat::RGBA8, vk::Format::eR8G8B8A8Unorm, 4, 4 },
            { "RGBA8_SRGB", ImageFormat::RGBA8_SRGB, vk::Format::eR8G8B8A8Srgb, 4, 4 },
            { "BGRA8", ImageFormat::BGRA8, vk::Format::eB8G8R8A8Unorm, 4, 4 },
            { "BGRA8_SRGB", ImageFormat::BGRA8_SRGB, vk::Format::eB8G8R8A8Srgb, 4, 4 },
            { "R16", ImageFormat::R16, vk::Format::eR16Unorm, 2, 1 },
            { "RG16", ImageFormat::RG16, vk::Format::eR16G16Unorm, 4, 2 },
            { "RGBA16", ImageFormat::RGBA16, vk::Format::eR16G16B16A16Unorm, 8, 4 },
            { "R16F", ImageFormat::R16F, vk::Format::eR16Sfloat, 2, 1 },
            { "RG16F", ImageFormat::RG16F, vk::Format::eR16G16Sfloat, 4, 2 },
            { "RGBA16F", ImageFormat::RGBA16F, vk::Format::eR16G16B16A16Sfloat, 8, 4 },
            { "R32F", ImageFormat::R32F, vk::Format::eR32Sfloat, 4, 1 },
            { "RG32F", ImageFormat::RG32F, vk::Format::eR32G32Sfloat, 8, 2 },
            { "RGBA32F", ImageFormat::RGBA32F, vk::Format::eR32G32B32A32Sfloat, 16, 4 },
            { "DEPTH16", ImageFormat::DEPTH16, vk::Format::eD16Unorm, 2, 1 },
            { "DEPTH24", ImageFormat::DEPTH24, vk::Format::eX8D24UnormPack32, 4, 1 },
            { "DEPTH32F", ImageFormat::DEPTH32F, vk::Format::eD32Sfloat, 4, 1 },
            { "DEPTH24_STENCIL8", ImageFormat::DEPTH24_STENCIL8, vk::Format::eD24UnormS8Uint, 4, 2 }
        };
    }

    std::string format_name(const ::testing::TestParamInfo<FormatCase>& info)
    {
        return std::string(info.param.name);
    }

    class ImageFormatTableTest : public ::testing::TestWithParam<FormatCase> { };

    TEST_P(ImageFormatTableTest, MapsToItsVulkanFormat)
    {
        EXPECT_EQ(TextureLoom::to_vulkan_format(GetParam().format), GetParam().vulkan);
    }

    TEST_P(ImageFormatTableTest, RoundTripsThroughTheVulkanFormat)
    {
        const auto back = TextureLoom::from_vulkan_format(TextureLoom::to_vulkan_format(GetParam().format));

        ASSERT_TRUE(back.has_value());
        EXPECT_EQ(*back, GetParam().format);
    }

    TEST_P(ImageFormatTableTest, BytesPerPixelMatchTheVulkanFormat)
    {
        EXPECT_EQ(TextureLoom::get_bytes_per_pixel(GetParam().format), GetParam().bytes_per_pixel);
    }

    TEST_P(ImageFormatTableTest, ChannelCountMatchesTheFormat)
    {
        EXPECT_EQ(TextureLoom::get_channel_count(GetParam().format), GetParam().channels);
    }

    TEST_P(ImageFormatTableTest, ImageSizeIsWidthTimesHeightTimesDepthTimesBytesPerPixel)
    {
        EXPECT_EQ(TextureLoom::calculate_image_size(7, 5, 3, GetParam().format),
            static_cast<size_t>(7) * 5 * 3 * GetParam().bytes_per_pixel);
    }

    INSTANTIATE_TEST_SUITE_P(Formats, ImageFormatTableTest,
        ::testing::ValuesIn(format_cases()), format_name);

    TEST(ImageFormatTest, ImageSizeOfAnEmptyExtentIsZero)
    {
        EXPECT_EQ(TextureLoom::calculate_image_size(0, 16, 1, ImageFormat::RGBA8), 0U);
        EXPECT_EQ(TextureLoom::calculate_image_size(16, 0, 1, ImageFormat::RGBA8), 0U);
        EXPECT_EQ(TextureLoom::calculate_image_size(16, 16, 0, ImageFormat::RGBA8), 0U);
    }

    TEST(ImageFormatTest, ImageSizeDoesNotOverflowThirtyTwoBitsForLargeImages)
    {
        EXPECT_EQ(TextureLoom::calculate_image_size(65536, 65536, 1, ImageFormat::RGBA8),
            static_cast<size_t>(65536) * 65536 * 4);
    }

    TEST(ImageFormatTest, SrgbFormatsAllocateByteStorageThatIsConsistent)
    {
        for (const ImageFormat format : { ImageFormat::RGBA8_SRGB, ImageFormat::BGRA8_SRGB }) {
            const auto data = Kakshya::ImageData::allocate(2, 2, 4, format);

            ASSERT_TRUE(data.has_value()) << static_cast<int>(format);
            EXPECT_EQ(data->byte_size(), 16U);
            EXPECT_TRUE(data->is_consistent());
        }
    }

    TEST(ImageFormatTest, UnlistedVulkanFormatsHaveNoImageFormat)
    {
        EXPECT_FALSE(TextureLoom::from_vulkan_format(vk::Format::eUndefined).has_value());
        EXPECT_FALSE(TextureLoom::from_vulkan_format(vk::Format::eR64Sfloat).has_value());
        EXPECT_FALSE(TextureLoom::from_vulkan_format(vk::Format::eBc1RgbUnormBlock).has_value());
    }

    TEST(ImageFormatTest, ReadableAndWritableFormatsRejectAMissingImage)
    {
        EXPECT_FALSE(TextureLoom::readable_format(nullptr).has_value());
        EXPECT_FALSE(TextureLoom::writable_format(nullptr).has_value());
    }

} // namespace
} // namespace MayaFlux::Test
