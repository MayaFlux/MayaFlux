#include "gtest/gtest.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "MayaFlux/Portal/Graphics/SamplerForge.hpp"

namespace MayaFlux::Test {
namespace {

    using Portal::Graphics::AddressMode;
    using Portal::Graphics::BorderColor;
    using Portal::Graphics::FilterMode;
    using Portal::Graphics::FitMode;
    using Portal::Graphics::FitPlan;
    using Portal::Graphics::SamplerForge;

    ::testing::AssertionResult rect_is(const vk::Rect2D& rect, int32_t x, int32_t y, uint32_t w, uint32_t h)
    {
        if (rect.offset.x == x && rect.offset.y == y && rect.extent.width == w && rect.extent.height == h) {
            return ::testing::AssertionSuccess();
        }
        return ::testing::AssertionFailure()
            << "got (" << rect.offset.x << ", " << rect.offset.y << ", "
            << rect.extent.width << "x" << rect.extent.height << "), expected ("
            << x << ", " << y << ", " << w << "x" << h << ")";
    }

    ::testing::AssertionResult sampler_is(const FitPlan& plan, AddressMode address, BorderColor border)
    {
        const auto& s = plan.sampler;
        if (s.address_mode_u == address && s.address_mode_v == address && s.address_mode_w == address
            && s.border_color == border) {
            return ::testing::AssertionSuccess();
        }
        return ::testing::AssertionFailure()
            << "addressing u/v/w = " << static_cast<int>(s.address_mode_u) << "/"
            << static_cast<int>(s.address_mode_v) << "/" << static_cast<int>(s.address_mode_w)
            << ", border " << static_cast<int>(s.border_color) << ", expected address "
            << static_cast<int>(address) << " border " << static_cast<int>(border);
    }

    // ------------------------------------------------------------------
    // Degenerate input
    // ------------------------------------------------------------------

    struct ZeroCase {
        std::string_view name;
        uint32_t src_width;
        uint32_t src_height;
        uint32_t dst_width;
        uint32_t dst_height;
    };

    class FitPlanZeroExtentTest : public ::testing::TestWithParam<ZeroCase> { };

    TEST_P(FitPlanZeroExtentTest, YieldsNoBlitsAndKeepsTheRequestedFilter)
    {
        const auto& p = GetParam();

        const FitPlan plan = SamplerForge::plan_fit(
            p.src_width, p.src_height, p.dst_width, p.dst_height, FitMode::CONTAIN, FilterMode::NEAREST);

        EXPECT_TRUE(plan.blits.empty());
        EXPECT_EQ(plan.sampler.mag_filter, FilterMode::NEAREST);
        EXPECT_EQ(plan.sampler.min_filter, FilterMode::NEAREST);
    }

    INSTANTIATE_TEST_SUITE_P(Extents, FitPlanZeroExtentTest,
        ::testing::Values(
            ZeroCase { "SourceWidth", 0, 4, 8, 8 },
            ZeroCase { "SourceHeight", 4, 0, 8, 8 },
            ZeroCase { "DestinationWidth", 4, 4, 0, 8 },
            ZeroCase { "DestinationHeight", 4, 4, 8, 0 }),
        [](const ::testing::TestParamInfo<ZeroCase>& info) { return std::string(info.param.name); });

    // ------------------------------------------------------------------
    // Per mode placement
    // ------------------------------------------------------------------

    TEST(FitPlanTest, DefaultsStretchWithLinearFiltering)
    {
        const FitPlan plan = SamplerForge::plan_fit(10, 10, 20, 20);

        ASSERT_EQ(plan.blits.size(), 1U);
        EXPECT_EQ(plan.blits[0].filter, FilterMode::LINEAR);
        EXPECT_EQ(plan.sampler.mag_filter, FilterMode::LINEAR);
        EXPECT_EQ(plan.sampler.min_filter, FilterMode::LINEAR);
        EXPECT_TRUE(rect_is(plan.blits[0].dst, 0, 0, 20, 20));
    }

    TEST(FitPlanTest, StretchMapsTheWholeSourceOntoTheWholeDestinationIgnoringAspect)
    {
        const FitPlan plan = SamplerForge::plan_fit(200, 100, 64, 64, FitMode::STRETCH);

        ASSERT_EQ(plan.blits.size(), 1U);
        EXPECT_TRUE(rect_is(plan.blits[0].src, 0, 0, 200, 100));
        EXPECT_TRUE(rect_is(plan.blits[0].dst, 0, 0, 64, 64));
        EXPECT_FALSE(plan.blits[0].flip_x);
        EXPECT_FALSE(plan.blits[0].flip_y);
    }

