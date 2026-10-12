#include "gtest/gtest.h"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "MayaFlux/Core/Backends/Input/TabletBackend.hpp"

namespace MayaFlux::Test {
namespace {

    using Core::TabletAxes;
    using Core::TabletBackend;
    using Core::TabletField;
    using Core::TabletFrame;
    using Core::TabletLayout;
    using Core::TabletToolType;

    constexpr uint16_t k_page_generic = 0x01;
    constexpr uint16_t k_page_digitizer = 0x0D;

    TabletLayout layout_of(const std::vector<uint8_t>& descriptor)
    {
        return TabletBackend::parse_descriptor(descriptor);
    }

    const TabletField* find_field(const TabletLayout& layout, uint16_t page, uint16_t usage)
    {
        for (const auto& field : layout.fields) {
            if (field.usage_page == page && field.usage == usage)
                return &field;
        }
        return nullptr;
    }

    std::vector<uint8_t> pen_descriptor()
    {
        return {
            0x05, 0x0D, 0x09, 0x02, 0xA1, 0x01, 0x85, 0x01, 0x09, 0x20, 0xA1, 0x00,
            0x09, 0x42, 0x09, 0x44, 0x09, 0x3C, 0x09, 0x32,
            0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x04, 0x81, 0x02,
            0x95, 0x04, 0x81, 0x03,
            0x05, 0x01, 0x09, 0x30, 0x09, 0x31,
            0x15, 0x00, 0x26, 0xFF, 0x7F, 0x75, 0x10, 0x95, 0x02, 0x81, 0x02,
            0x05, 0x0D, 0x09, 0x30,
            0x15, 0x00, 0x26, 0xFF, 0x03, 0x75, 0x10, 0x95, 0x01, 0x81, 0x02,
            0x09, 0x3D, 0x09, 0x3E,
            0x15, 0x81, 0x25, 0x7F, 0x75, 0x08, 0x95, 0x02, 0x81, 0x02,
            0xC0, 0xC0
        };
    }

    std::vector<uint8_t> serial_rotation_wheel_descriptor()
    {
        return {
            0x05, 0x0D, 0x09, 0x02, 0xA1, 0x01, 0x09, 0x20, 0xA1, 0x00,
            0x09, 0x32, 0x09, 0x42,
            0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x02, 0x81, 0x02,
            0x09, 0x5B,
            0x15, 0x00, 0x27, 0xFF, 0xFF, 0xFF, 0x7F, 0x75, 0x20, 0x95, 0x01, 0x81, 0x02,
            0x09, 0x41,
            0x15, 0x00, 0x26, 0x68, 0x01, 0x75, 0x10, 0x95, 0x01, 0x81, 0x02,
            0x05, 0x01, 0x09, 0x38,
            0x15, 0x81, 0x25, 0x7F, 0x75, 0x08, 0x95, 0x01, 0x81, 0x06,
            0xC0, 0xC0
        };
    }

    // ------------------------------------------------------------------
    // Descriptor parsing
    // ------------------------------------------------------------------

    TEST(TabletDescriptorTest, EmptyDescriptorYieldsEmptyLayout)
    {
        const TabletLayout layout = layout_of({});

        EXPECT_TRUE(layout.fields.empty());
        EXPECT_FALSE(layout.uses_report_ids);
        EXPECT_EQ(layout.axes, TabletAxes::NONE);
        EXPECT_FALSE(layout.has_invert);
        EXPECT_FALSE(layout.has_serial);
    }

    TEST(TabletDescriptorTest, PenDescriptorProducesOneFieldPerReportedUsage)
    {
        const TabletLayout layout = layout_of(pen_descriptor());

        EXPECT_EQ(layout.fields.size(), 9U);
        EXPECT_TRUE(layout.uses_report_ids);
        EXPECT_TRUE(layout.has_invert);
        EXPECT_FALSE(layout.has_serial);
    }

