#include "gtest/gtest.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "../support/scoped_working_dir.hpp"
#include "../support/vulkan_test_support.hpp"

#include "MayaFlux/IO/Image/ImageReader.hpp"
#include "MayaFlux/Buffers/Textures/TextureBuffer.hpp"

namespace MayaFlux::Test {
namespace {

    constexpr uint32_t image_width = 4;
    constexpr uint32_t image_height = 3;

    std::string make_image()
    {
        std::string bytes = "P6\n4 3\n255\n";
        for (uint32_t y = 0; y < image_height; ++y) {
            for (uint32_t x = 0; x < image_width; ++x) {
                bytes.push_back(static_cast<char>(20 + x * 17 + y * 3));
                bytes.push_back(static_cast<char>(60 + x * 5 + y * 19));
                bytes.push_back(static_cast<char>(100 + x * 11 + y * 7));
            }
        }
        return bytes;
    }

    std::vector<uint8_t> expected_rgba(const Kakshya::Region& region)
    {
        std::vector<uint8_t> pixels;
        for (uint64_t y = region.start_coordinates[1]; y <= region.end_coordinates[1]; ++y) {
            for (uint64_t x = region.start_coordinates[0]; x <= region.end_coordinates[0]; ++x) {
                pixels.push_back(static_cast<uint8_t>(20 + x * 17 + y * 3));
                pixels.push_back(static_cast<uint8_t>(60 + x * 5 + y * 19));
                pixels.push_back(static_cast<uint8_t>(100 + x * 11 + y * 7));
                pixels.push_back(255);
            }
        }
        return pixels;
    }

    struct CropCase {
        const char* name;
        Kakshya::Region region;
        bool filesystem_path;
    };

    class ImageReaderCropTest : public ::testing::TestWithParam<CropCase> { };

    TEST_P(ImageReaderCropTest, LoadsExactPixelsAtTheRegionsNaturalExtent)
    {
        ScopedWorkingDir files;
        files.write("image.pnm", make_image());
        const auto& param = GetParam();
        const auto image = param.filesystem_path
            ? IO::ImageReader::load(std::filesystem::path("image.pnm"), 4, param.region)
            : IO::ImageReader::load(std::string("image.pnm"), 4, param.region);

        ASSERT_TRUE(image.has_value());
        EXPECT_EQ(image->width, param.region.get_span(0));
        EXPECT_EQ(image->height, param.region.get_span(1));
        EXPECT_EQ(image->channels, 4U);
        EXPECT_EQ(image->format, Kakshya::ImageFormat::RGBA8);
        ASSERT_NE(image->as_uint8(), nullptr);
        EXPECT_EQ(*image->as_uint8(), expected_rgba(param.region));
    }

    INSTANTIATE_TEST_SUITE_P(
        PixelBounds, ImageReaderCropTest,
        ::testing::Values(
            CropCase { "InnerRectangle", Kakshya::Region::image_rect(1, 1, 2, 2), false },
            CropCase { "LastPixel", Kakshya::Region::image_rect(3, 2, 3, 2), true },
            CropCase { "WholeImage", Kakshya::Region::image_rect(0, 0, 3, 2), false }),
        [](const ::testing::TestParamInfo<CropCase>& info) { return info.param.name; });

    TEST(ImageReaderTest, ExistingWholeImageCallsKeepTheirPixelsAndDimensions)
    {
        ScopedWorkingDir files;
        files.write("image.pnm", make_image());

        const auto image = IO::ImageReader::load(std::string("image.pnm"));
        ASSERT_TRUE(image.has_value());
        EXPECT_EQ(image->width, image_width);
        EXPECT_EQ(image->height, image_height);
        EXPECT_EQ(image->channels, 4U);
        ASSERT_NE(image->as_uint8(), nullptr);
        EXPECT_EQ(*image->as_uint8(), expected_rgba(
            Kakshya::Region::image_rect(0, 0, 3, 2)));

        const auto explicit_channels = IO::ImageReader::load(
            std::filesystem::path("image.pnm"), 4);
        ASSERT_TRUE(explicit_channels.has_value());
        EXPECT_EQ(*explicit_channels->as_uint8(), *image->as_uint8());
    }

