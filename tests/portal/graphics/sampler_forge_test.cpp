#include "gtest/gtest.h"

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "../../support/vulkan_test_support.hpp"

#include "MayaFlux/Portal/Graphics/SamplerForge.hpp"

namespace MayaFlux::Test {
namespace {

    using Portal::Graphics::AddressMode;
    using Portal::Graphics::BorderColor;
    using Portal::Graphics::FilterMode;
    using Portal::Graphics::SamplerConfig;
    using Portal::Graphics::SamplerForge;

    SamplerForge& forge()
    {
        return Portal::Graphics::get_sampler_factory();
    }

    class SamplerForgeTest : public ::testing::Test {
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
            forge().shutdown();
        }

        void TearDown() override
        {
            forge().shutdown();
        }

        static const std::shared_ptr<Core::VulkanBackend>& backend() { return s_backend; }

    private:
        static inline std::shared_ptr<Core::VulkanBackend> s_backend;
    };

    // ------------------------------------------------------------------
    // Lifecycle
    // ------------------------------------------------------------------

    TEST_F(SamplerForgeTest, UninitializedForgeHandsOutNoSamplers)
    {
        EXPECT_FALSE(forge().is_initialized());
        EXPECT_FALSE(forge().get_or_create(SamplerConfig {}));
        EXPECT_FALSE(forge().get_default_linear());
        EXPECT_FALSE(forge().get_default_nearest());
        EXPECT_FALSE(forge().get_anisotropic());
        EXPECT_EQ(forge().get_sampler_count(), 0U);
    }

    TEST_F(SamplerForgeTest, DestroyingBeforeInitializationIsIgnored)
    {
        EXPECT_NO_THROW(forge().destroy_sampler(vk::Sampler {}));
        EXPECT_EQ(forge().get_sampler_count(), 0U);
    }

    TEST_F(SamplerForgeTest, InitializeRejectsANullBackend)
    {
        EXPECT_FALSE(forge().initialize(nullptr));
        EXPECT_FALSE(forge().is_initialized());
    }

    TEST_F(SamplerForgeTest, InitializeMakesTheForgeReadyWithAnEmptyCache)
    {
        ASSERT_TRUE(forge().initialize(backend()));

        EXPECT_TRUE(forge().is_initialized());
        EXPECT_EQ(forge().get_sampler_count(), 0U);
    }

    TEST_F(SamplerForgeTest, InitializingAgainKeepsTheCacheAndReportsSuccess)
    {
        ASSERT_TRUE(forge().initialize(backend()));
        const vk::Sampler sampler = forge().get_default_linear();
        ASSERT_TRUE(sampler);

        EXPECT_TRUE(forge().initialize(backend()));

        EXPECT_TRUE(forge().is_initialized());
        EXPECT_EQ(forge().get_sampler_count(), 1U);
        EXPECT_EQ(forge().get_default_linear(), sampler);
    }

    TEST_F(SamplerForgeTest, ShutdownEmptiesTheCacheAndReleasesTheBackend)
    {
        ASSERT_TRUE(forge().initialize(backend()));
        ASSERT_TRUE(forge().get_default_linear());
        ASSERT_TRUE(forge().get_default_nearest());

        forge().shutdown();

        EXPECT_FALSE(forge().is_initialized());
        EXPECT_EQ(forge().get_sampler_count(), 0U);
        EXPECT_FALSE(forge().get_default_linear());
    }

    TEST_F(SamplerForgeTest, ShutdownIsRepeatable)
    {
        ASSERT_TRUE(forge().initialize(backend()));

        forge().shutdown();

        EXPECT_NO_THROW(forge().shutdown());
        EXPECT_FALSE(forge().is_initialized());
    }

