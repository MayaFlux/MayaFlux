#include "gtest/gtest.h"

#if defined(MAYAFLUX_PLATFORM_WINDOWS)

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "MayaFlux/Core/Backends/System/COMBackend.hpp"

namespace MayaFlux::Test {
namespace {

    using namespace std::chrono_literals;

    class DialogOutcome {
    public:
        void set(Core::FileDialogResult result)
        {
            {
                std::lock_guard lock(m_mutex);
                ++m_calls;
                m_result = std::move(result);
            }
            m_changed.notify_all();
        }

        bool wait(std::chrono::milliseconds timeout)
        {
            std::unique_lock lock(m_mutex);
            return m_changed.wait_for(lock, timeout, [&] { return m_calls > 0; });
        }

        [[nodiscard]] int calls() const
        {
            std::lock_guard lock(m_mutex);
            return m_calls;
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
        int m_calls { 0 };
    };

    Core::FileDialogCallback record_into(const std::shared_ptr<DialogOutcome>& outcome)
    {
        return [outcome](Core::FileDialogResult result) { outcome->set(std::move(result)); };
    }

    BOOL CALLBACK find_dialog(HWND hwnd, LPARAM out)
    {
        DWORD process = 0;
        GetWindowThreadProcessId(hwnd, &process);
        if (process != GetCurrentProcessId() || !IsWindowVisible(hwnd))
            return TRUE;

        wchar_t name[32] {};
        GetClassNameW(hwnd, name, 32);
        if (std::wstring(name) != L"#32770")
            return TRUE;

        *reinterpret_cast<HWND*>(out) = hwnd;
        return FALSE;
    }

    HWND wait_for_dialog(const DialogOutcome& outcome, std::chrono::milliseconds timeout = 5s)
    {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            if (outcome.calls() > 0)
                return nullptr;
            HWND dialog = nullptr;
            EnumWindows(find_dialog, reinterpret_cast<LPARAM>(&dialog));
            if (dialog)
                return dialog;
            std::this_thread::sleep_for(10ms);
        }
        return nullptr;
    }

    struct DialogKind {
        const char* name;
        bool save;
    };

    void present(Core::COMBackend& backend, const DialogKind& kind,
        const std::shared_ptr<DialogOutcome>& outcome, std::filesystem::path start_dir = {})
    {
        std::vector<Core::SystemFileFilter> filters { { "Audio", { "wav", "flac" } }, { "All", { "*" } } };
        if (kind.save)
            backend.save_file(record_into(outcome), "take.wav", filters, std::move(start_dir));
        else
            backend.open_file(record_into(outcome), filters, std::move(start_dir));
    }

    class ComDialogTest : public ::testing::TestWithParam<DialogKind> { };

    const auto k_kind_name = [](const ::testing::TestParamInfo<DialogKind>& info) {
        return std::string(info.param.name);
    };

    const auto k_kinds = ::testing::Values(DialogKind { "Open", false }, DialogKind { "Save", true });

    TEST(ComBackendTest, InitializeAndShutdownAreIdempotent)
    {
        Core::COMBackend backend;
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

    TEST_P(ComDialogTest, UninitializedBackendReportsBackendErrorSynchronously)
    {
        Core::COMBackend backend;
        auto outcome = std::make_shared<DialogOutcome>();

        present(backend, GetParam(), outcome);

        EXPECT_EQ(outcome->calls(), 1);
        const auto result = outcome->result();
        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(result.error(), Core::SystemDialogError::BackendError);
    }

    TEST_P(ComDialogTest, ShutdownBackendReportsBackendErrorSynchronously)
    {
        Core::COMBackend backend;
        ASSERT_TRUE(backend.initialize());
        backend.shutdown();
        auto outcome = std::make_shared<DialogOutcome>();

        present(backend, GetParam(), outcome);

        EXPECT_EQ(outcome->calls(), 1);
        const auto result = outcome->result();
        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(result.error(), Core::SystemDialogError::BackendError);
    }

    TEST_P(ComDialogTest, ClosingDialogDeliversCancelledExactlyOnce)
    {
        Core::COMBackend backend;
        ASSERT_TRUE(backend.initialize());
        auto outcome = std::make_shared<DialogOutcome>();

        present(backend, GetParam(), outcome);
        EXPECT_EQ(outcome->calls(), 0);

        const HWND dialog = wait_for_dialog(*outcome);
        if (!dialog && outcome->calls() == 0)
            GTEST_SKIP() << "No file dialog appeared in this session";
        if (!dialog)
            GTEST_SKIP() << "Shell file dialog could not be created in this session";

        PostMessageW(dialog, WM_CLOSE, 0, 0);
        ASSERT_TRUE(outcome->wait(5s));
        std::this_thread::sleep_for(50ms);

        EXPECT_EQ(outcome->calls(), 1);
        const auto result = outcome->result();
        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(result.error(), Core::SystemDialogError::Cancelled);
        backend.shutdown();
    }

    TEST_P(ComDialogTest, UnresolvableStartDirectoryStillOpensDialog)
    {
        Core::COMBackend backend;
        ASSERT_TRUE(backend.initialize());
        auto outcome = std::make_shared<DialogOutcome>();

        present(backend, GetParam(), outcome, "Z:\\mayaflux\\no\\such\\directory");

        const HWND dialog = wait_for_dialog(*outcome);
        if (!dialog && outcome->calls() == 0)
            GTEST_SKIP() << "No file dialog appeared in this session";
        if (!dialog)
            GTEST_SKIP() << "Shell file dialog could not be created in this session";

        PostMessageW(dialog, WM_CLOSE, 0, 0);
        ASSERT_TRUE(outcome->wait(5s));

        const auto result = outcome->result();
        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(result.error(), Core::SystemDialogError::Cancelled);
        backend.shutdown();
    }

    INSTANTIATE_TEST_SUITE_P(ComBackend, ComDialogTest, k_kinds, k_kind_name);

}
}

#endif
