#include "gtest/gtest.h"

#if defined(MAYAFLUX_PLATFORM_LINUX)

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <expected>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <signal.h>
#include <unistd.h>

#include <dbus/dbus.h>

#include "MayaFlux/Core/Backends/System/DBusBackend.hpp"

namespace MayaFlux::Test {
namespace {

    using namespace std::chrono_literals;

    constexpr const char* k_portal_name = "org.freedesktop.portal.Desktop";
    constexpr const char* k_chooser_iface = "org.freedesktop.portal.FileChooser";
    constexpr const char* k_request_iface = "org.freedesktop.portal.Request";

    class PrivateBus {
    public:
        PrivateBus()
        {
            static std::atomic<int> counter { 0 };
            const auto directory = std::filesystem::temp_directory_path();
            m_config = directory / ("mayaflux-dbus-" + std::to_string(::getpid()) + "-" + std::to_string(counter++) + ".conf");

            std::ofstream file(m_config);
            file << "<busconfig>\n"
                 << "  <type>session</type>\n"
                 << "  <listen>unix:tmpdir=" << directory.string() << "</listen>\n"
                 << "  <auth>EXTERNAL</auth>\n"
                 << "  <policy context=\"default\">\n"
                 << "    <allow send_destination=\"*\" eavesdrop=\"true\"/>\n"
                 << "    <allow eavesdrop=\"true\"/>\n"
                 << "    <allow own=\"*\"/>\n"
                 << "  </policy>\n"
                 << "</busconfig>\n";
            file.close();

            const std::string command = "dbus-daemon --config-file='" + m_config.string()
                + "' --fork --print-address --print-pid 2>/dev/null";
            FILE* pipe = popen(command.c_str(), "r");
            if (!pipe)
                return;

            char line[512] {};
            while (std::fgets(line, sizeof(line), pipe)) {
                std::string text(line);
                while (!text.empty() && (text.back() == '\n' || text.back() == '\r'))
                    text.pop_back();
                if (text.rfind("unix:", 0) == 0)
                    m_address = text;
                else if (!text.empty() && text.find_first_not_of("0123456789") == std::string::npos)
                    m_pid = std::stoi(text);
            }
            pclose(pipe);
        }

        ~PrivateBus()
        {
            if (m_pid > 0)
                kill(m_pid, SIGTERM);
            std::error_code ec;
            std::filesystem::remove(m_config, ec);
        }

        PrivateBus(const PrivateBus&) = delete;
        PrivateBus& operator=(const PrivateBus&) = delete;
        PrivateBus(PrivateBus&&) = delete;
        PrivateBus& operator=(PrivateBus&&) = delete;

        [[nodiscard]] bool running() const { return !m_address.empty() && m_pid > 0; }
        [[nodiscard]] const std::string& address() const { return m_address; }

    private:
        std::filesystem::path m_config;
        std::string m_address;
        int m_pid { 0 };
    };

    /**
     * @brief One private bus for the whole process.
     *
     * libdbus reads DBUS_SESSION_BUS_ADDRESS once per process, so the address is
     * exported before the first connection and never changed afterwards.
     */
    PrivateBus& shared_bus()
    {
        struct SharedBus {
            PrivateBus bus;

            SharedBus()
            {
                if (bus.running())
                    setenv("DBUS_SESSION_BUS_ADDRESS", bus.address().c_str(), 1);
            }
        };
        static SharedBus shared;
        return shared.bus;
    }

    struct PortalFilter {
        std::string label;
        std::vector<std::string> globs;
        std::vector<uint32_t> types;
    };

    struct PortalCall {
        std::string method;
        std::string parent;
        std::string title;
        std::vector<std::string> option_keys;
        std::map<std::string, std::string> string_options;
        std::map<std::string, std::string> byte_options;
        std::map<std::string, std::string> signatures;
        std::vector<PortalFilter> filters;
    };

    struct PortalReply {
        uint32_t code { 0 };
        std::optional<std::string> uri;
        bool include_uris { true };
        bool before_reply { false };
    };