    TEST_F(SamplerForgeTest, ForgeServesSamplersAgainAfterAShutdownAndReinitialize)
    {
        ASSERT_TRUE(forge().initialize(backend()));
        ASSERT_TRUE(forge().get_default_linear());
        forge().shutdown();

        ASSERT_TRUE(forge().initialize(backend()));

        EXPECT_TRUE(forge().get_default_linear());
        EXPECT_EQ(forge().get_sampler_count(), 1U);
    }

    // ------------------------------------------------------------------
    // Caching
    // ------------------------------------------------------------------

    TEST_F(SamplerForgeTest, EqualConfigsShareOneCachedSampler)
    {
        ASSERT_TRUE(forge().initialize(backend()));
        SamplerConfig config;
        config.address_mode_u = AddressMode::MIRRORED_REPEAT;

        const vk::Sampler first = forge().get_or_create(config);
        const vk::Sampler second = forge().get_or_create(config);

        ASSERT_TRUE(first);
        EXPECT_EQ(first, second);
        EXPECT_EQ(forge().get_sampler_count(), 1U);
    }

    struct FieldCase {
        std::string_view name;
        std::function<void(SamplerConfig&)> change;
    };

    std::vector<FieldCase> field_cases()
    {
        return {
            { "MagFilter", [](SamplerConfig& c) { c.mag_filter = FilterMode::NEAREST; } },
            { "MinFilter", [](SamplerConfig& c) { c.min_filter = FilterMode::NEAREST; } },
            { "AddressU", [](SamplerConfig& c) { c.address_mode_u = AddressMode::CLAMP_TO_EDGE; } },
            { "AddressV", [](SamplerConfig& c) { c.address_mode_v = AddressMode::MIRRORED_REPEAT; } },
            { "AddressW", [](SamplerConfig& c) { c.address_mode_w = AddressMode::CLAMP_TO_EDGE; } },
            { "Anisotropy", [](SamplerConfig& c) { c.max_anisotropy = 4.0F; } },
            { "Mipmaps", [](SamplerConfig& c) { c.enable_mipmaps = true; } },
            { "BorderColor", [](SamplerConfig& c) { c.border_color = BorderColor::OPAQUE_WHITE; } }
        };
    }

    class SamplerForgeFieldTest : public SamplerForgeTest, public ::testing::WithParamInterface<FieldCase> { };

    TEST_P(SamplerForgeFieldTest, ChangingOneFieldYieldsAnotherSampler)
    {
        ASSERT_TRUE(forge().initialize(backend()));
        const SamplerConfig baseline;
        SamplerConfig changed;
        GetParam().change(changed);

        const vk::Sampler first = forge().get_or_create(baseline);
        const vk::Sampler second = forge().get_or_create(changed);

        ASSERT_TRUE(first);
        ASSERT_TRUE(second);
        EXPECT_NE(first, second);
        EXPECT_EQ(forge().get_sampler_count(), 2U);
    }

    INSTANTIATE_TEST_SUITE_P(Fields, SamplerForgeFieldTest,
        ::testing::ValuesIn(field_cases()),
        [](const ::testing::TestParamInfo<FieldCase>& info) { return std::string(info.param.name); });

    // ------------------------------------------------------------------
    // Creation over the enumerations
    // ------------------------------------------------------------------

    struct AddressCase {
        std::string_view name;
        AddressMode mode;
    };

    class SamplerForgeAddressTest : public SamplerForgeTest, public ::testing::WithParamInterface<AddressCase> { };

    TEST_P(SamplerForgeAddressTest, EveryAddressModeCreatesASampler)
    {
        ASSERT_TRUE(forge().initialize(backend()));
        SamplerConfig config;
        config.address_mode_u = GetParam().mode;
        config.address_mode_v = GetParam().mode;
        config.address_mode_w = GetParam().mode;

        EXPECT_TRUE(forge().get_or_create(config));
        EXPECT_EQ(forge().get_sampler_count(), 1U);
    }

