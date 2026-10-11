#include "gtest/gtest.h"

#if defined(MAYAFLUX_PLATFORM_MACOS)

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <unistd.h>

#include "MayaFlux/Core/Backends/System/NSBackend.hpp"

#include "../support/cocoa_event_injector.hpp"
#include "../support/cocoa_panel_driver.hpp"

#define REQUIRE_DISPLAY()                                                      \
    do {                                                                       \
        if (!CocoaInput::display_available())                                  \
            GTEST_SKIP() << "No window server session with an active display"; \
    } while (false)

namespace MayaFlux::Test {
namespace {

    namespace fs = std::filesystem;
    using namespace std::chrono_literals;
    using CocoaPanel::PanelAction;

    class DialogOutcome {
    public:
        void set(Core::FileDialogResult result)
        {
            {
                std::lock_guard lock(m_mutex);
                ++m_calls;
                m_thread = std::this_thread::get_id();
                m_result = std::move(result);
            }
            m_changed.notify_all();
        }

        [[nodiscard]] int calls() const
        {
            std::lock_guard lock(m_mutex);
            return m_calls;
        }

        [[nodiscard]] std::thread::id thread() const
        {
            std::lock_guard lock(m_mutex);
            return m_thread;
        }

        [[nodiscard]] Core::FileDialogResult result() const
        {
            std::lock_guard lock(m_mutex);
            return m_result.value_or(std::unexpected(Core::SystemDialogError::NotSupported));
        }

    private:
        mutable std::mutex m_mutex;
        std::condition_variable m_changed;
        std::optional<Core::FileDialogResult> m_result;
        std::thread::id m_thread;
        int m_calls { 0 };
    };

    Core::FileDialogCallback record_into(const std::shared_ptr<DialogOutcome>& outcome)
    {
        return [outcome](Core::FileDialogResult result) { outcome->set(std::move(result)); };
    }

    template <typename Predicate>
    bool eventually(Predicate predicate, std::chrono::milliseconds timeout = 5s)
    {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            if (predicate())
                return true;
            CocoaInput::pump_main_queue();
        }
        return predicate();
    }

    class ScratchDirectory {
    public:
        ScratchDirectory()
            : m_path(fs::temp_directory_path() / ("mayaflux_ns_backend_" + std::to_string(::getpid())))
        {
            fs::create_directories(m_path);
        }

        ~ScratchDirectory()
        {
            std::error_code ignored;
            fs::remove_all(m_path, ignored);
        }

        ScratchDirectory(const ScratchDirectory&) = delete;
        ScratchDirectory& operator=(const ScratchDirectory&) = delete;
        ScratchDirectory(ScratchDirectory&&) = delete;
        ScratchDirectory& operator=(ScratchDirectory&&) = delete;

        [[nodiscard]] const fs::path& path() const { return m_path; }

    private:
        fs::path m_path;
    };

    std::vector<Core::SystemFileFilter> audio_filters()
    {
        return { { "Audio", { "wav", "flac" } }, { "All", { "*" } } };
    }

    struct DialogKind {
        const char* name;
        bool save;
    };

    void present(Core::NSBackend& backend, const DialogKind& kind,
        const std::shared_ptr<DialogOutcome>& outcome,
        std::vector<Core::SystemFileFilter> filters = audio_filters(),
        fs::path start_dir = {})
    {
        if (kind.save)
            backend.save_file(record_into(outcome), "take.wav", std::move(filters), std::move(start_dir));
        else
            backend.open_file(record_into(outcome), std::move(filters), std::move(start_dir));
    }

    class NsDialogTest : public ::testing::TestWithParam<DialogKind> { };

    const auto k_kind_name = [](const ::testing::TestParamInfo<DialogKind>& info) {
        return std::string(info.param.name);
    };

    const auto k_kinds = ::testing::Values(DialogKind { "Open", false }, DialogKind { "Save", true });

    TEST(NsBackendTest, InitializeAndShutdownAreIdempotent)
    {
        Core::NSBackend backend;
        EXPECT_FALSE(backend.is_initialized());

        EXPECT_TRUE(backend.initialize());
        EXPECT_TRUE(backend.is_initialized());
        EXPECT_TRUE(backend.initialize());
        EXPECT_TRUE(backend.is_initialized());

        backend.shutdown();
        EXPECT_FALSE(backend.is_initialized());
        backend.shutdown();
        EXPECT_FALSE(backend.is_initialized());

        EXPECT_TRUE(backend.initialize());
        EXPECT_TRUE(backend.is_initialized());
        backend.shutdown();
    }

    TEST_P(NsDialogTest, UninitializedBackendReportsBackendErrorSynchronously)
    {
        Core::NSBackend backend;
        auto outcome = std::make_shared<DialogOutcome>();

        present(backend, GetParam(), outcome);

        EXPECT_EQ(outcome->calls(), 1);
        const auto result = outcome->result();
        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(result.error(), Core::SystemDialogError::BackendError);
    }

