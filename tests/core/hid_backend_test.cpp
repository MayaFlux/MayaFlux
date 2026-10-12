#include "gtest/gtest.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "MayaFlux/Core/Backends/Input/HIDBackend.hpp"

namespace MayaFlux::Test {
namespace {

    using Core::HIDBackend;
    using Core::HIDDeviceFilter;

    constexpr uint16_t k_unused_vid = 0xFFFF;
    constexpr uint16_t k_unused_pid = 0xFFFF;
    constexpr uint32_t k_unknown_id = 0xFFFFFFF0U;

    HIDDeviceFilter unmatched_filter()
    {
        return HIDDeviceFilter::device(k_unused_vid, k_unused_pid);
    }

    std::map<std::string, uint32_t> ids_by_path(const HIDBackend& backend)
    {
        std::map<std::string, uint32_t> result;
        for (const auto& device : backend.get_devices()) {
            const auto ext = backend.get_device_info_ext(device.id);
            if (ext)
                result[ext->path] = device.id;
        }
        return result;
    }

    constexpr uint16_t k_page_generic_desktop = 0x01;

    bool is_pointer_or_keyboard(const Core::HIDDeviceInfoExt& info)
    {
        if (info.usage_page != k_page_generic_desktop)
            return false;
        return info.usage == 0x01 || info.usage == 0x02 || info.usage == 0x06;
    }

    bool is_game_controller(const Core::HIDDeviceInfoExt& info)
    {
        return info.usage_page == k_page_generic_desktop
            && (info.usage == 0x04 || info.usage == 0x05);
    }

    std::optional<uint32_t> openable_non_pointer(HIDBackend& backend)
    {
        std::vector<Core::HIDDeviceInfoExt> candidates;
        for (const auto& device : backend.get_devices()) {
            const auto ext = backend.get_device_info_ext(device.id);
            if (ext && !is_pointer_or_keyboard(*ext))
                candidates.push_back(*ext);
        }

        std::ranges::sort(candidates, [](const auto& a, const auto& b) {
            const bool a_pad = is_game_controller(a);
            const bool b_pad = is_game_controller(b);
            if (a_pad != b_pad)
                return a_pad;
            return a.id < b.id;
        });

        for (const auto& candidate : candidates) {
            if (backend.open_device(candidate.id)) {
                std::cout << "[  HID   ] opened " << candidate.name << " (id " << candidate.id
                          << ", page 0x" << std::hex << candidate.usage_page
                          << " usage 0x" << candidate.usage << std::dec << ")\n";
                return candidate.id;
            }
        }
        return std::nullopt;
    }

#define REQUIRE_HID_DEVICE(backend)                        \
    do {                                                   \
        if ((backend).get_devices().empty())               \
            GTEST_SKIP() << "No HID device is enumerable"; \
    } while (0)

    // ------------------------------------------------------------------
    // Device filter
    // ------------------------------------------------------------------

    struct FilterCase {
        const char* name;
        HIDDeviceFilter filter;
        uint16_t vid;
        uint16_t pid;
        uint16_t usage_page;
        uint16_t usage;
        bool expected;
    };

    class HIDDeviceFilterTest : public ::testing::TestWithParam<FilterCase> { };

    TEST_P(HIDDeviceFilterTest, MatchesOnlyWhenEveryConstrainedFieldAgrees)
    {
        const auto& param = GetParam();
        EXPECT_EQ(param.filter.matches(param.vid, param.pid, param.usage_page, param.usage), param.expected);
    }