    void parse_filters(DBusMessageIter& variant, PortalCall& call)
    {
        DBusMessageIter outer;
        dbus_message_iter_recurse(&variant, &outer);
        while (dbus_message_iter_get_arg_type(&outer) == DBUS_TYPE_STRUCT) {
            DBusMessageIter filter;
            dbus_message_iter_recurse(&outer, &filter);

            PortalFilter parsed;
            const char* label = nullptr;
            dbus_message_iter_get_basic(&filter, static_cast<void*>(&label));
            parsed.label = label ? label : "";
            dbus_message_iter_next(&filter);

            DBusMessageIter patterns;
            dbus_message_iter_recurse(&filter, &patterns);
            while (dbus_message_iter_get_arg_type(&patterns) == DBUS_TYPE_STRUCT) {
                DBusMessageIter pattern;
                dbus_message_iter_recurse(&patterns, &pattern);
                dbus_uint32_t type = 0;
                dbus_message_iter_get_basic(&pattern, &type);
                dbus_message_iter_next(&pattern);
                const char* glob = nullptr;
                dbus_message_iter_get_basic(&pattern, static_cast<void*>(&glob));
                parsed.types.push_back(type);
                parsed.globs.emplace_back(glob ? glob : "");
                dbus_message_iter_next(&patterns);
            }

            call.filters.push_back(std::move(parsed));
            dbus_message_iter_next(&outer);
        }
    }

    PortalCall parse_call(DBusMessage* message)
    {
        PortalCall call;
        call.method = dbus_message_get_member(message);

        DBusMessageIter args;
        dbus_message_iter_init(message, &args);

        const char* parent = nullptr;
        dbus_message_iter_get_basic(&args, static_cast<void*>(&parent));
        call.parent = parent ? parent : "";
        dbus_message_iter_next(&args);

        const char* title = nullptr;
        dbus_message_iter_get_basic(&args, static_cast<void*>(&title));
        call.title = title ? title : "";
        dbus_message_iter_next(&args);

        DBusMessageIter options;
        dbus_message_iter_recurse(&args, &options);
        while (dbus_message_iter_get_arg_type(&options) == DBUS_TYPE_DICT_ENTRY) {
            DBusMessageIter entry;
            dbus_message_iter_recurse(&options, &entry);
            const char* key = nullptr;
            dbus_message_iter_get_basic(&entry, static_cast<void*>(&key));
            const std::string name = key ? key : "";
            call.option_keys.push_back(name);
            dbus_message_iter_next(&entry);

            DBusMessageIter variant;
            dbus_message_iter_recurse(&entry, &variant);
            char* signature = dbus_message_iter_get_signature(&variant);
            call.signatures[name] = signature ? signature : "";
            dbus_free(signature);

            if (dbus_message_iter_get_arg_type(&variant) == DBUS_TYPE_STRING) {
                const char* value = nullptr;
                dbus_message_iter_get_basic(&variant, static_cast<void*>(&value));
                call.string_options[name] = value ? value : "";
            } else if (dbus_message_iter_get_arg_type(&variant) == DBUS_TYPE_ARRAY
                && dbus_message_iter_get_element_type(&variant) == DBUS_TYPE_BYTE) {
                DBusMessageIter bytes;
                dbus_message_iter_recurse(&variant, &bytes);
                const char* data = nullptr;
                int count = 0;
                dbus_message_iter_get_fixed_array(&bytes, &data, &count);
                call.byte_options[name] = data ? std::string(data, static_cast<size_t>(count)) : std::string();
            } else if (name == "filters") {
                parse_filters(variant, call);
            }
            dbus_message_iter_next(&options);
        }
        return call;
    }