    TEST_P(NsDialogTest, ShutdownBackendReportsBackendErrorSynchronously)
    {
        Core::NSBackend backend;
        ASSERT_TRUE(backend.initialize());
        backend.shutdown();
        auto outcome = std::make_shared<DialogOutcome>();

        present(backend, GetParam(), outcome);

        EXPECT_EQ(outcome->calls(), 1);
        const auto result = outcome->result();
        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(result.error(), Core::SystemDialogError::BackendError);
    }

    TEST_P(NsDialogTest, CancellingPanelDeliversCancelledExactlyOnce)
    {
        REQUIRE_DISPLAY();
        Core::NSBackend backend;
        ASSERT_TRUE(backend.initialize());
        auto outcome = std::make_shared<DialogOutcome>();
        const auto panel = CocoaPanel::drive_next_panel(PanelAction::Cancel);

        present(backend, GetParam(), outcome);

        ASSERT_TRUE(panel->seen);
        EXPECT_EQ(panel->is_open_panel, !GetParam().save);
        EXPECT_EQ(outcome->calls(), 1);
        const auto result = outcome->result();
        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(result.error(), Core::SystemDialogError::Cancelled);
        backend.shutdown();
    }

    TEST_P(NsDialogTest, UnresolvableStartDirectoryStillPresentsPanel)
    {
        REQUIRE_DISPLAY();
        Core::NSBackend backend;
        ASSERT_TRUE(backend.initialize());
        auto outcome = std::make_shared<DialogOutcome>();
        const auto panel = CocoaPanel::drive_next_panel(PanelAction::Cancel);

        present(backend, GetParam(), outcome, audio_filters(), "/mayaflux/no/such/directory");

        ASSERT_TRUE(panel->seen);
        EXPECT_EQ(outcome->calls(), 1);
        const auto result = outcome->result();
        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(result.error(), Core::SystemDialogError::Cancelled);
        backend.shutdown();
    }

    TEST_P(NsDialogTest, ResultIsDeliveredOnMainThreadWhenCalledFromWorker)
    {
        REQUIRE_DISPLAY();
        Core::NSBackend backend;
        ASSERT_TRUE(backend.initialize());
        auto outcome = std::make_shared<DialogOutcome>();
        const auto panel = CocoaPanel::drive_next_panel(PanelAction::Cancel);
        const auto main_thread = std::this_thread::get_id();
        const auto kind = GetParam();

        std::thread worker([&] { present(backend, kind, outcome); });

        struct WorkerGuard {
            std::thread& worker;
            const std::shared_ptr<DialogOutcome>& outcome;

            WorkerGuard(std::thread& thread, const std::shared_ptr<DialogOutcome>& result)
                : worker(thread)
                , outcome(result)
            {
            }

            WorkerGuard(const WorkerGuard&) = delete;
            WorkerGuard& operator=(const WorkerGuard&) = delete;

            ~WorkerGuard()
            {
                eventually([this] { return outcome->calls() > 0; }, 10s);
                if (worker.joinable())
                    worker.join();
            }
        } guard { worker, outcome };

        ASSERT_TRUE(eventually([&] { return outcome->calls() > 0; }));
        EXPECT_TRUE(panel->seen);
        EXPECT_EQ(outcome->calls(), 1);
        EXPECT_EQ(outcome->thread(), main_thread);
        const auto result = outcome->result();
        ASSERT_FALSE(result.has_value()) << "delivered path: " << result->string();
        EXPECT_EQ(result.error(), Core::SystemDialogError::Cancelled);
        backend.shutdown();
    }

    TEST_P(NsDialogTest, BackendPresentsPanelsAgainAfterReinitialization)
    {
        REQUIRE_DISPLAY();
        Core::NSBackend backend;

        for (int round = 0; round < 2; ++round) {
            ASSERT_TRUE(backend.initialize());
            auto outcome = std::make_shared<DialogOutcome>();
            const auto panel = CocoaPanel::drive_next_panel(PanelAction::Cancel);

            present(backend, GetParam(), outcome);

            EXPECT_TRUE(panel->seen);
            EXPECT_EQ(outcome->calls(), 1);
            const auto result = outcome->result();
            ASSERT_FALSE(result.has_value());
            EXPECT_EQ(result.error(), Core::SystemDialogError::Cancelled);
            backend.shutdown();
        }
    }

    struct FilterCase {
        std::string name;
        bool save;
        std::vector<Core::SystemFileFilter> filters;
        std::vector<std::string> expected_extensions;
    };