    INSTANTIATE_TEST_SUITE_P(
        HIDFilters,
        HIDDeviceFilterTest,
        ::testing::Values(
            FilterCase { "AnyMatchesEverything", HIDDeviceFilter::any(), 0x1234, 0x5678, 0x0D, 0x02, true },
            FilterCase { "AnyMatchesZeroes", HIDDeviceFilter::any(), 0, 0, 0, 0, true },
            FilterCase { "KeyboardMatchesKeyboardUsage", HIDDeviceFilter::keyboard(), 0x1, 0x2, 0x01, 0x06, true },
            FilterCase { "KeyboardRejectsMouseUsage", HIDDeviceFilter::keyboard(), 0x1, 0x2, 0x01, 0x02, false },
            FilterCase { "KeyboardRejectsOtherPage", HIDDeviceFilter::keyboard(), 0x1, 0x2, 0x0D, 0x06, false },
            FilterCase { "KeyboardRejectsDefaultedUsage", HIDDeviceFilter::keyboard(), 0x1, 0x2, 0, 0, false },
            FilterCase { "MouseMatchesMouseUsage", HIDDeviceFilter::mouse(), 0x1, 0x2, 0x01, 0x02, true },
            FilterCase { "MouseRejectsKeyboardUsage", HIDDeviceFilter::mouse(), 0x1, 0x2, 0x01, 0x06, false },
            FilterCase { "ControllerMatchesGamepadUsage", HIDDeviceFilter::controller(), 0x1, 0x2, 0x01, 0x05, true },
            FilterCase { "ControllerRejectsJoystickUsage", HIDDeviceFilter::controller(), 0x1, 0x2, 0x01, 0x04, false },
            FilterCase { "SpecializedMatchesJoystickUsage", HIDDeviceFilter::specialized(), 0x1, 0x2, 0x01, 0x04, true },
            FilterCase { "SpecializedRejectsGamepadUsage", HIDDeviceFilter::specialized(), 0x1, 0x2, 0x01, 0x05, false },
            FilterCase { "DeviceMatchesVendorAndProductOnAnyUsage", HIDDeviceFilter::device(0x1234, 0x5678), 0x1234, 0x5678, 0x0D, 0x42, true },
            FilterCase { "DeviceRejectsOtherVendor", HIDDeviceFilter::device(0x1234, 0x5678), 0x4321, 0x5678, 0x01, 0x06, false },
            FilterCase { "DeviceRejectsOtherProduct", HIDDeviceFilter::device(0x1234, 0x5678), 0x1234, 0x8765, 0x01, 0x06, false }),
        [](const ::testing::TestParamInfo<FilterCase>& info) { return std::string(info.param.name); });

    // ------------------------------------------------------------------
    // Identity and unopened state
    // ------------------------------------------------------------------

    TEST(HIDBackendTest, ReportsIdentityBeforeInitialization)
    {
        HIDBackend backend;

        EXPECT_EQ(backend.get_type(), Core::InputType::HID);
        EXPECT_FALSE(backend.get_name().empty());
        EXPECT_NE(backend.get_version().find('.'), std::string::npos);
        EXPECT_FALSE(backend.is_initialized());
        EXPECT_FALSE(backend.is_running());
    }

    TEST(HIDBackendTest, QueriesAndIoBeforeInitializationAreInert)
    {
        HIDBackend backend;
        const std::array<uint8_t, 2> report { 0x01, 0x02 };
        std::array<uint8_t, 8> buffer {};

        EXPECT_EQ(backend.refresh_devices(), 0U);
        EXPECT_TRUE(backend.get_devices().empty());
        EXPECT_TRUE(backend.get_open_devices().empty());
        EXPECT_FALSE(backend.open_device(1));
        EXPECT_FALSE(backend.is_device_open(1));
        EXPECT_EQ(backend.get_device_info_ext(1), std::nullopt);
        EXPECT_EQ(backend.write(1, report), -1);
        EXPECT_EQ(backend.send_feature_report(1, report), -1);
        EXPECT_EQ(backend.get_feature_report(1, 0x01, buffer), -1);
    }

    TEST(HIDBackendTest, UnknownIdsAreRejectedOrIgnoredAfterInitialization)
    {
        HIDBackend backend;
        ASSERT_TRUE(backend.initialize());
        const std::array<uint8_t, 2> report { 0x01, 0x02 };
        std::array<uint8_t, 8> buffer {};

        EXPECT_FALSE(backend.open_device(k_unknown_id));
        EXPECT_FALSE(backend.is_device_open(k_unknown_id));
        EXPECT_NO_THROW(backend.close_device(k_unknown_id));
        EXPECT_EQ(backend.get_device_info_ext(k_unknown_id), std::nullopt);
        EXPECT_EQ(backend.write(k_unknown_id, report), -1);
        EXPECT_EQ(backend.send_feature_report(k_unknown_id, report), -1);
        EXPECT_EQ(backend.get_feature_report(k_unknown_id, 0x01, buffer), -1);
    }

