#include "gtest/gtest.h"

#include <cstdint>
#include <cstring>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include "../../support/vulkan_test_support.hpp"

#include "MayaFlux/Buffers/VKBuffer.hpp"
#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKImage.hpp"
#include "MayaFlux/Portal/Graphics/TextureLoom.hpp"

namespace MayaFlux::Test {
namespace {

    using Core::VKImage;
    using Kakshya::ImageFormat;
    using Portal::Graphics::FilterMode;
    using Portal::Graphics::ImageCacheEntry;
    using Portal::Graphics::ImageCacheSet;
    using Portal::Graphics::ImageKey;
    using Portal::Graphics::LayerBlit;
    using Portal::Graphics::SamplerConfig;
    using Portal::Graphics::TextureLoom;

    using ImagePtr = std::shared_ptr<VKImage>;

    TextureLoom& loom()
    {
        return Portal::Graphics::get_texture_manager();
    }

    class TextureLoomTest : public ::testing::Test {
    protected:
        static void SetUpTestSuite()
        {
            if (!vulkan_unavailable_reason().empty()) {
                return;
            }
            auto backend = std::make_shared<Core::VulkanBackend>();
            if (backend->initialize(headless_graphics_config())) {
                s_backend = std::move(backend);
            }
        }

        static void TearDownTestSuite()
        {
            s_backend.reset();
        }

        void SetUp() override
        {
            if (!s_backend) {
                GTEST_SKIP() << "No usable Vulkan device: " << vulkan_unavailable_reason();
            }
            loom().shutdown();
        }

        void TearDown() override
        {
            loom().shutdown();
        }

        static const std::shared_ptr<Core::VulkanBackend>& backend() { return s_backend; }

        class Staging {
        public:
            explicit Staging(size_t bytes)
                : m_buffer(std::make_shared<Buffers::VKBuffer>(
                      bytes, Buffers::VKBuffer::Usage::STAGING, Kakshya::DataModality::IMAGE_COLOR))
            {
                backend()->get_resource_manager().initialize_buffer(m_buffer);
            }

            ~Staging()
            {
                backend()->get_resource_manager().cleanup_buffer(m_buffer);
            }

            Staging(const Staging&) = delete;
            Staging& operator=(const Staging&) = delete;
            Staging(Staging&&) = delete;
            Staging& operator=(Staging&&) = delete;

            [[nodiscard]] const std::shared_ptr<Buffers::VKBuffer>& buffer() const { return m_buffer; }

        private:
            std::shared_ptr<Buffers::VKBuffer> m_buffer;
        };

    private:
        static inline std::shared_ptr<Core::VulkanBackend> s_backend;
    };

    std::vector<uint32_t> rgba8_words(const ImagePtr& image)
    {
        const auto data = loom().download_image(image);
        if (!data) {
            return {};
        }
        std::vector<uint32_t> words(data->byte_size() / sizeof(uint32_t));
        std::memcpy(words.data(), data->data(), words.size() * sizeof(uint32_t));
        return words;
    }

    std::vector<uint8_t> pixel_pattern(ImageFormat format, size_t texels)
    {
        const size_t bytes_per_pixel = TextureLoom::get_bytes_per_pixel(format);
        const size_t channels = TextureLoom::get_channel_count(format);
        const size_t element = bytes_per_pixel / channels;
        const bool half = format == ImageFormat::R16F || format == ImageFormat::RG16F || format == ImageFormat::RGBA16F;

        std::vector<uint8_t> bytes(texels * bytes_per_pixel);
        const size_t elements = texels * channels;
        for (size_t i = 0; i < elements; ++i) {
            uint8_t* at = bytes.data() + i * element;
            if (element == 1) {
                *at = static_cast<uint8_t>((i * 37 + 11) & 0xFF);
            } else if (element == 2) {
                const auto value = static_cast<uint16_t>(half ? 0x3C00 + (i % 256) : i * 257 + 1);
                std::memcpy(at, &value, sizeof(value));
            } else {
                const float value = 0.25F * static_cast<float>(i + 1);
                std::memcpy(at, &value, sizeof(value));
            }
        }
        return bytes;
    }

    // ------------------------------------------------------------------
    // Lifecycle
    // ------------------------------------------------------------------

    TEST_F(TextureLoomTest, UninitializedLoomCreatesNothing)
    {
        EXPECT_FALSE(loom().is_initialized());
        EXPECT_EQ(loom().create_2d(4, 4), nullptr);
        EXPECT_EQ(loom().create_2d_uninitialized(4, 4), nullptr);
        EXPECT_EQ(loom().create_3d(4, 4, 4), nullptr);
        EXPECT_EQ(loom().create_cubemap(4), nullptr);
        EXPECT_EQ(loom().create_2d_array(4, 4, 2), nullptr);
        EXPECT_EQ(loom().create_render_target(4, 4), nullptr);
        EXPECT_EQ(loom().create_depth_buffer(4, 4), nullptr);
        EXPECT_EQ(loom().create_storage_image(4, 4), nullptr);
        EXPECT_FALSE(loom().get_default_sampler());
        EXPECT_FALSE(loom().get_nearest_sampler());
    }

    TEST_F(TextureLoomTest, InitializeRejectsANullBackend)
    {
        EXPECT_FALSE(loom().initialize(nullptr));
        EXPECT_FALSE(loom().is_initialized());
    }