    TEST(FitPlanTest, ContainKeepsAspectAndCentersTheScaledSource)
    {
        const FitPlan plan = SamplerForge::plan_fit(200, 100, 100, 100, FitMode::CONTAIN);

        ASSERT_EQ(plan.blits.size(), 1U);
        EXPECT_TRUE(rect_is(plan.blits[0].src, 0, 0, 200, 100));
        EXPECT_TRUE(rect_is(plan.blits[0].dst, 0, 25, 100, 50));
    }

    TEST(FitPlanTest, ContainOfMatchingAspectCoversTheWholeDestination)
    {
        const FitPlan plan = SamplerForge::plan_fit(50, 25, 200, 100, FitMode::CONTAIN);

        ASSERT_EQ(plan.blits.size(), 1U);
        EXPECT_TRUE(rect_is(plan.blits[0].dst, 0, 0, 200, 100));
    }

    TEST(FitPlanTest, ContainCentersAlongTheShortAxisInATallDestination)
    {
        const FitPlan plan = SamplerForge::plan_fit(100, 100, 40, 100, FitMode::CONTAIN);

        ASSERT_EQ(plan.blits.size(), 1U);
        EXPECT_TRUE(rect_is(plan.blits[0].dst, 0, 30, 40, 40));
    }

    TEST(FitPlanTest, CoverCropsTheSourceToTheDestinationAspectAroundItsCenter)
    {
        const FitPlan plan = SamplerForge::plan_fit(200, 100, 100, 100, FitMode::COVER);

        ASSERT_EQ(plan.blits.size(), 1U);
        EXPECT_TRUE(rect_is(plan.blits[0].src, 50, 0, 100, 100));
        EXPECT_TRUE(rect_is(plan.blits[0].dst, 0, 0, 100, 100));
    }

    TEST(FitPlanTest, CoverCropsTheTallAxisWhenTheSourceIsNarrower)
    {
        const FitPlan plan = SamplerForge::plan_fit(100, 200, 100, 100, FitMode::COVER);

        ASSERT_EQ(plan.blits.size(), 1U);
        EXPECT_TRUE(rect_is(plan.blits[0].src, 0, 50, 100, 100));
        EXPECT_TRUE(rect_is(plan.blits[0].dst, 0, 0, 100, 100));
    }

    TEST(FitPlanTest, CenterPlacesASmallerSourceAtItsOwnSizeInTheMiddle)
    {
        const FitPlan plan = SamplerForge::plan_fit(50, 50, 100, 100, FitMode::CENTER);

        ASSERT_EQ(plan.blits.size(), 1U);
        EXPECT_TRUE(rect_is(plan.blits[0].src, 0, 0, 50, 50));
        EXPECT_TRUE(rect_is(plan.blits[0].dst, 25, 25, 50, 50));
    }

    TEST(FitPlanTest, CenterClipsALargerSourceToTheDestinationAroundItsCenter)
    {
        const FitPlan plan = SamplerForge::plan_fit(200, 200, 100, 100, FitMode::CENTER);

        ASSERT_EQ(plan.blits.size(), 1U);
        EXPECT_TRUE(rect_is(plan.blits[0].src, 50, 50, 100, 100));
        EXPECT_TRUE(rect_is(plan.blits[0].dst, 0, 0, 100, 100));
    }

    // ------------------------------------------------------------------
    // Tiling
    // ------------------------------------------------------------------

    TEST(FitPlanTest, TileEmitsOneBlitPerTileInRowMajorOrderWithEdgeTilesTrimmed)
    {
        const FitPlan plan = SamplerForge::plan_fit(4, 4, 10, 6, FitMode::TILE);

        ASSERT_EQ(plan.blits.size(), 6U);
        EXPECT_TRUE(rect_is(plan.blits[0].dst, 0, 0, 4, 4));
        EXPECT_TRUE(rect_is(plan.blits[1].dst, 4, 0, 4, 4));
        EXPECT_TRUE(rect_is(plan.blits[2].dst, 8, 0, 2, 4));
        EXPECT_TRUE(rect_is(plan.blits[3].dst, 0, 4, 4, 2));
        EXPECT_TRUE(rect_is(plan.blits[4].dst, 4, 4, 4, 2));
        EXPECT_TRUE(rect_is(plan.blits[5].dst, 8, 4, 2, 2));
        EXPECT_TRUE(rect_is(plan.blits[2].src, 0, 0, 2, 4));
        EXPECT_TRUE(rect_is(plan.blits[5].src, 0, 0, 2, 2));
    }

    TEST(FitPlanTest, TileNeverFlips)
    {
        const FitPlan plan = SamplerForge::plan_fit(4, 4, 10, 10, FitMode::TILE);

        ASSERT_FALSE(plan.blits.empty());
        for (const auto& blit : plan.blits) {
            EXPECT_FALSE(blit.flip_x);
            EXPECT_FALSE(blit.flip_y);
        }
    }