    // ------------------------------------------------------------------
    // Lifecycle
    // ------------------------------------------------------------------

    TEST(HIDBackendTest, InitializeIsIdempotentAndShutdownReleases)
    {
        HIDBackend backend;

        ASSERT_TRUE(backend.initialize());
        EXPECT_TRUE(backend.is_initialized());
        EXPECT_TRUE(backend.initialize());

        backend.shutdown();
        EXPECT_FALSE(backend.is_initialized());
        EXPECT_TRUE(backend.get_devices().empty());
        EXPECT_TRUE(backend.get_open_devices().empty());
        EXPECT_NO_THROW(backend.shutdown());
    }

    TEST(HIDBackendTest, StartRequiresInitialization)
    {
        HIDBackend backend;

        backend.start();

        EXPECT_FALSE(backend.is_running());
    }

    TEST(HIDBackendTest, StartAndStopAreIdempotent)
    {
        HIDBackend backend;
        ASSERT_TRUE(backend.initialize());

        EXPECT_NO_THROW(backend.stop());

        backend.start();
        EXPECT_TRUE(backend.is_running());
        EXPECT_NO_THROW(backend.start());
        EXPECT_TRUE(backend.is_running());

        backend.stop();
        EXPECT_FALSE(backend.is_running());
        EXPECT_NO_THROW(backend.stop());
    }

    TEST(HIDBackendTest, ShutdownStopsARunningBackend)
    {
        HIDBackend backend;
        ASSERT_TRUE(backend.initialize());
        backend.start();

        backend.shutdown();

        EXPECT_FALSE(backend.is_running());
        EXPECT_FALSE(backend.is_initialized());
    }

    TEST(HIDBackendTest, DestructionWhileRunningIsClean)
    {
        auto backend = std::make_unique<HIDBackend>();
        ASSERT_TRUE(backend->initialize());
        backend->start();

        EXPECT_NO_THROW(backend.reset());
    }

    TEST(HIDBackendTest, RestartsRepeatedlyInOneProcess)
    {
        HIDBackend backend;

        for (int i = 0; i < 3; ++i) {
            ASSERT_TRUE(backend.initialize()) << i;
            backend.start();
            EXPECT_TRUE(backend.is_running()) << i;
            backend.stop();
            backend.shutdown();
            EXPECT_FALSE(backend.is_initialized()) << i;
        }
    }

    TEST(HIDBackendTest, SequentialBackendsInitializeIndependently)
    {
        {
            HIDBackend first;
            ASSERT_TRUE(first.initialize());
        }

        HIDBackend second;
        EXPECT_TRUE(second.initialize());
    }

    TEST(HIDBackendTest, ReinitializationRepopulatesDevices)
    {
        HIDBackend backend;
        ASSERT_TRUE(backend.initialize());
        const size_t before = backend.get_devices().size();

        backend.shutdown();
        ASSERT_TRUE(backend.initialize());

        EXPECT_EQ(backend.get_devices().size(), before);
    }

    TEST(HIDBackendTest, CustomBufferAndTimeoutConfigInitializes)
    {
        HIDBackend::Config config;
        config.read_buffer_size = 16;
        config.poll_timeout_ms = 1;
        HIDBackend backend(config);

        EXPECT_TRUE(backend.initialize());
        backend.start();
        EXPECT_TRUE(backend.is_running());
        backend.stop();
    }

    // ------------------------------------------------------------------
    // Enumeration
    // ------------------------------------------------------------------