    TEST_F(TextureLoomTest, InitializingAgainReportsSuccessAndKeepsTheLoomUsable)
    {
        ASSERT_TRUE(loom().initialize(backend()));

        EXPECT_TRUE(loom().initialize(backend()));

        EXPECT_TRUE(loom().is_initialized());
        EXPECT_NE(loom().create_2d(4, 4), nullptr);
    }

    TEST_F(TextureLoomTest, ShutdownReleasesEveryImageTheLoomCreated)
    {
        ASSERT_TRUE(loom().initialize(backend()));
        const ImagePtr first = loom().create_2d(4, 4);
        const ImagePtr second = loom().create_storage_image(8, 8);
        ASSERT_TRUE(first && first->is_initialized());
        ASSERT_TRUE(second && second->is_initialized());

        loom().shutdown();

        EXPECT_FALSE(loom().is_initialized());
        EXPECT_FALSE(first->is_initialized());
        EXPECT_FALSE(second->is_initialized());
    }

    TEST_F(TextureLoomTest, ShutdownIsRepeatable)
    {
        ASSERT_TRUE(loom().initialize(backend()));

        loom().shutdown();

        EXPECT_NO_THROW(loom().shutdown());
    }

    TEST_F(TextureLoomTest, LoomCreatesImagesAgainAfterShutdownAndReinitialize)
    {
        ASSERT_TRUE(loom().initialize(backend()));
        ASSERT_NE(loom().create_2d(4, 4), nullptr);
        loom().shutdown();

        ASSERT_TRUE(loom().initialize(backend()));

        const ImagePtr image = loom().create_2d(4, 4);
        ASSERT_NE(image, nullptr);
        EXPECT_TRUE(image->is_initialized());
    }

    // ------------------------------------------------------------------
    // Creation
    // ------------------------------------------------------------------

    TEST_F(TextureLoomTest, Create2dWithoutDataIsASampledImageReadyForShaders)
    {
        ASSERT_TRUE(loom().initialize(backend()));

        const ImagePtr image = loom().create_2d(16, 8, ImageFormat::RGBA8);

        ASSERT_NE(image, nullptr);
        EXPECT_TRUE(image->is_initialized());
        EXPECT_EQ(image->get_width(), 16U);
        EXPECT_EQ(image->get_height(), 8U);
        EXPECT_EQ(image->get_array_layers(), 1U);
        EXPECT_EQ(image->get_mip_levels(), 1U);
        EXPECT_EQ(image->get_format(), TextureLoom::to_vulkan_format(ImageFormat::RGBA8));
        EXPECT_EQ(image->get_type(), VKImage::Type::TYPE_2D);
        EXPECT_EQ(image->get_current_layout(), vk::ImageLayout::eShaderReadOnlyOptimal);
    }

    TEST_F(TextureLoomTest, Create2dRecordsTheRequestedMipLevels)
    {
        ASSERT_TRUE(loom().initialize(backend()));

        const ImagePtr image = loom().create_2d(64, 64, ImageFormat::RGBA8, nullptr, 4);

        ASSERT_NE(image, nullptr);
        EXPECT_EQ(image->get_mip_levels(), 4U);
    }

    TEST_F(TextureLoomTest, Create2dUninitializedAllocatesWithoutAnUpload)
    {
        ASSERT_TRUE(loom().initialize(backend()));

        const ImagePtr image = loom().create_2d_uninitialized(32, 16, ImageFormat::R32F);

        ASSERT_NE(image, nullptr);
        EXPECT_TRUE(image->is_initialized());
        EXPECT_EQ(image->get_width(), 32U);
        EXPECT_EQ(image->get_height(), 16U);
        EXPECT_EQ(image->get_format(), vk::Format::eR32Sfloat);
    }

    TEST_F(TextureLoomTest, Create3dRecordsTheDepthAndKind)
    {
        ASSERT_TRUE(loom().initialize(backend()));

        const ImagePtr image = loom().create_3d(8, 4, 2, ImageFormat::RGBA8);

        ASSERT_NE(image, nullptr);
        EXPECT_EQ(image->get_type(), VKImage::Type::TYPE_3D);
        EXPECT_EQ(image->get_width(), 8U);
        EXPECT_EQ(image->get_height(), 4U);
        EXPECT_EQ(image->get_depth(), 2U);
        EXPECT_EQ(image->get_current_layout(), vk::ImageLayout::eShaderReadOnlyOptimal);
    }

    TEST_F(TextureLoomTest, CubemapIsSquareWithSixFaces)
    {
        ASSERT_TRUE(loom().initialize(backend()));

        const ImagePtr image = loom().create_cubemap(16, ImageFormat::RGBA8);

        ASSERT_NE(image, nullptr);
        EXPECT_EQ(image->get_type(), VKImage::Type::TYPE_CUBE);
        EXPECT_EQ(image->get_width(), 16U);
        EXPECT_EQ(image->get_height(), 16U);
        EXPECT_EQ(image->get_array_layers(), 6U);
    }

    TEST_F(TextureLoomTest, Create2dArrayRecordsTheLayerCount)
    {
        ASSERT_TRUE(loom().initialize(backend()));

        const ImagePtr image = loom().create_2d_array(8, 8, 5, ImageFormat::RGBA8);

        ASSERT_NE(image, nullptr);
        EXPECT_EQ(image->get_array_layers(), 5U);
        EXPECT_EQ(image->get_width(), 8U);
        EXPECT_EQ(image->get_current_layout(), vk::ImageLayout::eShaderReadOnlyOptimal);
    }