    TEST(TabletDescriptorTest, PenDescriptorPlacesFieldsAtCumulativeBitOffsets)
    {
        const TabletLayout layout = layout_of(pen_descriptor());

        const struct {
            uint16_t page;
            uint16_t usage;
            uint32_t offset;
            uint32_t size;
        } expected[] = {
            { k_page_digitizer, 0x42, 0, 1 },
            { k_page_digitizer, 0x44, 1, 1 },
            { k_page_digitizer, 0x3C, 2, 1 },
            { k_page_digitizer, 0x32, 3, 1 },
            { k_page_generic, 0x30, 8, 16 },
            { k_page_generic, 0x31, 24, 16 },
            { k_page_digitizer, 0x30, 40, 16 },
            { k_page_digitizer, 0x3D, 56, 8 },
            { k_page_digitizer, 0x3E, 64, 8 },
        };

        for (const auto& row : expected) {
            const TabletField* field = find_field(layout, row.page, row.usage);
            ASSERT_NE(field, nullptr) << row.page << ":" << row.usage;
            EXPECT_EQ(field->bit_offset, row.offset) << row.page << ":" << row.usage;
            EXPECT_EQ(field->bit_size, row.size) << row.page << ":" << row.usage;
            EXPECT_EQ(field->report_id, 1U) << row.page << ":" << row.usage;
        }
    }

    TEST(TabletDescriptorTest, PenDescriptorMapsUsagesToFrameSlots)
    {
        const TabletLayout layout = layout_of(pen_descriptor());

        EXPECT_EQ(find_field(layout, k_page_generic, 0x30)->slot, TabletFrame::X);
        EXPECT_EQ(find_field(layout, k_page_generic, 0x31)->slot, TabletFrame::Y);
        EXPECT_EQ(find_field(layout, k_page_digitizer, 0x30)->slot, TabletFrame::PRESSURE);
        EXPECT_EQ(find_field(layout, k_page_digitizer, 0x3D)->slot, TabletFrame::TILT_X);
        EXPECT_EQ(find_field(layout, k_page_digitizer, 0x3E)->slot, TabletFrame::TILT_Y);
    }

    TEST(TabletDescriptorTest, SwitchUsagesAreNotFrameSlots)
    {
        const TabletLayout layout = layout_of(pen_descriptor());

        for (const uint16_t usage : { 0x42, 0x44, 0x3C, 0x32 }) {
            const TabletField* field = find_field(layout, k_page_digitizer, static_cast<uint16_t>(usage));
            ASSERT_NE(field, nullptr) << usage;
            EXPECT_GE(field->slot, TabletFrame::SLOT_COUNT) << usage;
        }
    }

    TEST(TabletDescriptorTest, PenDescriptorReportsPressureAndTiltAxesOnly)
    {
        const TabletLayout layout = layout_of(pen_descriptor());

        EXPECT_EQ(layout.axes, TabletAxes::PRESSURE | TabletAxes::TILT);
        EXPECT_FALSE(Core::has_axis(layout.axes, TabletAxes::ROTATION));
        EXPECT_FALSE(Core::has_axis(layout.axes, TabletAxes::DISTANCE));
    }

    TEST(TabletDescriptorTest, LogicalRangesFollowDescriptorIncludingSignedMinimum)
    {
        const TabletLayout layout = layout_of(pen_descriptor());

        const TabletField* x = find_field(layout, k_page_generic, 0x30);
        const TabletField* pressure = find_field(layout, k_page_digitizer, 0x30);
        const TabletField* tilt = find_field(layout, k_page_digitizer, 0x3D);
        ASSERT_NE(x, nullptr);
        ASSERT_NE(pressure, nullptr);
        ASSERT_NE(tilt, nullptr);

        EXPECT_EQ(x->logical_min, 0);
        EXPECT_EQ(x->logical_max, 32767);
        EXPECT_EQ(pressure->logical_max, 1023);
        EXPECT_EQ(tilt->logical_min, -127);
        EXPECT_EQ(tilt->logical_max, 127);
    }