    TEST(ImageReaderTest, CroppedGrayscaleMatchesTheCorrespondingFullImagePixels)
    {
        ScopedWorkingDir files;
        files.write("image.pnm", make_image());

        const auto whole = IO::ImageReader::load(std::string("image.pnm"), 1);
        const auto crop = IO::ImageReader::load(std::string("image.pnm"), 1,
            Kakshya::Region::image_rect(1, 1, 2, 2));
        ASSERT_TRUE(whole.has_value());
        ASSERT_TRUE(crop.has_value());
        ASSERT_NE(whole->as_uint8(), nullptr);
        ASSERT_NE(crop->as_uint8(), nullptr);
        EXPECT_EQ(crop->format, Kakshya::ImageFormat::R8);
        EXPECT_EQ(*crop->as_uint8(), (std::vector<uint8_t> {
            (*whole->as_uint8())[5], (*whole->as_uint8())[6],
            (*whole->as_uint8())[9], (*whole->as_uint8())[10] }));
    }

    struct InvalidCropCase {
        const char* name;
        Kakshya::Region region;
    };

    class ImageReaderInvalidCropTest : public ::testing::TestWithParam<InvalidCropCase> { };

    TEST_P(ImageReaderInvalidCropTest, RejectsInvalidBounds)
    {
        ScopedWorkingDir files;
        files.write("image.pnm", make_image());
        EXPECT_FALSE(IO::ImageReader::load(std::string("image.pnm"), 4, GetParam().region));
    }

    INSTANTIATE_TEST_SUITE_P(
        InvalidBounds, ImageReaderInvalidCropTest,
        ::testing::Values(
            InvalidCropCase { "Reversed", Kakshya::Region::image_rect(2, 1, 1, 2) },
            InvalidCropCase { "PastRightEdge", Kakshya::Region::image_rect(0, 0, 4, 1) },
            InvalidCropCase { "PastBottomEdge", Kakshya::Region::image_rect(0, 0, 1, 3) },
            InvalidCropCase { "WrongDimensions", Kakshya::Region(
                std::vector<uint64_t> { 0 }, std::vector<uint64_t> { 1 }) }),
        [](const ::testing::TestParamInfo<InvalidCropCase>& info) { return info.param.name; });

    TEST(ImageReaderTest, ExistingReaderRegionReturnsExclusiveEndPixelBytes)
    {
        ScopedWorkingDir files;
        files.write("image.pnm", make_image());
        IO::ImageReader reader;
        ASSERT_TRUE(reader.open("image.pnm"));

        IO::FileRegion region;
        region.start_coordinates = { 1, 1 };
        region.end_coordinates = { 3, 3 };
        const auto values = reader.read_region(region);
        ASSERT_EQ(values.size(), 1U);
        const auto* pixels = std::get_if<std::vector<uint8_t>>(&values.front());
        ASSERT_NE(pixels, nullptr);
        EXPECT_EQ(*pixels, expected_rgba(Kakshya::Region::image_rect(1, 1, 2, 2)));
    }

    TEST(ImageReaderTest, ConfiguredReaderCreatesCroppedTextureBuffer)
    {
        ScopedWorkingDir files;
        files.write("image.pnm", make_image());
        IO::ImageReader whole_reader;
        IO::ImageReader cropped_reader(4, Kakshya::Region::image_rect(1, 1, 2, 2));
        ASSERT_TRUE(whole_reader.open("image.pnm"));
        ASSERT_TRUE(cropped_reader.open("image.pnm"));

        const auto whole = whole_reader.create_texture_buffer();
        const auto crop = cropped_reader.create_texture_buffer();
        ASSERT_NE(whole, nullptr);
        ASSERT_NE(crop, nullptr);
        EXPECT_EQ(whole->get_width(), image_width);
        EXPECT_EQ(whole->get_height(), image_height);
        EXPECT_EQ(crop->get_width(), 2U);
        EXPECT_EQ(crop->get_height(), 2U);
        EXPECT_EQ(crop->get_pixel_data(), expected_rgba(
            Kakshya::Region::image_rect(1, 1, 2, 2)));
        IO::ImageReader invalid_reader(4, Kakshya::Region::image_rect(0, 0, 4, 2));
        EXPECT_FALSE(invalid_reader.open("image.pnm"));
    }