    TEST_F(TextureLoomTest, Create2dArrayOfZeroLayersIsRefused)
    {
        ASSERT_TRUE(loom().initialize(backend()));

        EXPECT_EQ(loom().create_2d_array(8, 8, 0), nullptr);
    }

    TEST_F(TextureLoomTest, RenderTargetIsLeftReadyAsAColorAttachment)
    {
        ASSERT_TRUE(loom().initialize(backend()));

        const ImagePtr image = loom().create_render_target(32, 32, ImageFormat::RGBA8);

        ASSERT_NE(image, nullptr);
        EXPECT_EQ(image->get_usage(), VKImage::Usage::RENDER_TARGET);
        EXPECT_EQ(image->get_current_layout(), vk::ImageLayout::eColorAttachmentOptimal);
    }

    TEST_F(TextureLoomTest, StorageImageIsLeftInTheGeneralLayout)
    {
        ASSERT_TRUE(loom().initialize(backend()));

        const ImagePtr image = loom().create_storage_image(32, 32, ImageFormat::RGBA32F);

        ASSERT_NE(image, nullptr);
        EXPECT_EQ(image->get_usage(), VKImage::Usage::STORAGE);
        EXPECT_EQ(image->get_format(), vk::Format::eR32G32B32A32Sfloat);
        EXPECT_EQ(image->get_current_layout(), vk::ImageLayout::eGeneral);
    }

    TEST_F(TextureLoomTest, DepthBufferUsesA32BitFloatDepthFormatAndTheDepthAttachmentLayout)
    {
        ASSERT_TRUE(loom().initialize(backend()));

        const ImagePtr image = loom().create_depth_buffer(64, 64, false);

        ASSERT_NE(image, nullptr);
        EXPECT_EQ(image->get_usage(), VKImage::Usage::DEPTH_STENCIL);
        EXPECT_EQ(image->get_format(), vk::Format::eD32Sfloat);
        EXPECT_EQ(image->get_current_layout(), vk::ImageLayout::eDepthStencilAttachmentOptimal);
    }

    TEST_F(TextureLoomTest, DepthStencilBufferUsesA24BitDepthFormatWhereTheDeviceHasIt)
    {
        const auto features = backend()->get_context().get_physical_device()
                                  .getFormatProperties(vk::Format::eD24UnormS8Uint)
                                  .optimalTilingFeatures;
        if (!(features & vk::FormatFeatureFlagBits::eDepthStencilAttachment)) {
            GTEST_SKIP() << "Device has no D24S8 depth attachment format";
        }
        ASSERT_TRUE(loom().initialize(backend()));

        const ImagePtr image = loom().create_depth_buffer(64, 64, true);

        ASSERT_NE(image, nullptr);
        EXPECT_EQ(image->get_format(), vk::Format::eD24UnormS8Uint);
        EXPECT_EQ(image->get_aspect_flags(),
            vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil);
    }

    struct SampledFormat {
        std::string_view name;
        ImageFormat format;
    };

    class TextureLoomSampledFormatTest : public TextureLoomTest, public ::testing::WithParamInterface<SampledFormat> { };

    TEST_P(TextureLoomSampledFormatTest, SampledImageIsCreatedInTheFormatsVulkanFormat)
    {
        ASSERT_TRUE(loom().initialize(backend()));

        const ImagePtr image = loom().create_2d(8, 8, GetParam().format);

        ASSERT_NE(image, nullptr);
        EXPECT_TRUE(image->is_initialized());
        EXPECT_EQ(image->get_format(), TextureLoom::to_vulkan_format(GetParam().format));
    }

    INSTANTIATE_TEST_SUITE_P(Formats, TextureLoomSampledFormatTest,
        ::testing::Values(
            SampledFormat { "R8", ImageFormat::R8 },
            SampledFormat { "RG8", ImageFormat::RG8 },
            SampledFormat { "RGBA8", ImageFormat::RGBA8 },
            SampledFormat { "BGRA8", ImageFormat::BGRA8 },
            SampledFormat { "RGBA8_SRGB", ImageFormat::RGBA8_SRGB },
            SampledFormat { "BGRA8_SRGB", ImageFormat::BGRA8_SRGB },
            SampledFormat { "R16", ImageFormat::R16 },
            SampledFormat { "RG16", ImageFormat::RG16 },
            SampledFormat { "RGBA16", ImageFormat::RGBA16 },
            SampledFormat { "R16F", ImageFormat::R16F },
            SampledFormat { "RG16F", ImageFormat::RG16F },
            SampledFormat { "RGBA16F", ImageFormat::RGBA16F },
            SampledFormat { "R32F", ImageFormat::R32F },
            SampledFormat { "RG32F", ImageFormat::RG32F },
            SampledFormat { "RGBA32F", ImageFormat::RGBA32F }),
        [](const ::testing::TestParamInfo<SampledFormat>& info) { return std::string(info.param.name); });

    // ------------------------------------------------------------------
    // Upload and download
    // ------------------------------------------------------------------

    class TextureLoomRoundTripTest : public TextureLoomTest, public ::testing::WithParamInterface<SampledFormat> { };