    TEST(TabletDescriptorTest, SerialRotationAndWheelAreRecognised)
    {
        const TabletLayout layout = layout_of(serial_rotation_wheel_descriptor());

        EXPECT_FALSE(layout.uses_report_ids);
        EXPECT_TRUE(layout.has_serial);
        EXPECT_FALSE(layout.has_invert);
        EXPECT_EQ(layout.axes, TabletAxes::ROTATION | TabletAxes::WHEEL);

        const TabletField* serial = find_field(layout, k_page_digitizer, 0x5B);
        const TabletField* twist = find_field(layout, k_page_digitizer, 0x41);
        const TabletField* wheel = find_field(layout, k_page_generic, 0x38);
        ASSERT_NE(serial, nullptr);
        ASSERT_NE(twist, nullptr);
        ASSERT_NE(wheel, nullptr);

        EXPECT_EQ(serial->bit_offset, 2U);
        EXPECT_EQ(serial->bit_size, 32U);
        EXPECT_EQ(twist->bit_offset, 34U);
        EXPECT_EQ(twist->slot, TabletFrame::ROTATION);
        EXPECT_EQ(wheel->bit_offset, 50U);
        EXPECT_EQ(wheel->slot, TabletFrame::WHEEL);
    }

    TEST(TabletDescriptorTest, ReportIdsKeepIndependentBitCursors)
    {
        const std::vector<uint8_t> descriptor {
            0x05, 0x0D, 0x09, 0x02, 0xA1, 0x01,
            0x85, 0x01, 0x09, 0x30,
            0x15, 0x00, 0x26, 0xFF, 0x03, 0x75, 0x10, 0x95, 0x01, 0x81, 0x02,
            0x85, 0x02, 0x09, 0x3D,
            0x15, 0x81, 0x25, 0x7F, 0x75, 0x08, 0x95, 0x01, 0x81, 0x02,
            0xC0
        };

        const TabletLayout layout = layout_of(descriptor);

        const TabletField* pressure = find_field(layout, k_page_digitizer, 0x30);
        const TabletField* tilt = find_field(layout, k_page_digitizer, 0x3D);
        ASSERT_NE(pressure, nullptr);
        ASSERT_NE(tilt, nullptr);
        EXPECT_EQ(pressure->report_id, 1U);
        EXPECT_EQ(pressure->bit_offset, 0U);
        EXPECT_EQ(tilt->report_id, 2U);
        EXPECT_EQ(tilt->bit_offset, 0U);
    }

    TEST(TabletDescriptorTest, PopRestoresGlobalStateSavedByPush)
    {
        const std::vector<uint8_t> descriptor {
            0x05, 0x0D, 0x09, 0x02, 0xA1, 0x01, 0x85, 0x01,
            0x75, 0x08, 0x95, 0x01,
            0xA4,
            0x75, 0x10, 0x09, 0x30, 0x15, 0x00, 0x26, 0xFF, 0x03, 0x81, 0x02,
            0xB4,
            0x09, 0x3D, 0x15, 0x81, 0x25, 0x7F, 0x81, 0x02,
            0xC0
        };

        const TabletLayout layout = layout_of(descriptor);

        const TabletField* pressure = find_field(layout, k_page_digitizer, 0x30);
        const TabletField* tilt = find_field(layout, k_page_digitizer, 0x3D);
        ASSERT_NE(pressure, nullptr);
        ASSERT_NE(tilt, nullptr);
        EXPECT_EQ(pressure->bit_size, 16U);
        EXPECT_EQ(tilt->bit_size, 8U);
        EXPECT_EQ(tilt->bit_offset, 16U);
    }

    TEST(TabletDescriptorTest, ExtendedUsageCarriesItsOwnPage)
    {
        const std::vector<uint8_t> descriptor {
            0x05, 0x01, 0x09, 0x02, 0xA1, 0x01,
            0x0B, 0x30, 0x00, 0x0D, 0x00,
            0x15, 0x00, 0x26, 0xFF, 0x03, 0x75, 0x10, 0x95, 0x01, 0x81, 0x02,
            0xC0
        };

        const TabletLayout layout = layout_of(descriptor);

        ASSERT_EQ(layout.fields.size(), 1U);
        EXPECT_EQ(layout.fields[0].usage_page, k_page_digitizer);
        EXPECT_EQ(layout.fields[0].usage, 0x30);
        EXPECT_EQ(layout.fields[0].slot, TabletFrame::PRESSURE);
        EXPECT_EQ(layout.axes, TabletAxes::PRESSURE);
    }

