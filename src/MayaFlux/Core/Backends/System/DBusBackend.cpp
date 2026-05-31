#ifdef MAYAFLUX_PLATFORM_LINUX

#include "DBusBackend.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

#if __has_include(<dbus/dbus.h>)
#include <dbus/dbus.h>
#elif __has_include(<dbus-1.0/dbus/dbus.h>)
#include <dbus-1.0/dbus/dbus.h>
#else
#error "dbus/dbus.h not found"
#endif

namespace MayaFlux::Core {

namespace {

    constexpr const char* k_portal_service = "org.freedesktop.portal.Desktop";
    constexpr const char* k_portal_path = "/org/freedesktop/portal/desktop";
    constexpr const char* k_portal_iface = "org.freedesktop.portal.FileChooser";
    constexpr const char* k_request_iface = "org.freedesktop.portal.Request";
    constexpr const char* k_signal_response = "Response";

    void append_string_variant(DBusMessageIter& arr, const char* key, const char* value)
    {
        DBusMessageIter entry, variant;
        dbus_message_iter_open_container(&arr, DBUS_TYPE_DICT_ENTRY, nullptr, &entry);
        dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, static_cast<const void*>(&key));
        dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "s", &variant);
        dbus_message_iter_append_basic(&variant, DBUS_TYPE_STRING, static_cast<const void*>(&value));
        dbus_message_iter_close_container(&entry, &variant);
        dbus_message_iter_close_container(&arr, &entry);
    }

    /**
     * @brief Append the filters option as a(sa(us)) to an open options dict iter.
     *
     * XDG portal filter format: array of (label, array of (type, pattern)).
     * Type 0 = glob, type 1 = MIME type.
     */
    void append_filters(DBusMessageIter& opts, const std::vector<SystemFileFilter>& filters)
    {
        if (filters.empty()) {
            return;
        }

        DBusMessageIter entry, variant, outer, filter_struct, pattern_arr, pattern_struct;
        const char* key = "filters";

        dbus_message_iter_open_container(&opts, DBUS_TYPE_DICT_ENTRY, nullptr, &entry);
        dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, static_cast<const void*>(&key));
        dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "a(sa(us))", &variant);
        dbus_message_iter_open_container(&variant, DBUS_TYPE_ARRAY, "(sa(us))", &outer);

        for (const auto& f : filters) {
            dbus_message_iter_open_container(&outer, DBUS_TYPE_STRUCT, nullptr, &filter_struct);
            const char* label = f.name.c_str();
            dbus_message_iter_append_basic(&filter_struct, DBUS_TYPE_STRING, static_cast<const void*>(&label));
            dbus_message_iter_open_container(&filter_struct, DBUS_TYPE_ARRAY, "(us)", &pattern_arr);

            for (const auto& ext : f.extensions) {
                std::string glob = (ext == "*") ? "*" : "*." + ext;
                const char* glob_cstr = glob.c_str();
                dbus_uint32_t type = 0;
                dbus_message_iter_open_container(&pattern_arr, DBUS_TYPE_STRUCT, nullptr, &pattern_struct);
                dbus_message_iter_append_basic(&pattern_struct, DBUS_TYPE_UINT32, &type);
                dbus_message_iter_append_basic(&pattern_struct, DBUS_TYPE_STRING, static_cast<const void*>(&glob_cstr));
                dbus_message_iter_close_container(&pattern_arr, &pattern_struct);
            }

            dbus_message_iter_close_container(&filter_struct, &pattern_arr);
            dbus_message_iter_close_container(&outer, &filter_struct);
        }

        dbus_message_iter_close_container(&variant, &outer);
        dbus_message_iter_close_container(&entry, &variant);
        dbus_message_iter_close_container(&opts, &entry);
    }

    /**
     * @brief Block on @p conn until the portal Request signal arrives,
     *        then fire @p callback and return.
     *
     * Runs on a dedicated thread. Does not touch the backend's m_conn;
     * receives its own private connection pointer allocated per-call.
     */
    void dispatch_loop(DBusConnection* conn, std::string request_path, FileDialogCallback callback)
    {
        while (true) {
            dbus_connection_read_write(conn, 100);
            DBusMessage* msg = dbus_connection_pop_message(conn);
            if (!msg) {
                continue;
            }

            const bool is_signal = dbus_message_get_type(msg) == DBUS_MESSAGE_TYPE_SIGNAL;
            const bool on_request = (dbus_message_get_path(msg) == request_path);
            const bool is_response = dbus_message_has_member(msg, k_signal_response);

            if (is_signal && on_request && is_response) {
                dbus_uint32_t response_code = 0;
                DBusMessageIter iter;
                dbus_message_iter_init(msg, &iter);
                dbus_message_iter_get_basic(&iter, &response_code);

                if (response_code != 0) {
                    dbus_message_unref(msg);
                    dbus_connection_close(conn);
                    dbus_connection_unref(conn);
                    callback(std::unexpected(
                        response_code == 1
                            ? SystemDialogError::Cancelled
                            : SystemDialogError::BackendError));
                    return;
                }

                dbus_message_iter_next(&iter);
                DBusMessageIter results;
                dbus_message_iter_recurse(&iter, &results);

                std::string chosen_path;

                while (dbus_message_iter_get_arg_type(&results) == DBUS_TYPE_DICT_ENTRY) {
                    DBusMessageIter kv;
                    dbus_message_iter_recurse(&results, &kv);

                    const char* key = nullptr;
                    dbus_message_iter_get_basic(&kv, static_cast<void*>(&key));

                    if (key && std::string_view(key) == "uris") {
                        dbus_message_iter_next(&kv);
                        DBusMessageIter variant, arr;
                        dbus_message_iter_recurse(&kv, &variant);
                        dbus_message_iter_recurse(&variant, &arr);

                        if (dbus_message_iter_get_arg_type(&arr) == DBUS_TYPE_STRING) {
                            const char* uri = nullptr;
                            dbus_message_iter_get_basic(&arr, static_cast<void*>(&uri));
                            if (uri) {
                                std::string_view sv(uri);
                                if (sv.starts_with("file://")) {
                                    sv.remove_prefix(7);
                                }
                                chosen_path = std::string(sv);
                            }
                        }
                        break;
                    }
                    dbus_message_iter_next(&results);
                }

                dbus_message_unref(msg);
                dbus_connection_close(conn);
                dbus_connection_unref(conn);

                if (chosen_path.empty()) {
                    callback(std::unexpected(SystemDialogError::BackendError));
                } else {
                    callback(std::filesystem::path(chosen_path));
                }
                return;
            }

            dbus_message_unref(msg);
        }
    }

} // namespace