    void send_response(DBusConnection* connection, const std::string& request_path, const PortalReply& reply)
    {
        DBusMessage* signal = dbus_message_new_signal(request_path.c_str(), k_request_iface, "Response");
        DBusMessageIter args;
        dbus_message_iter_init_append(signal, &args);
        dbus_uint32_t code = reply.code;
        dbus_message_iter_append_basic(&args, DBUS_TYPE_UINT32, &code);

        DBusMessageIter results;
        dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY, "{sv}", &results);
        if (reply.uri && reply.include_uris) {
            DBusMessageIter entry, variant, uris;
            const char* key = "uris";
            const char* uri = reply.uri->c_str();
            dbus_message_iter_open_container(&results, DBUS_TYPE_DICT_ENTRY, nullptr, &entry);
            dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, static_cast<const void*>(&key));
            dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "as", &variant);
            dbus_message_iter_open_container(&variant, DBUS_TYPE_ARRAY, "s", &uris);
            dbus_message_iter_append_basic(&uris, DBUS_TYPE_STRING, static_cast<const void*>(&uri));
            dbus_message_iter_close_container(&variant, &uris);
            dbus_message_iter_close_container(&entry, &variant);
            dbus_message_iter_close_container(&results, &entry);
        }
        dbus_message_iter_close_container(&args, &results);

        dbus_connection_send(connection, signal, nullptr);
        dbus_connection_flush(connection);
        dbus_message_unref(signal);
    }

    class MockPortal {
    public:
        MockPortal()
        {
            DBusError error;
            dbus_error_init(&error);
            m_connection = dbus_bus_get_private(DBUS_BUS_SESSION, &error);
            if (!m_connection) {
                dbus_error_free(&error);
                return;
            }
            dbus_connection_set_exit_on_disconnect(m_connection, FALSE);

            bool owner = false;
            for (int attempt = 0; attempt < 100 && !owner; ++attempt) {
                const int result = dbus_bus_request_name(m_connection, k_portal_name,
                    DBUS_NAME_FLAG_DO_NOT_QUEUE, &error);
                owner = !dbus_error_is_set(&error) && result == DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER;
                dbus_error_free(&error);
                dbus_error_init(&error);
                if (!owner)
                    std::this_thread::sleep_for(10ms);
            }

            if (!owner) {
                dbus_connection_close(m_connection);
                dbus_connection_unref(m_connection);
                m_connection = nullptr;
                return;
            }

            m_thread = std::thread([this] { run(); });
        }

        ~MockPortal()
        {
            m_stop.store(true);
            if (m_thread.joinable())
                m_thread.join();
            if (m_connection) {
                dbus_connection_close(m_connection);
                dbus_connection_unref(m_connection);
            }
        }

        MockPortal(const MockPortal&) = delete;
        MockPortal& operator=(const MockPortal&) = delete;
        MockPortal(MockPortal&&) = delete;
        MockPortal& operator=(MockPortal&&) = delete;

        [[nodiscard]] bool ready() const { return m_connection != nullptr; }

        void queue_reply(PortalReply reply)
        {
            std::scoped_lock lock(m_mutex);
            m_replies.push_back(std::move(reply));
        }

        [[nodiscard]] std::vector<PortalCall> calls() const
        {
            std::scoped_lock lock(m_mutex);
            return m_calls;
        }

    private:
        struct Pending {
            std::string path;
            PortalReply reply;
            std::chrono::steady_clock::time_point due;
        };

        void run()
        {
            while (!m_stop.load()) {
                dbus_connection_read_write(m_connection, 20);
                while (DBusMessage* message = dbus_connection_pop_message(m_connection)) {
                    handle(message);
                    dbus_message_unref(message);
                }
                flush_due();
            }
        }

        void handle(DBusMessage* message)
        {
            const bool open = dbus_message_is_method_call(message, k_chooser_iface, "OpenFile");
            const bool save = dbus_message_is_method_call(message, k_chooser_iface, "SaveFile");
            if (!open && !save)
                return;

            PortalCall call = parse_call(message);
            PortalReply reply;
            size_t index = 0;
            {
                std::scoped_lock lock(m_mutex);
                index = m_calls.size();
                m_calls.push_back(std::move(call));
                if (!m_replies.empty()) {
                    reply = std::move(m_replies.front());
                    m_replies.pop_front();
                }
            }

            const std::string path = "/org/freedesktop/portal/desktop/request/mock/t" + std::to_string(index);
            if (reply.before_reply)
                send_response(m_connection, path, reply);

            DBusMessage* method_return = dbus_message_new_method_return(message);
            DBusMessageIter iter;
            dbus_message_iter_init_append(method_return, &iter);
            const char* path_cstr = path.c_str();
            dbus_message_iter_append_basic(&iter, DBUS_TYPE_OBJECT_PATH, static_cast<const void*>(&path_cstr));
            dbus_connection_send(m_connection, method_return, nullptr);
            dbus_connection_flush(m_connection);
            dbus_message_unref(method_return);

            if (!reply.before_reply)
                m_pending.push_back({ path, std::move(reply), std::chrono::steady_clock::now() + 150ms });
        }

        void flush_due()
        {
            const auto now = std::chrono::steady_clock::now();
            for (auto it = m_pending.begin(); it != m_pending.end();) {
                if (it->due <= now) {
                    send_response(m_connection, it->path, it->reply);
                    it = m_pending.erase(it);
                } else {
                    ++it;
                }
            }
        }

        DBusConnection* m_connection { nullptr };
        std::thread m_thread;
        std::atomic<bool> m_stop { false };
        mutable std::mutex m_mutex;
        std::vector<PortalCall> m_calls;
        std::deque<PortalReply> m_replies;
        std::vector<Pending> m_pending;
    };

    class PortalRig {
    public:
        explicit PortalRig(bool with_portal = true)
            : m_ready(shared_bus().running())
        {
            if (m_ready && with_portal) {
                portal = std::make_unique<MockPortal>();
                m_ready = portal->ready();
            }
        }

        PortalRig(const PortalRig&) = delete;
        PortalRig& operator=(const PortalRig&) = delete;
        PortalRig(PortalRig&&) = delete;
        PortalRig& operator=(PortalRig&&) = delete;

        [[nodiscard]] bool ready() const { return m_ready; }

        std::unique_ptr<MockPortal> portal;

    private:
        bool m_ready;
    };

    class DialogOutcome {
    public:
        void set(Core::FileDialogResult result)
        {
            {
                std::scoped_lock lock(m_mutex);
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
            std::scoped_lock lock(m_mutex);
            return m_calls;
        }

        [[nodiscard]] Core::FileDialogResult result() const
        {
            std::scoped_lock lock(m_mutex);
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

    struct DialogKind {
        const char* name;
        bool save;
        const char* method;
        const char* title;
    };

    void present(Core::DBusBackend& backend, const DialogKind& kind,
        const std::shared_ptr<DialogOutcome>& outcome,
        std::vector<Core::SystemFileFilter> filters = {}, std::filesystem::path start_dir = {})
    {
        if (kind.save)
            backend.save_file(record_into(outcome), "take.wav", std::move(filters), std::move(start_dir));
        else
            backend.open_file(record_into(outcome), std::move(filters), std::move(start_dir));
    }

    class DBusDialogTest : public ::testing::TestWithParam<DialogKind> { };

    const auto k_kind_name = [](const ::testing::TestParamInfo<DialogKind>& info) {
        return std::string(info.param.name);
    };

    const auto k_kinds = ::testing::Values(
        DialogKind { "Open", false, "OpenFile", "Open File" },
        DialogKind { "Save", true, "SaveFile", "Save File" });

#define REQUIRE_PORTAL(rig)                                           \
    do {                                                              \
        if (!(rig).ready())                                           \
            GTEST_SKIP() << "A private D-Bus session is unavailable"; \
    } while (false)

    // ------------------------------------------------------------------
    // Lifecycle
    // ------------------------------------------------------------------

    TEST(DBusBackendTest, InitializeAndShutdownAreIdempotent)
    {
        PortalRig rig(false);
        REQUIRE_PORTAL(rig);
        Core::DBusBackend backend;
        EXPECT_FALSE(backend.is_initialized());

        EXPECT_TRUE(backend.initialize());
        EXPECT_TRUE(backend.is_initialized());
        EXPECT_TRUE(backend.initialize());

        backend.shutdown();
        EXPECT_FALSE(backend.is_initialized());
        EXPECT_NO_THROW(backend.shutdown());

        EXPECT_TRUE(backend.initialize());
        EXPECT_TRUE(backend.is_initialized());
        backend.shutdown();
    }

    TEST_P(DBusDialogTest, UninitializedBackendReportsBackendErrorSynchronously)
    {
        Core::DBusBackend backend;
        auto outcome = std::make_shared<DialogOutcome>();

        present(backend, GetParam(), outcome);

        EXPECT_EQ(outcome->calls(), 1);
        const auto result = outcome->result();
        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(result.error(), Core::SystemDialogError::BackendError);
    }

    TEST_P(DBusDialogTest, ShutdownBackendReportsBackendErrorSynchronously)
    {
        PortalRig rig(false);
        REQUIRE_PORTAL(rig);
        Core::DBusBackend backend;
        ASSERT_TRUE(backend.initialize());
        backend.shutdown();
        auto outcome = std::make_shared<DialogOutcome>();

        present(backend, GetParam(), outcome);

        EXPECT_EQ(outcome->calls(), 1);
        const auto result = outcome->result();
        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(result.error(), Core::SystemDialogError::BackendError);
    }

    TEST_P(DBusDialogTest, MissingPortalServiceReportsBackendErrorSynchronously)
    {
        PortalRig rig(false);
        REQUIRE_PORTAL(rig);
        Core::DBusBackend backend;
        ASSERT_TRUE(backend.initialize());
        auto outcome = std::make_shared<DialogOutcome>();

        present(backend, GetParam(), outcome);

        EXPECT_EQ(outcome->calls(), 1);
        const auto result = outcome->result();
        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(result.error(), Core::SystemDialogError::BackendError);
        backend.shutdown();
    }

    // ------------------------------------------------------------------
    // Portal responses
    // ------------------------------------------------------------------

    TEST_P(DBusDialogTest, ChosenFileUriIsDeliveredAsAPathExactlyOnce)
    {
        PortalRig rig;
        REQUIRE_PORTAL(rig);
        rig.portal->queue_reply({ .code = 0, .uri = "file:///home/user/Mix%20Down.wav" });
        Core::DBusBackend backend;
        ASSERT_TRUE(backend.initialize());
        auto outcome = std::make_shared<DialogOutcome>();

        present(backend, GetParam(), outcome);

        ASSERT_TRUE(outcome->wait(5s));
        std::this_thread::sleep_for(100ms);
        EXPECT_EQ(outcome->calls(), 1);
        const auto result = outcome->result();
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result.value(), std::filesystem::path("/home/user/Mix Down.wav"));
        backend.shutdown();
    }

    TEST_P(DBusDialogTest, PercentEncodedUtf8IsDecoded)
    {
        PortalRig rig;
        REQUIRE_PORTAL(rig);
        rig.portal->queue_reply({ .code = 0, .uri = "file:///tmp/%E2%82%AC%20caf%C3%A9.wav" });
        Core::DBusBackend backend;
        ASSERT_TRUE(backend.initialize());
        auto outcome = std::make_shared<DialogOutcome>();

        present(backend, GetParam(), outcome);

        ASSERT_TRUE(outcome->wait(5s));
        const auto result = outcome->result();
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->string(), "/tmp/\xE2\x82\xAC caf\xC3\xA9.wav");
        backend.shutdown();
    }

    TEST_P(DBusDialogTest, InvalidPercentSequencesAreKeptLiterally)
    {
        PortalRig rig;
        REQUIRE_PORTAL(rig);
        rig.portal->queue_reply({ .code = 0, .uri = "file:///tmp/100%zz%41.wav" });
        Core::DBusBackend backend;
        ASSERT_TRUE(backend.initialize());
        auto outcome = std::make_shared<DialogOutcome>();

        present(backend, GetParam(), outcome);

        ASSERT_TRUE(outcome->wait(5s));
        const auto result = outcome->result();
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->string(), "/tmp/100%zzA.wav");
        backend.shutdown();
    }

    TEST_P(DBusDialogTest, UserCancelDeliversCancelled)
    {
        PortalRig rig;
        REQUIRE_PORTAL(rig);
        rig.portal->queue_reply({ .code = 1 });
        Core::DBusBackend backend;
        ASSERT_TRUE(backend.initialize());
        auto outcome = std::make_shared<DialogOutcome>();

        present(backend, GetParam(), outcome);

        ASSERT_TRUE(outcome->wait(5s));
        std::this_thread::sleep_for(100ms);
        EXPECT_EQ(outcome->calls(), 1);
        const auto result = outcome->result();
        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(result.error(), Core::SystemDialogError::Cancelled);
        backend.shutdown();
    }

    TEST_P(DBusDialogTest, OtherNonZeroResponseDeliversBackendError)
    {
        PortalRig rig;
        REQUIRE_PORTAL(rig);
        rig.portal->queue_reply({ .code = 2 });
        Core::DBusBackend backend;
        ASSERT_TRUE(backend.initialize());
        auto outcome = std::make_shared<DialogOutcome>();

        present(backend, GetParam(), outcome);

        ASSERT_TRUE(outcome->wait(5s));
        const auto result = outcome->result();
        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(result.error(), Core::SystemDialogError::BackendError);
        backend.shutdown();
    }

    TEST_P(DBusDialogTest, SuccessWithoutUrisDeliversBackendError)
    {
        PortalRig rig;
        REQUIRE_PORTAL(rig);
        rig.portal->queue_reply({ .code = 0, .uri = "file:///tmp/x.wav", .include_uris = false });
        Core::DBusBackend backend;
        ASSERT_TRUE(backend.initialize());
        auto outcome = std::make_shared<DialogOutcome>();

        present(backend, GetParam(), outcome);

        ASSERT_TRUE(outcome->wait(5s));
        const auto result = outcome->result();
        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(result.error(), Core::SystemDialogError::BackendError);
        backend.shutdown();
    }

    TEST(DBusBackendTest, SequentialDialogsEachReceiveTheirOwnResponse)
    {
        PortalRig rig;
        REQUIRE_PORTAL(rig);
        rig.portal->queue_reply({ .code = 0, .uri = "file:///tmp/first.wav" });
        rig.portal->queue_reply({ .code = 1 });
        rig.portal->queue_reply({ .code = 0, .uri = "file:///tmp/third.wav" });
        Core::DBusBackend backend;
        ASSERT_TRUE(backend.initialize());
        std::vector<std::shared_ptr<DialogOutcome>> outcomes;

        for (int i = 0; i < 3; ++i) {
            auto outcome = std::make_shared<DialogOutcome>();
            backend.open_file(record_into(outcome), {}, {});
            ASSERT_TRUE(outcome->wait(5s)) << i;
            outcomes.push_back(outcome);
        }

        ASSERT_TRUE(outcomes[0]->result().has_value());
        EXPECT_EQ(outcomes[0]->result().value(), std::filesystem::path("/tmp/first.wav"));
        ASSERT_FALSE(outcomes[1]->result().has_value());
        EXPECT_EQ(outcomes[1]->result().error(), Core::SystemDialogError::Cancelled);
        ASSERT_TRUE(outcomes[2]->result().has_value());
        EXPECT_EQ(outcomes[2]->result().value(), std::filesystem::path("/tmp/third.wav"));
        backend.shutdown();
    }

    // ------------------------------------------------------------------
    // Portal request marshalling
    // ------------------------------------------------------------------

    TEST_P(DBusDialogTest, RequestUsesTheMatchingPortalMethodAndTitle)
    {
        PortalRig rig;
        REQUIRE_PORTAL(rig);
        rig.portal->queue_reply({ .code = 1 });
        Core::DBusBackend backend;
        ASSERT_TRUE(backend.initialize());
        auto outcome = std::make_shared<DialogOutcome>();

        present(backend, GetParam(), outcome);

        ASSERT_TRUE(outcome->wait(5s));
        const auto calls = rig.portal->calls();
        ASSERT_EQ(calls.size(), 1U);
        EXPECT_EQ(calls.front().method, GetParam().method);
        EXPECT_EQ(calls.front().title, GetParam().title);
        EXPECT_TRUE(calls.front().parent.empty());
        backend.shutdown();
    }

    TEST_P(DBusDialogTest, RequestWithoutFiltersOrDirectoryCarriesNoFilterOrFolderOptions)
    {
        PortalRig rig;
        REQUIRE_PORTAL(rig);
        rig.portal->queue_reply({ .code = 1 });
        Core::DBusBackend backend;
        ASSERT_TRUE(backend.initialize());
        auto outcome = std::make_shared<DialogOutcome>();

        present(backend, GetParam(), outcome);

        ASSERT_TRUE(outcome->wait(5s));
        const auto calls = rig.portal->calls();
        ASSERT_EQ(calls.size(), 1U);
        for (const auto& key : calls.front().option_keys) {
            EXPECT_NE(key, "filters");
            EXPECT_NE(key, "current_folder");
        }
        EXPECT_TRUE(calls.front().filters.empty());
        backend.shutdown();
    }

    TEST_P(DBusDialogTest, FiltersAreMarshalledAsGlobPatterns)
    {
        PortalRig rig;
        REQUIRE_PORTAL(rig);
        rig.portal->queue_reply({ .code = 1 });
        Core::DBusBackend backend;
        ASSERT_TRUE(backend.initialize());
        auto outcome = std::make_shared<DialogOutcome>();

        present(backend, GetParam(), outcome,
            { { "Audio", { "wav", "flac" } }, { "All files", { "*" } } });

        ASSERT_TRUE(outcome->wait(5s));
        const auto calls = rig.portal->calls();
        ASSERT_EQ(calls.size(), 1U);
        const auto& filters = calls.front().filters;
        ASSERT_EQ(filters.size(), 2U);
        EXPECT_EQ(filters[0].label, "Audio");
        EXPECT_EQ(filters[0].globs, (std::vector<std::string> { "*.wav", "*.flac" }));
        EXPECT_EQ(filters[0].types, (std::vector<uint32_t> { 0, 0 }));
        EXPECT_EQ(filters[1].label, "All files");
        EXPECT_EQ(filters[1].globs, (std::vector<std::string> { "*" }));
        backend.shutdown();
    }

    TEST_P(DBusDialogTest, StartDirectoryIsSentAsANulTerminatedByteArray)
    {
        PortalRig rig;
        REQUIRE_PORTAL(rig);
        rig.portal->queue_reply({ .code = 1 });
        Core::DBusBackend backend;
        ASSERT_TRUE(backend.initialize());
        auto outcome = std::make_shared<DialogOutcome>();

        present(backend, GetParam(), outcome, {}, "/tmp/mix dir");

        ASSERT_TRUE(outcome->wait(5s));
        const auto calls = rig.portal->calls();
        ASSERT_EQ(calls.size(), 1U);
        ASSERT_EQ(calls.front().signatures.count("current_folder"), 1U);
        EXPECT_EQ(calls.front().signatures.at("current_folder"), "ay");
        ASSERT_EQ(calls.front().byte_options.count("current_folder"), 1U);
        EXPECT_EQ(calls.front().byte_options.at("current_folder"), std::string("/tmp/mix dir\0", 13));
        backend.shutdown();
    }

    TEST_P(DBusDialogTest, ResponseSentBeforeTheCallReturnsIsNotLost)
    {
        PortalRig rig;
        REQUIRE_PORTAL(rig);
        rig.portal->queue_reply({ .code = 0, .uri = "file:///tmp/instant.wav", .before_reply = true });
        Core::DBusBackend backend;
        ASSERT_TRUE(backend.initialize());
        auto outcome = std::make_shared<DialogOutcome>();

        present(backend, GetParam(), outcome);

        ASSERT_TRUE(outcome->wait(5s));
        const auto result = outcome->result();
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result.value(), std::filesystem::path("/tmp/instant.wav"));
        backend.shutdown();
    }

    TEST(DBusBackendTest, SaveSuggestsTheGivenFileNameAndOpenDoesNot)
    {
        PortalRig rig;
        REQUIRE_PORTAL(rig);
        rig.portal->queue_reply({ .code = 1 });
        rig.portal->queue_reply({ .code = 1 });
        Core::DBusBackend backend;
        ASSERT_TRUE(backend.initialize());
        auto saved = std::make_shared<DialogOutcome>();
        auto opened = std::make_shared<DialogOutcome>();

        backend.save_file(record_into(saved), "take.wav", {}, {});
        ASSERT_TRUE(saved->wait(5s));
        backend.open_file(record_into(opened), {}, {});
        ASSERT_TRUE(opened->wait(5s));

        const auto calls = rig.portal->calls();
        ASSERT_EQ(calls.size(), 2U);
        ASSERT_EQ(calls[0].string_options.count("current_name"), 1U);
        EXPECT_EQ(calls[0].string_options.at("current_name"), "take.wav");
        EXPECT_EQ(calls[1].string_options.count("current_name"), 0U);
        backend.shutdown();
    }

    INSTANTIATE_TEST_SUITE_P(DBusBackend, DBusDialogTest, k_kinds, k_kind_name);

} // namespace
} // namespace MayaFlux::Test

#endif