    TEST(FitPlanTest, TileOfAnExactMultipleEmitsFullSizeTilesOnly)
    {
        const FitPlan plan = SamplerForge::plan_fit(5, 5, 10, 10, FitMode::TILE);

        ASSERT_EQ(plan.blits.size(), 4U);
        for (const auto& blit : plan.blits) {
            EXPECT_EQ(blit.dst.extent.width, 5U);
            EXPECT_EQ(blit.dst.extent.height, 5U);
        }
    }

    TEST(FitPlanTest, TileOfASourceLargerThanTheDestinationEmitsOneClippedBlit)
    {
        const FitPlan plan = SamplerForge::plan_fit(10, 10, 4, 4, FitMode::TILE);

        ASSERT_EQ(plan.blits.size(), 1U);
        EXPECT_TRUE(rect_is(plan.blits[0].src, 0, 0, 4, 4));
        EXPECT_TRUE(rect_is(plan.blits[0].dst, 0, 0, 4, 4));
    }

    TEST(FitPlanTest, MirroredTileFlipsEveryOtherColumnAndReadsTheTrimmedEdgeFromTheFarSide)
    {
        const FitPlan plan = SamplerForge::plan_fit(5, 5, 8, 5, FitMode::TILE_MIRRORED);

        ASSERT_EQ(plan.blits.size(), 2U);
        EXPECT_FALSE(plan.blits[0].flip_x);
        EXPECT_TRUE(rect_is(plan.blits[0].src, 0, 0, 5, 5));
        EXPECT_TRUE(plan.blits[1].flip_x);
        EXPECT_FALSE(plan.blits[1].flip_y);
        EXPECT_TRUE(rect_is(plan.blits[1].src, 2, 0, 3, 5));
        EXPECT_TRUE(rect_is(plan.blits[1].dst, 5, 0, 3, 5));
    }

    TEST(FitPlanTest, MirroredTileFlipsEveryOtherRowAndReadsTheTrimmedEdgeFromTheFarSide)
    {
        const FitPlan plan = SamplerForge::plan_fit(5, 5, 5, 8, FitMode::TILE_MIRRORED);

        ASSERT_EQ(plan.blits.size(), 2U);
        EXPECT_FALSE(plan.blits[0].flip_y);
        EXPECT_TRUE(plan.blits[1].flip_y);
        EXPECT_FALSE(plan.blits[1].flip_x);
        EXPECT_TRUE(rect_is(plan.blits[1].src, 0, 2, 5, 3));
        EXPECT_TRUE(rect_is(plan.blits[1].dst, 0, 5, 5, 3));
    }

    TEST(FitPlanTest, MirroredTileFlipsDiagonalNeighboursOnBothAxes)
    {
        const FitPlan plan = SamplerForge::plan_fit(4, 4, 8, 8, FitMode::TILE_MIRRORED);

        ASSERT_EQ(plan.blits.size(), 4U);
        EXPECT_TRUE(!plan.blits[0].flip_x && !plan.blits[0].flip_y);
        EXPECT_TRUE(plan.blits[1].flip_x && !plan.blits[1].flip_y);
        EXPECT_TRUE(!plan.blits[2].flip_x && plan.blits[2].flip_y);
        EXPECT_TRUE(plan.blits[3].flip_x && plan.blits[3].flip_y);
    }

    // ------------------------------------------------------------------
    // Sampler addressing that matches each fit
    // ------------------------------------------------------------------

    struct AddressingCase {
        std::string_view name;
        FitMode mode;
        AddressMode address;
        BorderColor border;
    };

    class FitPlanAddressingTest : public ::testing::TestWithParam<AddressingCase> { };

    TEST_P(FitPlanAddressingTest, SamplerAddressingReproducesTheFitWhenSampledDirectly)
    {
        const auto& p = GetParam();

        const FitPlan plan = SamplerForge::plan_fit(30, 20, 50, 40, p.mode);

        EXPECT_TRUE(sampler_is(plan, p.address, p.border));
    }

    INSTANTIATE_TEST_SUITE_P(Modes, FitPlanAddressingTest,
        ::testing::Values(
            AddressingCase { "Stretch", FitMode::STRETCH, AddressMode::CLAMP_TO_EDGE, BorderColor::DEFAULT },
            AddressingCase { "Contain", FitMode::CONTAIN, AddressMode::CLAMP_TO_BORDER, BorderColor::TRANSPARENT_BLACK },
            AddressingCase { "Cover", FitMode::COVER, AddressMode::CLAMP_TO_EDGE, BorderColor::DEFAULT },
            AddressingCase { "Center", FitMode::CENTER, AddressMode::CLAMP_TO_BORDER, BorderColor::TRANSPARENT_BLACK },
            AddressingCase { "Tile", FitMode::TILE, AddressMode::REPEAT, BorderColor::DEFAULT },
            AddressingCase { "TileMirrored", FitMode::TILE_MIRRORED, AddressMode::MIRRORED_REPEAT, BorderColor::DEFAULT }),
        [](const ::testing::TestParamInfo<AddressingCase>& info) { return std::string(info.param.name); });