// =============================================================================
// Lifecycle
// =============================================================================

DBusBackend::~DBusBackend()
{
    shutdown();
}

bool DBusBackend::initialize()
{
    if (m_initialized) {
        return true;
    }

    DBusError err;
    dbus_error_init(&err);

    m_conn = dbus_bus_get_private(DBUS_BUS_SESSION, &err);

    if (!m_conn || dbus_error_is_set(&err)) {
        MF_ERROR(Journal::Component::Core, Journal::Context::API,
            "DBusBackend: session bus connection failed: {}",
            dbus_error_is_set(&err) ? err.message : "unknown");
        dbus_error_free(&err);
        return false;
    }

    dbus_connection_set_exit_on_disconnect(m_conn, FALSE);

    m_initialized = true;
    MF_INFO(Journal::Component::Core, Journal::Context::API,
        "DBusBackend initialized");
    return true;
}

void DBusBackend::shutdown()
{
    if (!m_initialized) {
        return;
    }

    dbus_connection_close(m_conn);
    dbus_connection_unref(m_conn);
    m_conn = nullptr;
    m_initialized = false;

    MF_INFO(Journal::Component::Core, Journal::Context::API,
        "DBusBackend shutdown");
}

// =============================================================================
// Dialog operations
// =============================================================================

void DBusBackend::open_file(
    FileDialogCallback callback,
    std::vector<SystemFileFilter> filters,
    std::filesystem::path start_dir)
{
    invoke_portal("OpenFile", std::move(callback), filters, start_dir, {});
}