    std::vector<FilterCase> filter_cases()
    {
        std::vector<FilterCase> cases;
        for (const bool save : { false, true }) {
            const std::string prefix = save ? "Save" : "Open";
            cases.push_back({ prefix + "AudioAndWildcard", save, audio_filters(), { "flac", "wav" } });
            cases.push_back({ prefix + "MultipleGroups", save,
                { { "Audio", { "wav", "flac" } }, { "Images", { "png" } } },
                { "flac", "png", "wav" } });
            cases.push_back({ prefix + "WildcardOnly", save, { { "All", { "*" } } }, {} });
            cases.push_back({ prefix + "NoFilters", save, {}, {} });
        }
        return cases;
    }

    class NsFilterTest : public ::testing::TestWithParam<FilterCase> { };

    TEST_P(NsFilterTest, PanelAllowsOnlyTheRequestedExtensions)
    {
        REQUIRE_DISPLAY();
        const auto& param = GetParam();
        Core::NSBackend backend;
        ASSERT_TRUE(backend.initialize());
        auto outcome = std::make_shared<DialogOutcome>();
        const auto panel = CocoaPanel::drive_next_panel(PanelAction::Cancel);

        present(backend, DialogKind { "", param.save }, outcome, param.filters);

        ASSERT_TRUE(panel->seen);
        auto extensions = panel->extensions;
        std::ranges::sort(extensions);
        EXPECT_EQ(extensions, param.expected_extensions);
        backend.shutdown();
    }

    INSTANTIATE_TEST_SUITE_P(
        NsBackend,
        NsFilterTest,
        ::testing::ValuesIn(filter_cases()),
        [](const ::testing::TestParamInfo<FilterCase>& info) { return info.param.name; });

    TEST(NsBackendTest, OpenPanelSelectsSingleFilesOnly)
    {
        REQUIRE_DISPLAY();
        Core::NSBackend backend;
        ASSERT_TRUE(backend.initialize());
        auto outcome = std::make_shared<DialogOutcome>();
        const auto panel = CocoaPanel::drive_next_panel(PanelAction::Cancel);

        present(backend, DialogKind { "Open", false }, outcome);

        ASSERT_TRUE(panel->seen);
        EXPECT_TRUE(panel->is_open_panel);
        EXPECT_TRUE(panel->can_choose_files);
        EXPECT_FALSE(panel->can_choose_directories);
        EXPECT_FALSE(panel->allows_multiple_selection);
        backend.shutdown();
    }

    TEST_P(NsDialogTest, PanelOpensInRequestedStartDirectory)
    {
        REQUIRE_DISPLAY();
        const ScratchDirectory scratch;
        Core::NSBackend backend;
        ASSERT_TRUE(backend.initialize());
        auto outcome = std::make_shared<DialogOutcome>();
        const auto panel = CocoaPanel::drive_next_panel(PanelAction::Cancel);

        present(backend, GetParam(), outcome, audio_filters(), scratch.path());

        ASSERT_TRUE(panel->seen);
        std::error_code error;
        EXPECT_TRUE(fs::equivalent(panel->directory, scratch.path(), error));
        backend.shutdown();
    }

    INSTANTIATE_TEST_SUITE_P(NsBackend, NsDialogTest, k_kinds, k_kind_name);

    TEST(NsBackendTest, SavePanelCarriesSuggestedName)
    {
        REQUIRE_DISPLAY();
        Core::NSBackend backend;
        ASSERT_TRUE(backend.initialize());
        auto outcome = std::make_shared<DialogOutcome>();
        const auto panel = CocoaPanel::drive_next_panel(PanelAction::Cancel);

        present(backend, DialogKind { "Save", true }, outcome);

        ASSERT_TRUE(panel->seen);
        EXPECT_FALSE(panel->is_open_panel);
        EXPECT_EQ(panel->name, "take.wav");
        backend.shutdown();
    }

    TEST(NsBackendTest, AcceptingSavePanelDeliversStartDirectoryAndSuggestedName)
    {
        REQUIRE_DISPLAY();
        const ScratchDirectory scratch;
        Core::NSBackend backend;
        ASSERT_TRUE(backend.initialize());
        auto outcome = std::make_shared<DialogOutcome>();
        const auto panel = CocoaPanel::drive_next_panel(PanelAction::Accept);

        present(backend, DialogKind { "Save", true }, outcome, audio_filters(), scratch.path());

        ASSERT_TRUE(panel->seen);
        EXPECT_EQ(outcome->calls(), 1);
        const auto result = outcome->result();
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->filename(), "take.wav");
        std::error_code error;
        EXPECT_TRUE(fs::equivalent(result->parent_path(), scratch.path(), error));
        EXPECT_FALSE(fs::exists(*result));
        backend.shutdown();
    }

}
}

#undef REQUIRE_DISPLAY

#endif