    // ------------------------------------------------------------------
    // Invariants over sizes and modes
    // ------------------------------------------------------------------

    struct SizeCase {
        std::string_view name;
        uint32_t src_width;
        uint32_t src_height;
        uint32_t dst_width;
        uint32_t dst_height;
    };

    using BoundsParam = std::tuple<SizeCase, FitMode>;

    std::string bounds_name(const ::testing::TestParamInfo<BoundsParam>& info)
    {
        static constexpr std::string_view modes[] = {
            "Stretch", "Contain", "Cover", "Center", "Tile", "TileMirrored"
        };
        return std::string(std::get<0>(info.param).name) + "_"
            + std::string(modes[static_cast<std::size_t>(std::get<1>(info.param))]);
    }

    class FitPlanBoundsTest : public ::testing::TestWithParam<BoundsParam> { };

    TEST_P(FitPlanBoundsTest, EveryBlitStaysInsideBothImagesWithNonZeroSizeAndRequestedFilter)
    {
        const auto& [size, mode] = GetParam();

        const FitPlan plan = SamplerForge::plan_fit(
            size.src_width, size.src_height, size.dst_width, size.dst_height, mode, FilterMode::NEAREST);

        ASSERT_FALSE(plan.blits.empty());
        for (const auto& blit : plan.blits) {
            EXPECT_GT(blit.src.extent.width, 0U);
            EXPECT_GT(blit.src.extent.height, 0U);
            EXPECT_GT(blit.dst.extent.width, 0U);
            EXPECT_GT(blit.dst.extent.height, 0U);
            EXPECT_GE(blit.src.offset.x, 0);
            EXPECT_GE(blit.src.offset.y, 0);
            EXPECT_GE(blit.dst.offset.x, 0);
            EXPECT_GE(blit.dst.offset.y, 0);
            EXPECT_LE(static_cast<uint32_t>(blit.src.offset.x) + blit.src.extent.width, size.src_width);
            EXPECT_LE(static_cast<uint32_t>(blit.src.offset.y) + blit.src.extent.height, size.src_height);
            EXPECT_LE(static_cast<uint32_t>(blit.dst.offset.x) + blit.dst.extent.width, size.dst_width);
            EXPECT_LE(static_cast<uint32_t>(blit.dst.offset.y) + blit.dst.extent.height, size.dst_height);
            EXPECT_EQ(blit.src_layer, 0U);
            EXPECT_EQ(blit.dst_layer, 0U);
            EXPECT_EQ(blit.filter, FilterMode::NEAREST);
        }
    }

    TEST_P(FitPlanBoundsTest, DestinationAreaIsFullyCoveredByTheModesThatFillIt)
    {
        const auto& [size, mode] = GetParam();

        const FitPlan plan = SamplerForge::plan_fit(
            size.src_width, size.src_height, size.dst_width, size.dst_height, mode);

        uint64_t covered = 0;
        for (const auto& blit : plan.blits) {
            covered += static_cast<uint64_t>(blit.dst.extent.width) * blit.dst.extent.height;
        }
        const uint64_t total = static_cast<uint64_t>(size.dst_width) * size.dst_height;

        EXPECT_LE(covered, total);
        if (mode == FitMode::STRETCH || mode == FitMode::COVER
            || mode == FitMode::TILE || mode == FitMode::TILE_MIRRORED) {
            EXPECT_EQ(covered, total);
        }
    }

    INSTANTIATE_TEST_SUITE_P(Sizes, FitPlanBoundsTest,
        ::testing::Combine(
            ::testing::Values(
                SizeCase { "Square", 64, 64, 64, 64 },
                SizeCase { "Upscale", 16, 9, 1920, 1080 },
                SizeCase { "Downscale", 3840, 2160, 640, 360 },
                SizeCase { "OddPrime", 7, 13, 101, 37 },
                SizeCase { "SinglePixel", 1, 1, 5, 5 },
                SizeCase { "TallToWide", 10, 200, 200, 10 },
                SizeCase { "NarrowDestination", 100, 100, 1, 100 }),
            ::testing::Values(
                FitMode::STRETCH, FitMode::CONTAIN, FitMode::COVER,
                FitMode::CENTER, FitMode::TILE, FitMode::TILE_MIRRORED)),
        bounds_name);

} // namespace
} // namespace MayaFlux::Test