    TEST(TabletDescriptorTest, UsageRangeExpandsAcrossReportCount)
    {
        const std::vector<uint8_t> descriptor {
            0x05, 0x0D, 0x09, 0x02, 0xA1, 0x01,
            0x19, 0x3D, 0x29, 0x3E,
            0x15, 0x81, 0x25, 0x7F, 0x75, 0x08, 0x95, 0x02, 0x81, 0x02,
            0xC0
        };

        const TabletLayout layout = layout_of(descriptor);

        ASSERT_EQ(layout.fields.size(), 2U);
        EXPECT_EQ(layout.fields[0].usage, 0x3D);
        EXPECT_EQ(layout.fields[0].slot, TabletFrame::TILT_X);
        EXPECT_EQ(layout.fields[0].bit_offset, 0U);
        EXPECT_EQ(layout.fields[1].usage, 0x3E);
        EXPECT_EQ(layout.fields[1].slot, TabletFrame::TILT_Y);
        EXPECT_EQ(layout.fields[1].bit_offset, 8U);
    }

    TEST(TabletDescriptorTest, DescriptorWithoutDigitizerPageIsNotATablet)
    {
        const std::vector<uint8_t> mouse {
            0x05, 0x01, 0x09, 0x02, 0xA1, 0x01, 0x09, 0x01, 0xA1, 0x00,
            0x05, 0x09, 0x19, 0x01, 0x29, 0x03,
            0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x03, 0x81, 0x02,
            0x75, 0x05, 0x95, 0x01, 0x81, 0x03,
            0x05, 0x01, 0x09, 0x30, 0x09, 0x31,
            0x15, 0x81, 0x25, 0x7F, 0x75, 0x08, 0x95, 0x02, 0x81, 0x06,
            0xC0, 0xC0
        };

        const TabletLayout layout = layout_of(mouse);

        EXPECT_TRUE(layout.fields.empty());
    }

    TEST(TabletDescriptorTest, ConstantPaddingProducesNoFieldsButAdvancesCursor)
    {
        const TabletLayout layout = layout_of(pen_descriptor());

        for (const auto& field : layout.fields) {
            EXPECT_TRUE(field.bit_offset < 4U || field.bit_offset >= 8U)
                << field.usage_page << ":" << field.usage;
        }
    }

    TEST(TabletDescriptorTest, TruncatedItemStopsParsingWithoutFault)
    {
        const std::vector<uint8_t> truncated {
            0x05, 0x0D, 0x09, 0x02, 0xA1, 0x01, 0x26, 0xFF
        };

        EXPECT_NO_THROW({
            const TabletLayout layout = layout_of(truncated);
            EXPECT_TRUE(layout.fields.empty());
        });
    }

    TEST(TabletDescriptorTest, ZeroSizedInputItemIsSkipped)
    {
        const std::vector<uint8_t> descriptor {
            0x05, 0x0D, 0x09, 0x02, 0xA1, 0x01,
            0x09, 0x30, 0x15, 0x00, 0x26, 0xFF, 0x03, 0x81, 0x02,
            0xC0
        };

        const TabletLayout layout = layout_of(descriptor);

        EXPECT_TRUE(layout.fields.empty());
    }

    // ------------------------------------------------------------------
    // Lifecycle
    // ------------------------------------------------------------------

    TEST(TabletBackendTest, ReportsIdentityBeforeInitialization)
    {
        TabletBackend backend;

        EXPECT_EQ(backend.get_type(), Core::InputType::TABLET);
        EXPECT_FALSE(backend.get_name().empty());
        EXPECT_EQ(backend.get_version().rfind("HIDAPI", 0), 0U);
        EXPECT_FALSE(backend.is_initialized());
        EXPECT_FALSE(backend.is_running());
    }

    TEST(TabletBackendTest, QueriesBeforeInitializationAreEmpty)
    {
        TabletBackend backend;

        EXPECT_EQ(backend.refresh_devices(), 0U);
        EXPECT_TRUE(backend.get_devices().empty());
        EXPECT_TRUE(backend.get_open_devices().empty());
        EXPECT_FALSE(backend.open_device(1));
        EXPECT_FALSE(backend.is_device_open(1));
        EXPECT_EQ(backend.get_tool_info(1), std::nullopt);
        EXPECT_EQ(backend.get_layout(1), std::nullopt);
    }