void DBusBackend::save_file(
    FileDialogCallback callback,
    std::string suggested_name,
    std::vector<SystemFileFilter> filters,
    std::filesystem::path start_dir)
{
    invoke_portal("SaveFile", std::move(callback), filters, start_dir, suggested_name);
}

void DBusBackend::invoke_portal(
    const char* method,
    FileDialogCallback callback,
    const std::vector<SystemFileFilter>& filters,
    const std::filesystem::path& start_dir,
    const std::string& suggested) const
{
    if (!m_initialized) {
        callback(std::unexpected(SystemDialogError::BackendError));
        return;
    }

    DBusError err;
    dbus_error_init(&err);

    // Each dialog call gets its own private connection for the dispatch thread
    DBusConnection* call_conn = dbus_bus_get_private(DBUS_BUS_SESSION, &err);
    if (!call_conn || dbus_error_is_set(&err)) {
        MF_ERROR(Journal::Component::Core, Journal::Context::API,
            "DBusBackend: failed to open per-call connection: {}",
            dbus_error_is_set(&err) ? err.message : "unknown");
        dbus_error_free(&err);
        callback(std::unexpected(SystemDialogError::BackendError));
        return;
    }

    dbus_connection_set_exit_on_disconnect(call_conn, FALSE);

    DBusMessage* call_msg = dbus_message_new_method_call(
        k_portal_service, k_portal_path, k_portal_iface, method);

    if (!call_msg) {
        dbus_connection_close(call_conn);
        dbus_connection_unref(call_conn);
        callback(std::unexpected(SystemDialogError::BackendError));
        return;
    }

    const char* parent = "";
    const char* title = (std::string_view(method) == "SaveFile") ? "Save File" : "Open File";

    DBusMessageIter args, opts;
    dbus_message_iter_init_append(call_msg, &args);
    dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, static_cast<const void*>(&parent));
    dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, static_cast<const void*>(&title));
    dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY, "{sv}", &opts);

    if (!start_dir.empty()) {
        std::string uri = "file://" + start_dir.string();
        append_string_variant(opts, "current_folder", uri.c_str());
    }
    if (!suggested.empty()) {
        append_string_variant(opts, "current_name", suggested.c_str());
    }

    append_filters(opts, filters);

    dbus_message_iter_close_container(&args, &opts);

    DBusMessage* reply = dbus_connection_send_with_reply_and_block(call_conn, call_msg, 5000, &err);
    dbus_message_unref(call_msg);

    if (!reply || dbus_error_is_set(&err)) {
        MF_ERROR(Journal::Component::Core, Journal::Context::API,
            "DBusBackend: portal method call failed: {}",
            dbus_error_is_set(&err) ? err.message : "no reply");
        dbus_error_free(&err);
        dbus_connection_close(call_conn);
        dbus_connection_unref(call_conn);
        callback(std::unexpected(SystemDialogError::BackendError));
        return;
    }

    const char* request_path_cstr = nullptr;
    DBusMessageIter reply_iter;
    dbus_message_iter_init(reply, &reply_iter);
    dbus_message_iter_get_basic(&reply_iter, static_cast<void*>(&request_path_cstr));
    std::string request_path = request_path_cstr ? request_path_cstr : "";
    dbus_message_unref(reply);

    if (request_path.empty()) {
        dbus_connection_close(call_conn);
        dbus_connection_unref(call_conn);
        callback(std::unexpected(SystemDialogError::BackendError));
        return;
    }

    std::string match = "type='signal',path='" + request_path
        + "',interface='" + k_request_iface
        + "',member='" + k_signal_response + "'";

    dbus_bus_add_match(call_conn, match.c_str(), &err);
    dbus_connection_flush(call_conn);

    if (dbus_error_is_set(&err)) {
        MF_ERROR(Journal::Component::Core, Journal::Context::API,
            "DBusBackend: add_match failed: {}", err.message);
        dbus_error_free(&err);
        dbus_connection_close(call_conn);
        dbus_connection_unref(call_conn);
        callback(std::unexpected(SystemDialogError::BackendError));
        return;
    }

    std::thread(dispatch_loop, call_conn, std::move(request_path), std::move(callback)).detach();
}

} // namespace MayaFlux::Core

#endif // MAYAFLUX_PLATFORM_LINUX