    INSTANTIATE_TEST_SUITE_P(Modes, SamplerForgeAddressTest,
        ::testing::Values(
            AddressCase { "Repeat", AddressMode::REPEAT },
            AddressCase { "MirroredRepeat", AddressMode::MIRRORED_REPEAT },
            AddressCase { "ClampToEdge", AddressMode::CLAMP_TO_EDGE },
            AddressCase { "ClampToBorder", AddressMode::CLAMP_TO_BORDER }),
        [](const ::testing::TestParamInfo<AddressCase>& info) { return std::string(info.param.name); });

    struct BorderCase {
        std::string_view name;
        BorderColor color;
    };

    class SamplerForgeBorderTest : public SamplerForgeTest, public ::testing::WithParamInterface<BorderCase> { };

    TEST_P(SamplerForgeBorderTest, EveryBorderColorCreatesAClampToBorderSampler)
    {
        ASSERT_TRUE(forge().initialize(backend()));
        SamplerConfig config;
        config.address_mode_u = AddressMode::CLAMP_TO_BORDER;
        config.address_mode_v = AddressMode::CLAMP_TO_BORDER;
        config.address_mode_w = AddressMode::CLAMP_TO_BORDER;
        config.border_color = GetParam().color;

        EXPECT_TRUE(forge().get_or_create(config));
    }

    INSTANTIATE_TEST_SUITE_P(Colors, SamplerForgeBorderTest,
        ::testing::Values(
            BorderCase { "Default", BorderColor::DEFAULT },
            BorderCase { "TransparentBlack", BorderColor::TRANSPARENT_BLACK },
            BorderCase { "OpaqueBlack", BorderColor::OPAQUE_BLACK },
            BorderCase { "OpaqueWhite", BorderColor::OPAQUE_WHITE }),
        [](const ::testing::TestParamInfo<BorderCase>& info) { return std::string(info.param.name); });

    struct FilterCase {
        std::string_view name;
        FilterMode mode;
        bool mipmaps;
    };

    class SamplerForgeFilterTest : public SamplerForgeTest, public ::testing::WithParamInterface<FilterCase> { };

    TEST_P(SamplerForgeFilterTest, EachFilterCreatesASamplerWithAndWithoutMipmaps)
    {
        ASSERT_TRUE(forge().initialize(backend()));
        SamplerConfig config;
        config.mag_filter = GetParam().mode;
        config.min_filter = GetParam().mode;
        config.enable_mipmaps = GetParam().mipmaps;

        EXPECT_TRUE(forge().get_or_create(config));
    }

    INSTANTIATE_TEST_SUITE_P(Filters, SamplerForgeFilterTest,
        ::testing::Values(
            FilterCase { "NearestNoMips", FilterMode::NEAREST, false },
            FilterCase { "NearestMips", FilterMode::NEAREST, true },
            FilterCase { "LinearNoMips", FilterMode::LINEAR, false },
            FilterCase { "LinearMips", FilterMode::LINEAR, true }),
        [](const ::testing::TestParamInfo<FilterCase>& info) { return std::string(info.param.name); });

    TEST_F(SamplerForgeTest, CubicFilterEitherCreatesASamplerOrLeavesTheCacheEmpty)
    {
        ASSERT_TRUE(forge().initialize(backend()));
        SamplerConfig config;
        config.mag_filter = FilterMode::CUBIC;
        config.min_filter = FilterMode::CUBIC;

        const vk::Sampler sampler = forge().get_or_create(config);

        EXPECT_EQ(forge().get_sampler_count(), sampler ? 1U : 0U);
    }

    TEST_F(SamplerForgeTest, AnisotropyBeyondTheDeviceLimitStillCreatesASampler)
    {
        ASSERT_TRUE(forge().initialize(backend()));
        SamplerConfig config;
        config.max_anisotropy = 1024.0F;

        EXPECT_TRUE(forge().get_or_create(config));
    }

    // ------------------------------------------------------------------
    // Defaults
    // ------------------------------------------------------------------