    TEST_P(TextureLoomRoundTripTest, UploadedPixelsComeBackUnchanged)
    {
        ASSERT_TRUE(loom().initialize(backend()));
        constexpr uint32_t width = 4;
        constexpr uint32_t height = 3;
        const auto pixels = pixel_pattern(GetParam().format, static_cast<size_t>(width) * height);

        const ImagePtr image = loom().create_2d(width, height, GetParam().format, pixels.data());
        ASSERT_NE(image, nullptr);
        const auto downloaded = loom().download_image(image);

        ASSERT_TRUE(downloaded.has_value());
        ASSERT_EQ(downloaded->byte_size(), pixels.size());
        EXPECT_EQ(std::memcmp(downloaded->data(), pixels.data(), pixels.size()), 0);
    }

    INSTANTIATE_TEST_SUITE_P(Formats, TextureLoomRoundTripTest,
        ::testing::Values(
            SampledFormat { "R8", ImageFormat::R8 },
            SampledFormat { "RG8", ImageFormat::RG8 },
            SampledFormat { "RGBA8", ImageFormat::RGBA8 },
            SampledFormat { "BGRA8", ImageFormat::BGRA8 },
            SampledFormat { "RGBA8_SRGB", ImageFormat::RGBA8_SRGB },
            SampledFormat { "BGRA8_SRGB", ImageFormat::BGRA8_SRGB },
            SampledFormat { "R16", ImageFormat::R16 },
            SampledFormat { "RG16", ImageFormat::RG16 },
            SampledFormat { "RGBA16", ImageFormat::RGBA16 },
            SampledFormat { "R16F", ImageFormat::R16F },
            SampledFormat { "RG16F", ImageFormat::RG16F },
            SampledFormat { "RGBA16F", ImageFormat::RGBA16F },
            SampledFormat { "R32F", ImageFormat::R32F },
            SampledFormat { "RG32F", ImageFormat::RG32F },
            SampledFormat { "RGBA32F", ImageFormat::RGBA32F }),
        [](const ::testing::TestParamInfo<SampledFormat>& info) { return std::string(info.param.name); });

    TEST_F(TextureLoomTest, UploadDataFillsAnImageCreatedWithoutPixels)
    {
        ASSERT_TRUE(loom().initialize(backend()));
        const std::vector<uint32_t> pixels { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 };
        const ImagePtr image = loom().create_2d_uninitialized(4, 3, ImageFormat::RGBA8);
        ASSERT_NE(image, nullptr);

        loom().upload_data(image, pixels.data(), pixels.size() * sizeof(uint32_t));

        EXPECT_EQ(rgba8_words(image), pixels);
    }

    TEST_F(TextureLoomTest, UploadAndDownloadThroughACallerStagingBufferRoundTrip)
    {
        ASSERT_TRUE(loom().initialize(backend()));
        const std::vector<uint32_t> pixels { 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 };
        const size_t bytes = pixels.size() * sizeof(uint32_t);
        Staging staging(bytes);
        const ImagePtr image = loom().create_2d_uninitialized(4, 3, ImageFormat::RGBA8);
        ASSERT_NE(image, nullptr);

        loom().upload_data(image, pixels.data(), bytes, staging.buffer());
        const auto downloaded = loom().download_image(image, staging.buffer());

        ASSERT_TRUE(downloaded.has_value());
        ASSERT_EQ(downloaded->byte_size(), bytes);
        EXPECT_EQ(std::memcmp(downloaded->data(), pixels.data(), bytes), 0);
    }

    TEST_F(TextureLoomTest, DownloadDataFillsACallerBuffer)
    {
        ASSERT_TRUE(loom().initialize(backend()));
        const std::vector<uint32_t> pixels { 41, 42, 43, 44 };
        const size_t bytes = pixels.size() * sizeof(uint32_t);
        Staging staging(bytes);
        const ImagePtr image = loom().create_2d(2, 2, ImageFormat::RGBA8, pixels.data());
        ASSERT_NE(image, nullptr);
        std::vector<uint32_t> out(pixels.size(), 0);

        loom().download_data(image, out.data(), bytes, staging.buffer());

        EXPECT_EQ(out, pixels);
    }

    TEST_F(TextureLoomTest, InvalidUploadArgumentsAreIgnored)
    {
        ASSERT_TRUE(loom().initialize(backend()));
        const std::vector<uint32_t> pixels { 1, 2, 3, 4 };
        const ImagePtr image = loom().create_2d_uninitialized(2, 2, ImageFormat::RGBA8);

        EXPECT_NO_THROW(loom().upload_data(nullptr, pixels.data(), 16));
        EXPECT_NO_THROW(loom().upload_data(image, nullptr, 16));
        EXPECT_NO_THROW(loom().upload_data(image, pixels.data(), 16, nullptr));
    }

    TEST_F(TextureLoomTest, DownloadImageRefusesImagesThatAreNotASingleLayer2dImage)
    {
        ASSERT_TRUE(loom().initialize(backend()));

        EXPECT_FALSE(loom().download_image(loom().create_2d_array(4, 4, 2)).has_value());
        EXPECT_FALSE(loom().download_image(loom().create_cubemap(4)).has_value());
        EXPECT_FALSE(loom().download_image(loom().create_3d(4, 4, 4)).has_value());
        EXPECT_FALSE(loom().download_image(nullptr).has_value());
    }

    TEST_F(TextureLoomTest, DownloadImageIsRefusedWhileTheLoomIsNotInitialized)
    {
        EXPECT_FALSE(loom().download_image(nullptr).has_value());
    }

    // ------------------------------------------------------------------
    // Typed pixel data
    // ------------------------------------------------------------------