    TEST(TabletBackendTest, InitializeIsIdempotentAndShutdownReleases)
    {
        TabletBackend backend;

        ASSERT_TRUE(backend.initialize());
        EXPECT_TRUE(backend.is_initialized());
        EXPECT_TRUE(backend.initialize());

        backend.shutdown();
        EXPECT_FALSE(backend.is_initialized());
        EXPECT_TRUE(backend.get_devices().empty());
        EXPECT_NO_THROW(backend.shutdown());
    }

    TEST(TabletBackendTest, StartRequiresInitialization)
    {
        TabletBackend backend;

        backend.start();

        EXPECT_FALSE(backend.is_running());
    }

    TEST(TabletBackendTest, StartAndStopAreIdempotent)
    {
        TabletBackend backend;
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

    TEST(TabletBackendTest, ShutdownStopsARunningBackend)
    {
        TabletBackend backend;
        ASSERT_TRUE(backend.initialize());
        backend.start();

        backend.shutdown();

        EXPECT_FALSE(backend.is_running());
        EXPECT_FALSE(backend.is_initialized());
    }

    TEST(TabletBackendTest, DestructionWhileRunningIsClean)
    {
        auto backend = std::make_unique<TabletBackend>();
        ASSERT_TRUE(backend->initialize());
        backend->start();

        EXPECT_NO_THROW(backend.reset());
    }

    TEST(TabletBackendTest, RestartsRepeatedlyInOneProcess)
    {
        TabletBackend backend;

        for (int i = 0; i < 3; ++i) {
            ASSERT_TRUE(backend.initialize()) << i;
            backend.start();
            EXPECT_TRUE(backend.is_running()) << i;
            backend.stop();
            backend.shutdown();
            EXPECT_FALSE(backend.is_initialized()) << i;
        }
    }

    TEST(TabletBackendTest, SequentialBackendsInitializeIndependently)
    {
        {
            TabletBackend first;
            ASSERT_TRUE(first.initialize());
        }

        TabletBackend second;
        EXPECT_TRUE(second.initialize());
    }

    TEST(TabletBackendTest, ProbingOnlyAnnouncedDigitizersStillInitializes)
    {
        TabletBackend::Config config;
        config.probe_all_devices = false;
        TabletBackend backend(config);

        EXPECT_TRUE(backend.initialize());
    }

    TEST(TabletBackendTest, UnknownIdsAreRejectedOrIgnored)
    {
        TabletBackend backend;
        ASSERT_TRUE(backend.initialize());
        constexpr uint32_t unknown = 0xFFFFFFF0U;

        EXPECT_FALSE(backend.open_device(unknown));
        EXPECT_FALSE(backend.is_device_open(unknown));
        EXPECT_NO_THROW(backend.close_device(unknown));
        EXPECT_EQ(backend.get_tool_info(unknown), std::nullopt);
        EXPECT_EQ(backend.get_layout(unknown), std::nullopt);
    }

    TEST(TabletBackendTest, DeviceCallbackAnnouncesEveryAdoptedToolOnInitialize)
    {
        TabletBackend backend;
        std::vector<Core::InputDeviceInfo> announced;
        std::vector<bool> connected;
        backend.set_device_callback([&](const Core::InputDeviceInfo& info, bool is_connected) {
            announced.push_back(info);
            connected.push_back(is_connected);
        });

        ASSERT_TRUE(backend.initialize());

        const auto devices = backend.get_devices();
        EXPECT_EQ(announced.size(), devices.size());
        for (const bool flag : connected)
            EXPECT_TRUE(flag);
        for (const auto& info : announced)
            EXPECT_EQ(info.backend_type, Core::InputType::TABLET);
    }

    TEST(TabletBackendTest, RefreshDoesNotDuplicateAdoptedTools)
    {
        TabletBackend backend;
        ASSERT_TRUE(backend.initialize());
        const size_t first = backend.get_devices().size();

        EXPECT_EQ(backend.refresh_devices(), first);
        EXPECT_EQ(backend.get_devices().size(), first);
    }

    // ------------------------------------------------------------------
    // Attached tablet
    // ------------------------------------------------------------------

#define REQUIRE_TABLET(backend)                           \
    do {                                                  \
        if ((backend).get_devices().empty())              \
            GTEST_SKIP() << "No tablet device available"; \
    } while (0)

    TEST(TabletBackendTest, ToolInfoAndLayoutAgreeWithEnumeration)
    {
        TabletBackend backend;
        ASSERT_TRUE(backend.initialize());
        REQUIRE_TABLET(backend);

        for (const auto& device : backend.get_devices()) {
            const auto tool = backend.get_tool_info(device.id);
            const auto layout = backend.get_layout(device.id);
            ASSERT_TRUE(tool.has_value()) << device.id;
            ASSERT_TRUE(layout.has_value()) << device.id;

            EXPECT_EQ(tool->id, device.id);
            EXPECT_EQ(tool->name, device.name);
            EXPECT_TRUE(tool->is_connected);
            EXPECT_EQ(tool->backend_type, Core::InputType::TABLET);
            EXPECT_NE(tool->tool_type, TabletToolType::UNKNOWN);
            EXPECT_FALSE(tool->tablet_name.empty());
            EXPECT_FALSE(layout->fields.empty());
            EXPECT_EQ(tool->axes, layout->axes);
        }
    }

    TEST(TabletBackendTest, DisablingEraserSplitReportsPensOnly)
    {
        TabletBackend::Config config;
        config.split_eraser = false;
        TabletBackend backend(config);
        ASSERT_TRUE(backend.initialize());
        REQUIRE_TABLET(backend);

        for (const auto& device : backend.get_devices()) {
            const auto tool = backend.get_tool_info(device.id);
            ASSERT_TRUE(tool.has_value());
            EXPECT_EQ(tool->tool_type, TabletToolType::PEN);
        }
    }

    TEST(TabletBackendTest, PenAndEraserOfOneTabletShareLayoutAndName)
    {
        TabletBackend backend;
        ASSERT_TRUE(backend.initialize());
        REQUIRE_TABLET(backend);

        for (const auto& device : backend.get_devices()) {
            const auto tool = backend.get_tool_info(device.id);
            ASSERT_TRUE(tool.has_value());
            if (tool->tool_type != TabletToolType::ERASER)
                continue;

            bool found_pen = false;
            for (const auto& other : backend.get_devices()) {
                const auto pen = backend.get_tool_info(other.id);
                if (!pen || pen->tool_type != TabletToolType::PEN || pen->tablet_name != tool->tablet_name)
                    continue;
                found_pen = true;
                EXPECT_NE(pen->id, tool->id);
                EXPECT_EQ(backend.get_layout(pen->id)->fields.size(), backend.get_layout(tool->id)->fields.size());
            }
            EXPECT_TRUE(found_pen);
        }
    }

    TEST(TabletBackendTest, ToolsAreOpenAfterInitializeAndReopenAfterClose)
    {
        TabletBackend backend;
        ASSERT_TRUE(backend.initialize());
        REQUIRE_TABLET(backend);
        const uint32_t id = backend.get_devices().front().id;

        EXPECT_TRUE(backend.is_device_open(id));

        backend.close_device(id);
        EXPECT_FALSE(backend.is_device_open(id));
        EXPECT_NO_THROW(backend.close_device(id));

        EXPECT_TRUE(backend.open_device(id));
        EXPECT_TRUE(backend.is_device_open(id));
    }

    TEST(TabletBackendTest, OpeningAnOpenToolKeepsItOpen)
    {
        TabletBackend backend;
        ASSERT_TRUE(backend.initialize());
        REQUIRE_TABLET(backend);
        const uint32_t id = backend.get_devices().front().id;

        EXPECT_TRUE(backend.open_device(id));
        EXPECT_TRUE(backend.is_device_open(id));
    }

    TEST(TabletBackendTest, RunningBackendSurvivesCloseAndReopenOfATool)
    {
        TabletBackend backend;
        ASSERT_TRUE(backend.initialize());
        REQUIRE_TABLET(backend);
        const uint32_t id = backend.get_devices().front().id;

        backend.start();
        backend.close_device(id);
        EXPECT_TRUE(backend.open_device(id));
        backend.stop();

        EXPECT_FALSE(backend.is_running());
        EXPECT_TRUE(backend.is_device_open(id));
    }

} // namespace
} // namespace MayaFlux::Test