    TEST_F(SamplerForgeTest, LinearAndNearestDefaultsAreDistinctAndStable)
    {
        ASSERT_TRUE(forge().initialize(backend()));

        const vk::Sampler linear = forge().get_default_linear();
        const vk::Sampler nearest = forge().get_default_nearest();

        ASSERT_TRUE(linear);
        ASSERT_TRUE(nearest);
        EXPECT_NE(linear, nearest);
        EXPECT_EQ(forge().get_default_linear(), linear);
        EXPECT_EQ(forge().get_default_nearest(), nearest);
        EXPECT_EQ(forge().get_sampler_count(), 2U);
    }

    TEST_F(SamplerForgeTest, LinearDefaultIsTheSamplerOfADefaultConfig)
    {
        ASSERT_TRUE(forge().initialize(backend()));

        EXPECT_EQ(forge().get_default_linear(), forge().get_or_create(SamplerConfig {}));
        EXPECT_EQ(forge().get_sampler_count(), 1U);
    }

    TEST_F(SamplerForgeTest, AnisotropicRequestsAreClampedIntoOneToSixteen)
    {
        ASSERT_TRUE(forge().initialize(backend()));

        const vk::Sampler sixteen = forge().get_anisotropic();

        ASSERT_TRUE(sixteen);
        EXPECT_EQ(forge().get_anisotropic(16.0F), sixteen);
        EXPECT_EQ(forge().get_anisotropic(100.0F), sixteen);
        EXPECT_EQ(forge().get_sampler_count(), 1U);
    }

    TEST_F(SamplerForgeTest, AnisotropicRequestBelowOneIsTheLinearDefault)
    {
        ASSERT_TRUE(forge().initialize(backend()));

        EXPECT_EQ(forge().get_anisotropic(0.25F), forge().get_default_linear());
        EXPECT_EQ(forge().get_sampler_count(), 1U);
    }

    TEST_F(SamplerForgeTest, DifferentAnisotropyLevelsAreDifferentSamplers)
    {
        ASSERT_TRUE(forge().initialize(backend()));

        const vk::Sampler four = forge().get_anisotropic(4.0F);
        const vk::Sampler eight = forge().get_anisotropic(8.0F);

        ASSERT_TRUE(four);
        ASSERT_TRUE(eight);
        EXPECT_NE(four, eight);
    }

    // ------------------------------------------------------------------
    // Destruction
    // ------------------------------------------------------------------

    TEST_F(SamplerForgeTest, DestroySamplerRemovesItFromTheCacheAndTheConfigCanBeRecreated)
    {
        ASSERT_TRUE(forge().initialize(backend()));
        const vk::Sampler sampler = forge().get_default_linear();
        ASSERT_TRUE(sampler);

        forge().destroy_sampler(sampler);

        EXPECT_EQ(forge().get_sampler_count(), 0U);
        EXPECT_TRUE(forge().get_default_linear());
        EXPECT_EQ(forge().get_sampler_count(), 1U);
    }

    TEST_F(SamplerForgeTest, DestroySamplerLeavesTheOtherCachedSamplersUntouched)
    {
        ASSERT_TRUE(forge().initialize(backend()));
        const vk::Sampler linear = forge().get_default_linear();
        const vk::Sampler nearest = forge().get_default_nearest();
        ASSERT_TRUE(linear);
        ASSERT_TRUE(nearest);

        forge().destroy_sampler(linear);

        EXPECT_EQ(forge().get_sampler_count(), 1U);
        EXPECT_EQ(forge().get_default_nearest(), nearest);
        EXPECT_EQ(forge().get_sampler_count(), 1U);
    }

    TEST_F(SamplerForgeTest, DestroySamplerIgnoresANullHandle)
    {
        ASSERT_TRUE(forge().initialize(backend()));
        ASSERT_TRUE(forge().get_default_linear());

        forge().destroy_sampler(vk::Sampler {});

        EXPECT_EQ(forge().get_sampler_count(), 1U);
    }

} // namespace
} // namespace MayaFlux::Test