    TEST_F(TextureLoomTest, Create2dFromAVec4VariantUploadsFloatTexels)
    {
        ASSERT_TRUE(loom().initialize(backend()));
        const std::vector<glm::vec4> texels {
            { 0.0F, 0.25F, 0.5F, 1.0F }, { 1.0F, 0.75F, 0.5F, 0.25F },
            { 0.1F, 0.2F, 0.3F, 0.4F }, { 0.9F, 0.8F, 0.7F, 0.6F }
        };

        const ImagePtr image = loom().create_2d(Kakshya::DataVariant(texels), 2, 2);

        ASSERT_NE(image, nullptr);
        EXPECT_EQ(image->get_format(), vk::Format::eR32G32B32A32Sfloat);
        const auto downloaded = loom().download_image(image);
        ASSERT_TRUE(downloaded.has_value());
        ASSERT_EQ(downloaded->byte_size(), texels.size() * sizeof(glm::vec4));
        EXPECT_EQ(std::memcmp(downloaded->data(), texels.data(), downloaded->byte_size()), 0);
    }

    TEST_F(TextureLoomTest, Create2dFromAVariantOfTheWrongSizeThrows)
    {
        ASSERT_TRUE(loom().initialize(backend()));
        const std::vector<glm::vec4> texels(4, glm::vec4(1.0F));

        EXPECT_THROW(static_cast<void>(loom().create_2d(Kakshya::DataVariant(texels), 3, 3)), std::invalid_argument);
    }

    TEST_F(TextureLoomTest, Create2dFromAnEmptyVariantReturnsNothing)
    {
        ASSERT_TRUE(loom().initialize(backend()));

        EXPECT_EQ(loom().create_2d(Kakshya::DataVariant(std::vector<glm::vec4> {}), 2, 2), nullptr);
    }

    // ------------------------------------------------------------------
    // Layout, format queries
    // ------------------------------------------------------------------

    TEST_F(TextureLoomTest, TransitionLayoutRecordsTheNewLayoutOnTheImage)
    {
        ASSERT_TRUE(loom().initialize(backend()));
        const ImagePtr image = loom().create_2d(4, 4);
        ASSERT_NE(image, nullptr);

        loom().transition_layout(image, image->get_current_layout(), vk::ImageLayout::eGeneral);

        EXPECT_EQ(image->get_current_layout(), vk::ImageLayout::eGeneral);
    }

    TEST_F(TextureLoomTest, TransitionLayoutOfAMissingImageIsIgnored)
    {
        ASSERT_TRUE(loom().initialize(backend()));

        EXPECT_NO_THROW(loom().transition_layout(nullptr, vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral));
    }

    TEST_F(TextureLoomTest, SampledImagesAreReadableAndWritableInTheirOwnFormat)
    {
        ASSERT_TRUE(loom().initialize(backend()));
        const ImagePtr image = loom().create_2d(8, 8, ImageFormat::RG16F);
        ASSERT_NE(image, nullptr);

        EXPECT_EQ(TextureLoom::readable_format(image), ImageFormat::RG16F);
        EXPECT_EQ(TextureLoom::writable_format(image), ImageFormat::RG16F);
    }

    TEST_F(TextureLoomTest, DepthImageIsReadableInItsDepthFormat)
    {
        ASSERT_TRUE(loom().initialize(backend()));
        const ImagePtr image = loom().create_depth_buffer(8, 8, false);
        ASSERT_NE(image, nullptr);

        EXPECT_EQ(TextureLoom::readable_format(image), ImageFormat::DEPTH32F);
    }

    TEST_F(TextureLoomTest, CubemapArrayAnd3dImagesHaveNoTransferFormat)
    {
        ASSERT_TRUE(loom().initialize(backend()));

        EXPECT_FALSE(TextureLoom::readable_format(loom().create_cubemap(4)).has_value());
        EXPECT_FALSE(TextureLoom::readable_format(loom().create_2d_array(4, 4, 2)).has_value());
        EXPECT_FALSE(TextureLoom::writable_format(loom().create_3d(4, 4, 4)).has_value());
    }

    // ------------------------------------------------------------------
    // Layers and blits
    // ------------------------------------------------------------------

    vk::Rect2D rect(int32_t x, int32_t y, uint32_t w, uint32_t h)
    {
        return vk::Rect2D { vk::Offset2D { x, y }, vk::Extent2D { w, h } };
    }

    TEST_F(TextureLoomTest, UploadLayerWritesOneLayerOfAnArrayThatBlitsBackOut)
    {
        ASSERT_TRUE(loom().initialize(backend()));
        const ImagePtr array = loom().create_2d_array(2, 2, 3, ImageFormat::RGBA8);
        const ImagePtr flat = loom().create_2d(2, 2, ImageFormat::RGBA8);
        ASSERT_NE(array, nullptr);
        ASSERT_NE(flat, nullptr);
        if (!loom().can_blit(array, flat, FilterMode::NEAREST)) {
            GTEST_SKIP() << "Device cannot blit RGBA8";
        }
        const std::vector<uint32_t> layer { 101, 102, 103, 104 };

        ASSERT_TRUE(loom().upload_layer(array, 1, layer.data(), layer.size() * sizeof(uint32_t)));
        ASSERT_TRUE(loom().blit_layer(array, flat,
            LayerBlit { .src_layer = 1, .dst_layer = 0, .src = rect(0, 0, 2, 2), .dst = rect(0, 0, 2, 2), .filter = FilterMode::NEAREST }));

        EXPECT_EQ(rgba8_words(flat), layer);
    }

