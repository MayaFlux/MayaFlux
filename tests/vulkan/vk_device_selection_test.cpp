#include "gtest/gtest.h"

#include <algorithm>
#include <cctype>

#include "../support/scoped_env.hpp"
#include "../support/vulkan_test_support.hpp"

#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKContext.hpp"
#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKDevice.hpp"

namespace MayaFlux::Test {
namespace {

    using Core::GlobalGraphicsConfig;
    using Core::GraphicsBackendInfo;
    using Core::QueueFamilyIndices;
    using Core::VulkanBackend;

    struct Candidate {
        std::string name;
        std::string uuid;
    };

    std::string uuid_hex(vk::PhysicalDevice device)
    {
        const auto chain = device.getProperties2<vk::PhysicalDeviceProperties2, vk::PhysicalDeviceVulkan11Properties>();
        const auto& uuid = chain.get<vk::PhysicalDeviceVulkan11Properties>().deviceUUID;
        std::string out;
        for (const auto byte : uuid) {
            out += std::format("{:02x}", static_cast<unsigned>(byte));
        }
        return out;
    }

    std::vector<Candidate> enumerate_devices()
    {
        Core::VKInstance instance;
        if (!instance.initialize(false, {})) {
            return {};
        }
        std::vector<Candidate> candidates;
        for (const auto& device : instance.get_instance().enumeratePhysicalDevices()) {
            candidates.push_back({ std::string(device.getProperties().deviceName.data()), uuid_hex(device) });
        }
        return candidates;
    }

    bool try_initialize(VulkanBackend& backend, const GlobalGraphicsConfig& config)
    {
        try {
            return backend.initialize(config);
        } catch (const std::exception&) {
            return false;
        }
    }

    std::optional<std::string> default_device_uuid()
    {
        VulkanBackend backend;
        if (!try_initialize(backend, headless_graphics_config())) {
            return std::nullopt;
        }
        return uuid_hex(backend.get_context().get_physical_device());
    }

    std::optional<size_t> default_device_index(const std::vector<Candidate>& candidates)
    {
        const auto uuid = default_device_uuid();
        if (!uuid) {
            return std::nullopt;
        }
        const auto it = std::ranges::find(candidates, *uuid, &Candidate::uuid);
        if (it == candidates.end()) {
            return std::nullopt;
        }
        return static_cast<size_t>(it - candidates.begin());
    }