    TEST(ImageReaderTest, ConfiguredReaderForwardsChannelCountAndRegion)
    {
        ScopedWorkingDir files;
        files.write("image.pnm", make_image());
        const auto region = Kakshya::Region::image_rect(1, 1, 2, 2);
        IO::ImageReader reader(1, region);
        ASSERT_TRUE(reader.open("image.pnm"));

        const auto expected = IO::ImageReader::load(std::string("image.pnm"), 1, region);
        const auto buffer = reader.create_texture_buffer();
        ASSERT_TRUE(expected.has_value());
        ASSERT_NE(buffer, nullptr);
        ASSERT_NE(expected->as_uint8(), nullptr);
        EXPECT_EQ(buffer->get_format(), Kakshya::ImageFormat::R8);
        EXPECT_EQ(buffer->get_width(), 2U);
        EXPECT_EQ(buffer->get_height(), 2U);
        EXPECT_EQ(buffer->get_pixel_data(), *expected->as_uint8());
        EXPECT_EQ(buffer->get_render_config().fragment_shader, "texture_gray.frag.spv");
    }

    TEST(ImageReaderTest, TwoChannelReaderUsesGrayAlphaDisplay)
    {
        ScopedWorkingDir files;
        files.write("image.pnm", make_image());
        IO::ImageReader reader(2);
        ASSERT_TRUE(reader.open("image.pnm"));

        const auto buffer = reader.create_texture_buffer();
        ASSERT_NE(buffer, nullptr);
        EXPECT_EQ(buffer->get_format(), Kakshya::ImageFormat::RG8);
        EXPECT_EQ(buffer->get_render_config().fragment_shader,
            "texture_gray_alpha.frag.spv");
    }

    class ImageReaderTextureTest : public ::testing::Test {
    protected:
        static void SetUpTestSuite()
        {
            if (!vulkan_unavailable_reason().empty()) {
                return;
            }
            s_backend = std::make_shared<Core::VulkanBackend>();
            if (!s_backend->initialize(headless_graphics_config())) {
                s_backend.reset();
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
            auto& loom = Portal::Graphics::TextureLoom::instance();
            loom.shutdown();
            ASSERT_TRUE(loom.initialize(s_backend));
        }

        void TearDown() override
        {
            Portal::Graphics::TextureLoom::instance().shutdown();
        }

    private:
        static inline std::shared_ptr<Core::VulkanBackend> s_backend;
    };

    TEST_F(ImageReaderTextureTest, UploadsCroppedPixelsAndKeepsWholeImageBehavior)
    {
        ScopedWorkingDir files;
        files.write("image.pnm", make_image());

        const auto whole = IO::ImageReader::load_texture("image.pnm");
        const auto crop = IO::ImageReader::load_texture("image.pnm", 4,
            Kakshya::Region::image_rect(1, 1, 2, 2));
        ASSERT_NE(whole, nullptr);
        ASSERT_NE(crop, nullptr);

        auto& loom = Portal::Graphics::TextureLoom::instance();
        const auto whole_pixels = loom.download_image(whole);
        const auto crop_pixels = loom.download_image(crop);
        ASSERT_TRUE(whole_pixels.has_value());
        ASSERT_TRUE(crop_pixels.has_value());
        EXPECT_EQ(whole_pixels->width, image_width);
        EXPECT_EQ(whole_pixels->height, image_height);
        EXPECT_EQ(crop_pixels->width, 2U);
        EXPECT_EQ(crop_pixels->height, 2U);
        ASSERT_NE(crop_pixels->as_uint8(), nullptr);
        EXPECT_EQ(*crop_pixels->as_uint8(), expected_rgba(
            Kakshya::Region::image_rect(1, 1, 2, 2)));
    }

} // namespace
} // namespace MayaFlux::Test