    TEST_F(TextureLoomTest, UploadLayerRejectsABadLayerSizeOrMissingArguments)
    {
        ASSERT_TRUE(loom().initialize(backend()));
        const ImagePtr array = loom().create_2d_array(2, 2, 2, ImageFormat::RGBA8);
        ASSERT_NE(array, nullptr);
        const std::vector<uint32_t> layer { 1, 2, 3, 4 };
        const size_t bytes = layer.size() * sizeof(uint32_t);

        EXPECT_FALSE(loom().upload_layer(array, 2, layer.data(), bytes));
        EXPECT_FALSE(loom().upload_layer(array, 0, layer.data(), bytes - 4));
        EXPECT_FALSE(loom().upload_layer(array, 0, nullptr, bytes));
        EXPECT_FALSE(loom().upload_layer(nullptr, 0, layer.data(), bytes));
    }

    TEST_F(TextureLoomTest, DeferredLayerUploadNeedsACallerStagingBuffer)
    {
        ASSERT_TRUE(loom().initialize(backend()));
        const ImagePtr array = loom().create_2d_array(2, 2, 2, ImageFormat::RGBA8);
        ASSERT_NE(array, nullptr);
        const std::vector<uint32_t> layer { 1, 2, 3, 4 };

        EXPECT_FALSE(loom().upload_layer(array, 0, layer.data(), layer.size() * sizeof(uint32_t), nullptr, true));
    }

    TEST_F(TextureLoomTest, BlitScalesWithNearestFilteringByReplicatingSourcePixels)
    {
        ASSERT_TRUE(loom().initialize(backend()));
        const std::vector<uint32_t> source { 1, 2, 3, 4 };
        const ImagePtr src = loom().create_2d(2, 2, ImageFormat::RGBA8, source.data());
        const ImagePtr dst = loom().create_2d(4, 4, ImageFormat::RGBA8);
        ASSERT_TRUE(src && dst);
        if (!loom().can_blit(src, dst, FilterMode::NEAREST)) {
            GTEST_SKIP() << "Device cannot blit RGBA8";
        }

        ASSERT_TRUE(loom().blit_layer(src, dst,
            LayerBlit { .src = rect(0, 0, 2, 2), .dst = rect(0, 0, 4, 4), .filter = FilterMode::NEAREST }));

        const std::vector<uint32_t> expected {
            1, 1, 2, 2,
            1, 1, 2, 2,
            3, 3, 4, 4,
            3, 3, 4, 4
        };
        EXPECT_EQ(rgba8_words(dst), expected);
    }

    TEST_F(TextureLoomTest, BlitWithFlipXMirrorsTheRow)
    {
        ASSERT_TRUE(loom().initialize(backend()));
        const std::vector<uint32_t> source { 1, 2, 3, 4 };
        const ImagePtr src = loom().create_2d(4, 1, ImageFormat::RGBA8, source.data());
        const ImagePtr dst = loom().create_2d(4, 1, ImageFormat::RGBA8);
        ASSERT_TRUE(src && dst);
        if (!loom().can_blit(src, dst, FilterMode::NEAREST)) {
            GTEST_SKIP() << "Device cannot blit RGBA8";
        }

        ASSERT_TRUE(loom().blit_layer(src, dst,
            LayerBlit { .src = rect(0, 0, 4, 1), .dst = rect(0, 0, 4, 1), .filter = FilterMode::NEAREST, .flip_x = true }));

        EXPECT_EQ(rgba8_words(dst), (std::vector<uint32_t> { 4, 3, 2, 1 }));
    }

    TEST_F(TextureLoomTest, BlitWithAZeroExtentRectangleRunsToTheImageEdge)
    {
        ASSERT_TRUE(loom().initialize(backend()));
        const std::vector<uint32_t> source { 1, 2, 3, 4 };
        const ImagePtr src = loom().create_2d(4, 1, ImageFormat::RGBA8, source.data());
        const ImagePtr dst = loom().create_2d(4, 1, ImageFormat::RGBA8);
        ASSERT_TRUE(src && dst);
        if (!loom().can_blit(src, dst, FilterMode::NEAREST)) {
            GTEST_SKIP() << "Device cannot blit RGBA8";
        }

        ASSERT_TRUE(loom().blit_layer(src, dst, LayerBlit { .filter = FilterMode::NEAREST }));

        EXPECT_EQ(rgba8_words(dst), source);
    }

    TEST_F(TextureLoomTest, BlitRefusesMissingImagesTheSameImageAndRectanglesOutsideAnImage)
    {
        ASSERT_TRUE(loom().initialize(backend()));
        const ImagePtr a = loom().create_2d(4, 4, ImageFormat::RGBA8);
        const ImagePtr b = loom().create_2d(4, 4, ImageFormat::RGBA8);
        ASSERT_TRUE(a && b);

        EXPECT_FALSE(loom().blit_layer(nullptr, b));
        EXPECT_FALSE(loom().blit_layer(a, nullptr));
        EXPECT_FALSE(loom().blit_layer(a, a));
        EXPECT_FALSE(loom().blit_layer(a, b, LayerBlit { .src = rect(2, 2, 4, 4) }));
        EXPECT_FALSE(loom().blit_layer(a, b, LayerBlit { .dst = rect(-1, 0, 2, 2) }));
        EXPECT_FALSE(loom().blit_layer(a, b, LayerBlit { .dst = rect(4, 0, 1, 1) }));
    }