    TEST(HIDBackendTest, DeviceCallbackAnnouncesEveryEnumeratedDeviceOnInitialize)
    {
        HIDBackend backend;
        std::vector<Core::InputDeviceInfo> announced;
        std::vector<bool> connected;
        backend.set_device_callback([&](const Core::InputDeviceInfo& info, bool is_connected) {
            announced.push_back(info);
            connected.push_back(is_connected);
        });

        ASSERT_TRUE(backend.initialize());

        EXPECT_EQ(announced.size(), backend.get_devices().size());
        for (const bool flag : connected)
            EXPECT_TRUE(flag);
        for (const auto& info : announced)
            EXPECT_EQ(info.backend_type, Core::InputType::HID);
    }

    TEST(HIDBackendTest, EnumeratedDevicesHaveUniqueIdsAndDescribeThemselves)
    {
        HIDBackend backend;
        ASSERT_TRUE(backend.initialize());
        REQUIRE_HID_DEVICE(backend);

        std::set<uint32_t> ids;
        for (const auto& device : backend.get_devices()) {
            EXPECT_TRUE(ids.insert(device.id).second) << device.id;
            EXPECT_FALSE(device.name.empty()) << device.id;
            EXPECT_EQ(device.backend_type, Core::InputType::HID);

            const auto ext = backend.get_device_info_ext(device.id);
            ASSERT_TRUE(ext.has_value()) << device.id;
            EXPECT_EQ(ext->id, device.id);
            EXPECT_EQ(ext->backend_type, Core::InputType::HID);
            EXPECT_TRUE(ext->is_connected);
            EXPECT_FALSE(ext->path.empty());
        }
    }

    TEST(HIDBackendTest, RefreshKeepsIdsStableForUnchangedPaths)
    {
        HIDBackend backend;
        ASSERT_TRUE(backend.initialize());
        REQUIRE_HID_DEVICE(backend);
        const auto before = ids_by_path(backend);

        const size_t count = backend.refresh_devices();

        EXPECT_EQ(count, before.size());
        EXPECT_EQ(ids_by_path(backend), before);
    }

    // ------------------------------------------------------------------
    // Filters
    // ------------------------------------------------------------------

    TEST(HIDBackendTest, ConfiguredFilterThatMatchesNothingEnumeratesNothing)
    {
        HIDBackend::Config config;
        config.filters.push_back(unmatched_filter());
        HIDBackend backend(config);

        ASSERT_TRUE(backend.initialize());

        EXPECT_TRUE(backend.get_devices().empty());
        EXPECT_EQ(backend.refresh_devices(), 0U);
    }

    TEST(HIDBackendTest, ConfiguredDeviceFilterKeepsOnlyThatVendorAndProduct)
    {
        uint16_t vid = 0;
        uint16_t pid = 0;
        {
            HIDBackend probe;
            ASSERT_TRUE(probe.initialize());
            REQUIRE_HID_DEVICE(probe);
            const auto ext = probe.get_device_info_ext(probe.get_devices().front().id);
            ASSERT_TRUE(ext.has_value());
            vid = ext->vendor_id;
            pid = ext->product_id;
        }

        HIDBackend::Config config;
        config.filters.push_back(HIDDeviceFilter::device(vid, pid));
        HIDBackend backend(config);
        ASSERT_TRUE(backend.initialize());

        ASSERT_FALSE(backend.get_devices().empty());
        for (const auto& device : backend.get_devices()) {
            const auto ext = backend.get_device_info_ext(device.id);
            ASSERT_TRUE(ext.has_value());
            EXPECT_EQ(ext->vendor_id, vid);
            EXPECT_EQ(ext->product_id, pid);
        }
    }

    TEST(HIDBackendTest, AddedFilterTakesEffectOnRefreshAndClearRestoresDevices)
    {
        HIDBackend backend;
        ASSERT_TRUE(backend.initialize());
        const size_t all = backend.get_devices().size();

        backend.add_device_filter(unmatched_filter());
        EXPECT_EQ(backend.refresh_devices(), 0U);
        EXPECT_TRUE(backend.get_devices().empty());

        backend.clear_device_filters();
        EXPECT_EQ(backend.refresh_devices(), all);
        EXPECT_EQ(backend.get_devices().size(), all);
    }