    std::string lowered(std::string text)
    {
        std::ranges::transform(text, text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return text;
    }

    std::string uppered(std::string text)
    {
        std::ranges::transform(text, text.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        return text;
    }

    class VKDevicePreferenceTest : public ::testing::TestWithParam<GraphicsBackendInfo::DevicePreference> { };

}

TEST(QueueFamilyIndicesTest, EmptyIndicesAreIncompleteAndPresentNowhere)
{
    const QueueFamilyIndices indices;

    EXPECT_FALSE(indices.is_complete());
    EXPECT_FALSE(indices.can_present(0));
    EXPECT_FALSE(indices.preferred_present_family().has_value());
}

TEST(QueueFamilyIndicesTest, GraphicsFamilyMakesTheIndicesComplete)
{
    QueueFamilyIndices indices;
    indices.graphics_family = 2;
    EXPECT_TRUE(indices.is_complete());
}

TEST(QueueFamilyIndicesTest, PresentMaskIsReadPerFamilyAndBoundedByItsWidth)
{
    QueueFamilyIndices indices;
    indices.present_family_mask = (1U << 0) | (1U << 5) | (1U << 31);

    EXPECT_TRUE(indices.can_present(0));
    EXPECT_FALSE(indices.can_present(1));
    EXPECT_TRUE(indices.can_present(5));
    EXPECT_TRUE(indices.can_present(31));
    EXPECT_FALSE(indices.can_present(32));
    EXPECT_FALSE(indices.can_present(1000));
}

TEST(QueueFamilyIndicesTest, PreferredPresentFamilyIsGraphicsWhenItCanPresent)
{
    QueueFamilyIndices indices;
    indices.graphics_family = 3;
    indices.present_family_mask = (1U << 1) | (1U << 3);

    EXPECT_EQ(indices.preferred_present_family(), std::optional<uint32_t> { 3 });
}

TEST(QueueFamilyIndicesTest, PreferredPresentFamilyFallsBackToTheLowestPresentingFamily)
{
    QueueFamilyIndices indices;
    indices.graphics_family = 0;
    indices.present_family_mask = (1U << 4) | (1U << 2);

    EXPECT_EQ(indices.preferred_present_family(), std::optional<uint32_t> { 2 });
}

TEST(QueueFamilyIndicesTest, PreferredPresentFamilyIsEmptyWithoutAnyPresentingFamily)
{
    QueueFamilyIndices indices;
    indices.graphics_family = 0;

    EXPECT_FALSE(indices.preferred_present_family().has_value());
}

TEST(VKDeviceSelectionTest, SelectedQueueFamiliesAreCompleteAndCapable)
{
    REQUIRE_VULKAN();
    VulkanBackend backend;
    ASSERT_TRUE(backend.initialize(headless_graphics_config()));

    const auto& families = backend.get_context().get_queue_families();
    const auto properties = backend.get_context().get_physical_device().getQueueFamilyProperties();

    ASSERT_TRUE(families.is_complete());
    ASSERT_TRUE(families.compute_family.has_value());
    ASSERT_TRUE(families.transfer_family.has_value());
    EXPECT_TRUE(properties[families.graphics_family.value()].queueFlags & vk::QueueFlagBits::eGraphics);
    EXPECT_TRUE(properties[families.compute_family.value()].queueFlags & vk::QueueFlagBits::eCompute);
    EXPECT_TRUE(properties[families.transfer_family.value()].queueFlags & vk::QueueFlagBits::eTransfer);
}

TEST(VKDeviceSelectionTest, EnumerationFindsTheDeviceThatDefaultSelectionPicks)
{
    REQUIRE_VULKAN();
    const auto candidates = enumerate_devices();
    ASSERT_FALSE(candidates.empty());

    EXPECT_TRUE(default_device_index(candidates).has_value());
}

TEST(VKDeviceSelectionTest, StrictIndexSelectionPicksThatDeviceOrRefuses)
{
    REQUIRE_VULKAN();
    const auto candidates = enumerate_devices();
    ASSERT_FALSE(candidates.empty());

    size_t selected = 0;
    for (size_t i = 0; i < candidates.size(); ++i) {
        VulkanBackend backend;
        auto config = headless_graphics_config();
        config.backend_info.device_index = static_cast<int32_t>(i);
        config.backend_info.strict_device_selection = true;

        if (try_initialize(backend, config)) {
            ++selected;
            EXPECT_EQ(uuid_hex(backend.get_context().get_physical_device()), candidates[i].uuid) << "index " << i;
        }
    }
    EXPECT_GE(selected, 1U);
}

TEST(VKDeviceSelectionTest, StrictUuidSelectionPicksThatDeviceOrRefuses)
{
    REQUIRE_VULKAN();
    const auto candidates = enumerate_devices();
    ASSERT_FALSE(candidates.empty());

    size_t selected = 0;
    for (const auto& candidate : candidates) {
        VulkanBackend backend;
        auto config = headless_graphics_config();
        config.backend_info.device_uuid = candidate.uuid;
        config.backend_info.strict_device_selection = true;

        if (try_initialize(backend, config)) {
            ++selected;
            EXPECT_EQ(uuid_hex(backend.get_context().get_physical_device()), candidate.uuid) << candidate.name;
        }
    }
    EXPECT_GE(selected, 1U);
}

TEST(VKDeviceSelectionTest, NameSelectionIgnoresCaseAndAcceptsSubstrings)
{
    REQUIRE_VULKAN();
    const auto candidates = enumerate_devices();
    const auto chosen = default_device_index(candidates);
    ASSERT_TRUE(chosen.has_value());
    const auto& target = candidates[*chosen];

    for (const auto& needle : { uppered(target.name), lowered(target.name), lowered(target.name.substr(0, std::min<size_t>(6, target.name.size()))) }) {
        VulkanBackend backend;
        auto config = headless_graphics_config();
        config.backend_info.device_name = needle;
        config.backend_info.strict_device_selection = true;

        ASSERT_TRUE(try_initialize(backend, config)) << needle;
        EXPECT_EQ(std::string(backend.get_context().get_physical_device().getProperties().deviceName.data()), target.name) << needle;
    }
}

TEST(VKDeviceSelectionTest, StrictSelectorsThatMatchNothingRefuseToInitialize)
{
    REQUIRE_VULKAN();
    const auto candidates = enumerate_devices();

    auto by_index = headless_graphics_config();
    by_index.backend_info.device_index = static_cast<int32_t>(candidates.size() + 7);
    by_index.backend_info.strict_device_selection = true;
    auto by_uuid = headless_graphics_config();
    by_uuid.backend_info.device_uuid = "ffffffffffffffffffffffffffffffff";
    by_uuid.backend_info.strict_device_selection = true;

    VulkanBackend first;
    VulkanBackend second;
    EXPECT_FALSE(try_initialize(first, by_index));
    EXPECT_FALSE(try_initialize(second, by_uuid));
}

TEST(VKDeviceSelectionTest, LenientSelectorsThatMatchNothingFallBackToScoring)
{
    REQUIRE_VULKAN();
    const auto candidates = enumerate_devices();
    const auto expected = default_device_uuid();
    ASSERT_TRUE(expected.has_value());

    VulkanBackend backend;
    auto config = headless_graphics_config();
    config.backend_info.device_index = static_cast<int32_t>(candidates.size() + 7);
    config.backend_info.device_uuid = "ffffffffffffffffffffffffffffffff";
    config.backend_info.device_name = "no such gpu exists";

    ASSERT_TRUE(try_initialize(backend, config));
    EXPECT_EQ(uuid_hex(backend.get_context().get_physical_device()), *expected);
}

TEST(VKDeviceSelectionTest, EnvironmentIndexOverridesTheConfiguredSelectors)
{
    REQUIRE_VULKAN();
    const auto candidates = enumerate_devices();
    const auto chosen = default_device_index(candidates);
    ASSERT_TRUE(chosen.has_value());
    const ScopedEnv env("MAYAFLUX_GPU", std::to_string(*chosen).c_str());

    VulkanBackend backend;
    auto config = headless_graphics_config();
    config.backend_info.device_name = "no such gpu exists";
    config.backend_info.device_uuid = "ffffffffffffffffffffffffffffffff";
    config.backend_info.strict_device_selection = true;

    ASSERT_TRUE(try_initialize(backend, config));
    EXPECT_EQ(uuid_hex(backend.get_context().get_physical_device()), candidates[*chosen].uuid);
}

TEST(VKDeviceSelectionTest, EnvironmentNameOverridesTheConfiguredIndex)
{
    REQUIRE_VULKAN();
    const auto candidates = enumerate_devices();
    const auto chosen = default_device_index(candidates);
    ASSERT_TRUE(chosen.has_value());
    const std::string needle = lowered(candidates[*chosen].name.substr(0, std::min<size_t>(6, candidates[*chosen].name.size())));
    const ScopedEnv env("MAYAFLUX_GPU", needle.c_str());

    VulkanBackend backend;
    auto config = headless_graphics_config();
    config.backend_info.device_index = static_cast<int32_t>(candidates.size() + 7);
    config.backend_info.strict_device_selection = true;

    ASSERT_TRUE(try_initialize(backend, config));
    EXPECT_EQ(std::string(backend.get_context().get_physical_device().getProperties().deviceName.data()), candidates[*chosen].name);
}

TEST(VKDeviceSelectionTest, EnvironmentSelectorThatMatchesNothingRespectsStrictness)
{
    REQUIRE_VULKAN();
    const ScopedEnv env("MAYAFLUX_GPU", "no such gpu exists");

    VulkanBackend lenient;
    EXPECT_TRUE(try_initialize(lenient, headless_graphics_config()));

    VulkanBackend strict;
    auto config = headless_graphics_config();
    config.backend_info.strict_device_selection = true;
    EXPECT_FALSE(try_initialize(strict, config));
}

TEST(VKDeviceSelectionTest, EmptyEnvironmentSelectorIsIgnored)
{
    REQUIRE_VULKAN();
    const ScopedEnv env("MAYAFLUX_GPU", "");

    VulkanBackend backend;
    auto config = headless_graphics_config();
    config.backend_info.strict_device_selection = true;
    config.backend_info.device_name = "no such gpu exists";

    EXPECT_FALSE(try_initialize(backend, config));
}

TEST_P(VKDevicePreferenceTest, EveryPreferenceStillSelectsAViableDevice)
{
    REQUIRE_VULKAN();
    const auto candidates = enumerate_devices();
    VulkanBackend backend;
    auto config = headless_graphics_config();
    config.backend_info.device_preference = GetParam();

    ASSERT_TRUE(try_initialize(backend, config));
    EXPECT_NE(std::ranges::find(candidates, uuid_hex(backend.get_context().get_physical_device()), &Candidate::uuid),
        candidates.end());
}

INSTANTIATE_TEST_SUITE_P(Preferences, VKDevicePreferenceTest,
    ::testing::Values(
        GraphicsBackendInfo::DevicePreference::AUTO,
        GraphicsBackendInfo::DevicePreference::DISCRETE,
        GraphicsBackendInfo::DevicePreference::INTEGRATED,
        GraphicsBackendInfo::DevicePreference::VIRTUAL,
        GraphicsBackendInfo::DevicePreference::EXTERNAL));

TEST(VKDeviceSelectionTest, DisablingTheComputeQueueSharesTheGraphicsQueue)
{
    REQUIRE_VULKAN();
    VulkanBackend backend;
    auto config = headless_graphics_config();
    config.backend_info.enable_compute_queue = false;

    ASSERT_TRUE(try_initialize(backend, config));
    EXPECT_EQ(backend.get_context().get_compute_queue(), backend.get_context().get_graphics_queue());
}

TEST(VKDeviceSelectionTest, EnabledComputeQueueUsesTheComputeFamily)
{
    REQUIRE_VULKAN();
    VulkanBackend backend;
    ASSERT_TRUE(backend.initialize(headless_graphics_config()));

    const auto& families = backend.get_context().get_queue_families();
    if (families.compute_family == families.graphics_family) {
        EXPECT_EQ(backend.get_context().get_compute_queue(), backend.get_context().get_graphics_queue());
    } else {
        EXPECT_NE(backend.get_context().get_compute_queue(), backend.get_context().get_graphics_queue());
    }
}

TEST(VKDeviceSelectionTest, TransferQueueIsSeparateOnlyWhenRequestedAndAvailable)
{
    REQUIRE_VULKAN();
    {
        VulkanBackend backend;
        ASSERT_TRUE(backend.initialize(headless_graphics_config()));
        EXPECT_EQ(backend.get_context().get_transfer_queue(), backend.get_context().get_graphics_queue());
    }

    VulkanBackend backend;
    auto config = headless_graphics_config();
    config.backend_info.enable_transfer_queue = true;
    ASSERT_TRUE(try_initialize(backend, config));

    const auto& families = backend.get_context().get_queue_families();
    if (families.transfer_family == families.graphics_family) {
        EXPECT_EQ(backend.get_context().get_transfer_queue(), backend.get_context().get_graphics_queue());
    } else {
        EXPECT_NE(backend.get_context().get_transfer_queue(), backend.get_context().get_graphics_queue());
    }
}

}