    TEST_F(TextureLoomTest, BlitRefusesLayersThatDoNotExist)
    {
        ASSERT_TRUE(loom().initialize(backend()));
        const ImagePtr a = loom().create_2d(4, 4, ImageFormat::RGBA8);
        const ImagePtr b = loom().create_2d(4, 4, ImageFormat::RGBA8);
        ASSERT_TRUE(a && b);

        EXPECT_FALSE(loom().blit_layer(a, b, LayerBlit { .src_layer = 1 }));
        EXPECT_FALSE(loom().blit_layer(a, b, LayerBlit { .dst_layer = 1 }));
    }

    TEST_F(TextureLoomTest, BlitRefusesDepthImages)
    {
        ASSERT_TRUE(loom().initialize(backend()));
        const ImagePtr color = loom().create_2d(4, 4, ImageFormat::RGBA8);
        const ImagePtr depth = loom().create_depth_buffer(4, 4, false);
        ASSERT_TRUE(color && depth);

        EXPECT_FALSE(loom().blit_layer(depth, color));
        EXPECT_FALSE(loom().blit_layer(color, depth));
    }

    TEST_F(TextureLoomTest, CanBlitIsFalseForMissingImagesAndAnUninitializedLoom)
    {
        EXPECT_FALSE(loom().can_blit(nullptr, nullptr));

        ASSERT_TRUE(loom().initialize(backend()));
        const ImagePtr image = loom().create_2d(4, 4, ImageFormat::RGBA8);
        EXPECT_FALSE(loom().can_blit(image, nullptr));
        EXPECT_FALSE(loom().can_blit(nullptr, image));
    }

    TEST_F(TextureLoomTest, CanBlitIsTrueBetweenTwoRgba8ImagesOnADeviceThatSupportsIt)
    {
        ASSERT_TRUE(loom().initialize(backend()));
        const ImagePtr a = loom().create_2d(4, 4, ImageFormat::RGBA8);
        const ImagePtr b = loom().create_2d(4, 4, ImageFormat::RGBA8);
        ASSERT_TRUE(a && b);
        const auto features = backend()->get_context().get_physical_device()
                                  .getFormatProperties(vk::Format::eR8G8B8A8Unorm)
                                  .optimalTilingFeatures;
        const bool device_supports = (features & vk::FormatFeatureFlagBits::eBlitSrc)
            && (features & vk::FormatFeatureFlagBits::eBlitDst);

        EXPECT_EQ(loom().can_blit(a, b, FilterMode::NEAREST), static_cast<bool>(device_supports));
    }

    // ------------------------------------------------------------------
    // Image cache
    // ------------------------------------------------------------------

    ImageKey sampled_key(uint32_t width, uint32_t height)
    {
        return ImageKey { .width = width, .height = height, .layers = 1, .mip_levels = 1, .format = ImageFormat::RGBA8, .kind = ImageKey::Kind::SAMPLED_2D };
    }

    TEST_F(TextureLoomTest, CachedImageIsReusedForTheSameKeyAndReplacedForANewOne)
    {
        ASSERT_TRUE(loom().initialize(backend()));
        ImageCacheEntry entry;

        const ImagePtr first = loom().acquire_cached_image(entry, sampled_key(8, 8));
        const ImagePtr again = loom().acquire_cached_image(entry, sampled_key(8, 8));
        const ImagePtr larger = loom().acquire_cached_image(entry, sampled_key(16, 16));

        ASSERT_NE(first, nullptr);
        EXPECT_EQ(again, first);
        ASSERT_NE(larger, nullptr);
        EXPECT_NE(larger, first);
        EXPECT_EQ(entry.image, larger);
        EXPECT_EQ(larger->get_width(), 16U);
    }

    TEST_F(TextureLoomTest, CachedKeyKindsMapToSampledArrayAndStorageImages)
    {
        ASSERT_TRUE(loom().initialize(backend()));
        ImageCacheEntry sampled_entry;
        ImageCacheEntry array_entry;
        ImageCacheEntry storage_entry;

        const ImagePtr sampled = loom().acquire_cached_image(sampled_entry, sampled_key(8, 8));
        ImageKey array_key = sampled_key(8, 8);
        array_key.layers = 3;
        array_key.kind = ImageKey::Kind::SAMPLED_ARRAY;
        const ImagePtr array = loom().acquire_cached_image(array_entry, array_key);
        ImageKey storage_key = sampled_key(8, 8);
        storage_key.kind = ImageKey::Kind::STORAGE_2D;
        const ImagePtr storage = loom().acquire_cached_image(storage_entry, storage_key);

        ASSERT_TRUE(sampled && array && storage);
        EXPECT_EQ(sampled->get_usage(), VKImage::Usage::TEXTURE_2D);
        EXPECT_EQ(array->get_array_layers(), 3U);
        EXPECT_EQ(storage->get_usage(), VKImage::Usage::STORAGE);
    }