    TEST(HIDBackendTest, FilterRemovalAnnouncesDisconnectForEachExcludedDevice)
    {
        HIDBackend backend;
        ASSERT_TRUE(backend.initialize());
        REQUIRE_HID_DEVICE(backend);
        const size_t all = backend.get_devices().size();

        size_t disconnects = 0;
        backend.set_device_callback([&](const Core::InputDeviceInfo&, bool is_connected) {
            if (!is_connected)
                ++disconnects;
        });

        backend.add_device_filter(unmatched_filter());
        backend.refresh_devices();

        EXPECT_EQ(disconnects, all);
    }

    // ------------------------------------------------------------------
    // Opening devices
    // ------------------------------------------------------------------

    TEST(HIDBackendTest, OpenDeviceTracksOpenStateAndClosesAndReopens)
    {
        HIDBackend backend;
        ASSERT_TRUE(backend.initialize());
        REQUIRE_HID_DEVICE(backend);
        const auto id = openable_non_pointer(backend);
        if (!id)
            GTEST_SKIP() << "No openable HID device other than a pointer or keyboard";

        EXPECT_TRUE(backend.is_device_open(*id));
        const auto open = backend.get_open_devices();
        EXPECT_NE(std::ranges::find(open, *id), open.end());
        EXPECT_TRUE(backend.open_device(*id));

        backend.close_device(*id);
        EXPECT_FALSE(backend.is_device_open(*id));
        const auto remaining = backend.get_open_devices();
        EXPECT_EQ(std::ranges::find(remaining, *id), remaining.end());
        EXPECT_NO_THROW(backend.close_device(*id));

        EXPECT_TRUE(backend.open_device(*id));
        EXPECT_TRUE(backend.is_device_open(*id));
    }

    TEST(HIDBackendTest, ShutdownClosesOpenDevices)
    {
        HIDBackend backend;
        ASSERT_TRUE(backend.initialize());
        REQUIRE_HID_DEVICE(backend);
        const auto id = openable_non_pointer(backend);
        if (!id)
            GTEST_SKIP() << "No openable HID device other than a pointer or keyboard";

        backend.shutdown();

        EXPECT_FALSE(backend.is_device_open(*id));
        EXPECT_TRUE(backend.get_open_devices().empty());
    }

    TEST(HIDBackendTest, PollingAnOpenDeviceDeliversWellFormedReportsOnly)
    {
        HIDBackend backend;
        ASSERT_TRUE(backend.initialize());
        REQUIRE_HID_DEVICE(backend);
        const auto id = openable_non_pointer(backend);
        if (!id)
            GTEST_SKIP() << "No openable HID device other than a pointer or keyboard";

        std::mutex mutex;
        std::vector<Core::InputValue> received;
        backend.set_input_callback([&](const Core::InputValue& value) {
            std::lock_guard lock(mutex);
            received.push_back(value);
        });

        backend.start();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        backend.stop();

        EXPECT_FALSE(backend.is_running());
        EXPECT_TRUE(backend.is_device_open(*id));

        std::lock_guard lock(mutex);
        for (const auto& value : received) {
            EXPECT_EQ(value.type, Core::InputValue::Type::BYTES);
            EXPECT_EQ(value.source_type, Core::InputType::HID);
            EXPECT_EQ(value.device_id, *id);
        }
    }

    TEST(HIDBackendTest, RestartingPollingKeepsOpenDevices)
    {
        HIDBackend backend;
        ASSERT_TRUE(backend.initialize());
        REQUIRE_HID_DEVICE(backend);
        const auto id = openable_non_pointer(backend);
        if (!id)
            GTEST_SKIP() << "No openable HID device other than a pointer or keyboard";

        for (int i = 0; i < 3; ++i) {
            backend.start();
            EXPECT_TRUE(backend.is_running()) << i;
            backend.stop();
            EXPECT_FALSE(backend.is_running()) << i;
        }

        EXPECT_TRUE(backend.is_device_open(*id));
    }

} // namespace
} // namespace MayaFlux::Test