    TEST_F(TextureLoomTest, CachedImageRefusesDegenerateAndInconsistentKeys)
    {
        ASSERT_TRUE(loom().initialize(backend()));
        ImageCacheEntry entry;

        ImageKey zero_width = sampled_key(0, 8);
        ImageKey layered_2d = sampled_key(8, 8);
        layered_2d.layers = 2;
        ImageKey mipped_array = sampled_key(8, 8);
        mipped_array.kind = ImageKey::Kind::SAMPLED_ARRAY;
        mipped_array.layers = 2;
        mipped_array.mip_levels = 2;
        ImageKey layered_storage = sampled_key(8, 8);
        layered_storage.kind = ImageKey::Kind::STORAGE_2D;
        layered_storage.layers = 2;

        EXPECT_EQ(loom().acquire_cached_image(entry, zero_width), nullptr);
        EXPECT_EQ(loom().acquire_cached_image(entry, layered_2d), nullptr);
        EXPECT_EQ(loom().acquire_cached_image(entry, mipped_array), nullptr);
        EXPECT_EQ(loom().acquire_cached_image(entry, layered_storage), nullptr);
        EXPECT_EQ(entry.image, nullptr);
    }

    TEST_F(TextureLoomTest, RefreshCachedImageUploadsPixelsIntoTheCachedImage)
    {
        ASSERT_TRUE(loom().initialize(backend()));
        ImageCacheEntry entry;
        const std::vector<uint32_t> pixels { 7, 8, 9, 10 };
        const std::span<const uint8_t> bytes(
            reinterpret_cast<const uint8_t*>(pixels.data()), pixels.size() * sizeof(uint32_t));

        const ImagePtr image = loom().refresh_cached_image(entry, sampled_key(2, 2), bytes);

        ASSERT_NE(image, nullptr);
        EXPECT_EQ(rgba8_words(image), pixels);
    }

    TEST_F(TextureLoomTest, RefreshCachedImageRefusesAWrongByteCountAndStorageKeys)
    {
        ASSERT_TRUE(loom().initialize(backend()));
        ImageCacheEntry entry;
        const std::vector<uint8_t> too_few(15, 0);
        const std::vector<uint8_t> exact(16, 0);
        ImageKey storage_key = sampled_key(2, 2);
        storage_key.kind = ImageKey::Kind::STORAGE_2D;

        EXPECT_EQ(loom().refresh_cached_image(entry, sampled_key(2, 2), too_few), nullptr);
        EXPECT_EQ(loom().refresh_cached_image(entry, storage_key, exact), nullptr);
    }

    TEST_F(TextureLoomTest, RefreshCachedImageRefusesAStagingBufferThatIsTooSmall)
    {
        ASSERT_TRUE(loom().initialize(backend()));
        ImageCacheEntry entry;
        const std::vector<uint8_t> exact(16, 0);
        Staging tiny(4);

        EXPECT_EQ(loom().refresh_cached_image(entry, sampled_key(2, 2), exact, tiny.buffer()), nullptr);
    }

    TEST_F(TextureLoomTest, CacheSetReturnsOneImagePerKey)
    {
        ASSERT_TRUE(loom().initialize(backend()));
        ImageCacheSet cache(1U << 20);

        const ImagePtr a = cache.acquire(loom(), sampled_key(8, 8));
        const ImagePtr a_again = cache.acquire(loom(), sampled_key(8, 8));
        const ImagePtr b = cache.acquire(loom(), sampled_key(16, 8));

        ASSERT_NE(a, nullptr);
        EXPECT_EQ(a_again, a);
        ASSERT_NE(b, nullptr);
        EXPECT_NE(b, a);
        EXPECT_EQ(cache.acquire(loom(), sampled_key(8, 8)), a);
    }

    TEST_F(TextureLoomTest, CacheSetDoesNotRememberAKeyItCouldNotCreate)
    {
        ASSERT_TRUE(loom().initialize(backend()));
        ImageCacheSet cache(1U << 20);

        EXPECT_EQ(cache.acquire(loom(), sampled_key(0, 8)), nullptr);
        EXPECT_EQ(cache.acquire(loom(), sampled_key(0, 8)), nullptr);
        EXPECT_NE(cache.acquire(loom(), sampled_key(8, 8)), nullptr);
    }

    TEST_F(TextureLoomTest, CacheSetStillServesImagesBeyondItsByteBudget)
    {
        ASSERT_TRUE(loom().initialize(backend()));
        ImageCacheSet cache(1);

        EXPECT_NE(cache.acquire(loom(), sampled_key(8, 8)), nullptr);
        EXPECT_NE(cache.acquire(loom(), sampled_key(16, 16)), nullptr);
    }

    // ------------------------------------------------------------------
    // Samplers
    // ------------------------------------------------------------------

    TEST_F(TextureLoomTest, DefaultAndNearestSamplersAreDistinctAndStable)
    {
        ASSERT_TRUE(loom().initialize(backend()));

        const vk::Sampler linear = loom().get_default_sampler();
        const vk::Sampler nearest = loom().get_nearest_sampler();

        ASSERT_TRUE(linear);
        ASSERT_TRUE(nearest);
        EXPECT_NE(linear, nearest);
        EXPECT_EQ(loom().get_default_sampler(), linear);
        EXPECT_EQ(loom().get_nearest_sampler(), nearest);
    }

    TEST_F(TextureLoomTest, EqualSamplerConfigsShareOneSampler)
    {
        ASSERT_TRUE(loom().initialize(backend()));
        SamplerConfig config;
        config.address_mode_u = Portal::Graphics::AddressMode::MIRRORED_REPEAT;

        const vk::Sampler first = loom().get_or_create_sampler(config);
        const vk::Sampler second = loom().get_or_create_sampler(config);

        ASSERT_TRUE(first);
        EXPECT_EQ(first, second);
    }

} // namespace
} // namespace MayaFlux::Test
