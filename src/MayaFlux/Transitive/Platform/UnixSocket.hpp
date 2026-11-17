#pragma once

#include "Acquisition.hpp"
#include "Process.hpp"

#if defined(MAYAFLUX_PLATFORM_WINDOWS)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace MayaFlux::Platform {

/**
 * @brief A Unix socket at an explicit file path, absolute or relative to the working directory.
 */
struct UnixSocketFromPath {
    std::filesystem::path path;
};

/**
 * @brief A Unix socket given by file name, placed in the user's runtime directory.
 *
 * The directory is the first of runtime_directory_candidates() that exists.
 */
struct UnixSocketFromName {
    std::string file_name;
};

using UnixSocketSource = std::variant<UnixSocketFromPath, UnixSocketFromName>;

/**
 * @brief A socket path that is safe to bind.
 *
 * replaced_stale is true when a socket file nothing answered on was removed.
 */
struct UnixSocket {
    std::filesystem::path path;
    bool replaced_stale;
};

inline constexpr std::string_view default_unix_socket_name = "mayaflux-lila.sock";

#if defined(MAYAFLUX_PLATFORM_MACOS)
inline constexpr size_t unix_socket_path_limit = 103;
#else
inline constexpr size_t unix_socket_path_limit = 107;
#endif

/**
 * @brief Per-user directories a Unix socket may live in, in the order tried.
 *
 * Read from the environment on every call, so a changed variable is seen at once.
 * Linux: XDG_RUNTIME_DIR, then /run/user/<uid>. macOS: TMPDIR. Windows: TEMP,
 * then TMP. Variables that are empty or not absolute are skipped. Nothing here
 * checks that a directory exists.
 */
[[nodiscard]] inline std::vector<std::filesystem::path> runtime_directory_candidates()
{
    std::vector<std::filesystem::path> directories;

    const auto add_variable = [&directories](const char* name) {
        const std::string value = safe_getenv(name);
        if (value.empty()) {
            return;
        }
        std::filesystem::path directory(value);
        if (directory.is_absolute()) {
            directories.push_back(std::move(directory));
        }
    };

#if defined(MAYAFLUX_PLATFORM_WINDOWS)
    add_variable("TEMP");
    add_variable("TMP");
#elif defined(MAYAFLUX_PLATFORM_MACOS)
    add_variable("TMPDIR");
#else
    add_variable("XDG_RUNTIME_DIR");
    directories.emplace_back("/run/user/" + std::to_string(::geteuid()));
#endif

    return directories;
}

namespace detail {

    inline bool socket_directory_is_safe(const std::filesystem::path& directory)
    {
#if defined(MAYAFLUX_PLATFORM_WINDOWS)
        (void)directory;
        return true;
#else
        std::error_code ec;
        const auto status = std::filesystem::status(directory, ec);
        if (ec) {
            return false;
        }
        const auto permissions = status.permissions();
        const bool others_write = (permissions & std::filesystem::perms::others_write) != std::filesystem::perms::none;
        const bool sticky = (permissions & std::filesystem::perms::sticky_bit) != std::filesystem::perms::none;
        return !others_write || sticky;
#endif
    }

    inline bool entry_is_socket(const std::filesystem::file_status& status, const std::filesystem::path& path)
    {
#if defined(MAYAFLUX_PLATFORM_WINDOWS)
        if (status.type() == std::filesystem::file_type::socket) {
            return true;
        }
        constexpr DWORD af_unix_reparse_tag = 0x80000023;
        WIN32_FIND_DATAW found {};
        const HANDLE handle = FindFirstFileW(path.c_str(), &found);
        if (handle == INVALID_HANDLE_VALUE) {
            return false;
        }
        FindClose(handle);
        return (found.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0
            && found.dwReserved0 == af_unix_reparse_tag;
#else
        (void)path;
        return status.type() == std::filesystem::file_type::socket;
#endif
    }

    inline Acquired<std::filesystem::path> resolve_socket_path(const UnixSocketSource& source)
    {
        return std::visit(Overloaded {
                              [](const UnixSocketFromPath& from) -> Acquired<std::filesystem::path> {
                                  if (has_control_character(from.path.string())) {
                                      return std::unexpected(make_failure(AcquireFailure::Reason::InvalidName, from.path.string()));
                                  }
                                  return resolve_absolute(from.path);
                              },
                              [](const UnixSocketFromName& from) -> Acquired<std::filesystem::path> {
                                  if (auto failure = check_file_name(from.file_name)) {
                                      return std::unexpected(*failure);
                                  }
                                  std::vector<std::filesystem::path> tried;
                                  for (const auto& directory : runtime_directory_candidates()) {
                                      tried.push_back(directory);
                                      if (path_is_directory(directory)) {
                                          return directory / from.file_name;
                                      }
                                  }
                                  return std::unexpected(make_failure(AcquireFailure::Reason::Missing, from.file_name, std::move(tried)));
                              } },
            source);
    }

}

/**
 * @brief Decide whether a Unix socket path can be bound, and clear a stale one.
 *
 * The path is resolved, then refused if it contains a control character (NUL
 * included, since the filesystem would see it cut short), if it is longer than
 * unix_socket_path_limit bytes, if its directory is missing, is not a directory, or is writable by others
 * without the sticky bit (group write is accepted, since group membership is not
 * inspected, and ownership is not checked), or if something other than a socket
 * already sits there. A symbolic link is never followed and never removed. A
 * socket file is probed: if is_alive reports an answer the path is in use and
 * nothing is touched, otherwise the stale file is removed. Nothing is created.
 *
 * The result is advisory. Two processes can pass together, and bind is what
 * refuses the second, so a failed bind must still be handled.
 *
 * On Windows the directory check is skipped, and an entry that is not a
 * directory and whose type is unknown is treated as a socket. Both are unverified.
 *
 * @param source The request.
 * @param is_alive Called with the path of an existing socket file. Return true if a
 *        server answers on it. It may throw, and then nothing is removed.
 * @return The socket path, or the reason it was refused with the paths checked.
 */
template <std::predicate<const std::filesystem::path&> Probe>
[[nodiscard]] Acquired<UnixSocket> acquire_unix_socket(const UnixSocketSource& source, Probe&& is_alive)
{
    auto resolved = detail::resolve_socket_path(source);
    if (!resolved) {
        return std::unexpected(resolved.error());
    }
    const std::filesystem::path path = std::move(*resolved);

    if (path.u8string().size() > unix_socket_path_limit) {
        return std::unexpected(detail::make_failure(AcquireFailure::Reason::TooLong, path.string(), { path }));
    }

    const std::filesystem::path directory = path.parent_path();
    if (!detail::path_exists(directory)) {
        return std::unexpected(detail::make_failure(AcquireFailure::Reason::Missing, path.string(), { directory }));
    }
    if (!detail::path_is_directory(directory)) {
        return std::unexpected(detail::make_failure(AcquireFailure::Reason::NotDirectory, path.string(), { directory }));
    }
    if (!detail::socket_directory_is_safe(directory)) {
        return std::unexpected(detail::make_failure(AcquireFailure::Reason::Unsafe, path.string(), { directory }));
    }

    std::error_code ec;
    const auto entry = std::filesystem::symlink_status(path, ec);
    if (entry.type() == std::filesystem::file_type::not_found) {
        return UnixSocket { .path = path, .replaced_stale = false };
    }
    if (ec) {
        return std::unexpected(detail::make_failure(AcquireFailure::Reason::Unsafe, path.string(), { path }));
    }
    if (!detail::entry_is_socket(entry, path)) {
        return std::unexpected(detail::make_failure(AcquireFailure::Reason::NotSocket, path.string(), { path }));
    }

    if (is_alive(path)) {
        return std::unexpected(detail::make_failure(AcquireFailure::Reason::InUse, path.string(), { path }));
    }

    std::filesystem::remove(path, ec);
    if (ec) {
        return std::unexpected(detail::make_failure(AcquireFailure::Reason::StaleNotRemoved, path.string(), { path }));
    }
    return UnixSocket { .path = path, .replaced_stale = true };
}

/**
 * @brief Make a bound socket file usable by its owner only.
 *
 * Call after bind. On POSIX the file mode becomes owner read and write, whatever
 * the umask was. On Windows this does nothing and the file keeps the access its
 * directory gives it.
 */
[[nodiscard]] inline Acquired<void> restrict_unix_socket_to_owner(const std::filesystem::path& path)
{
#if defined(MAYAFLUX_PLATFORM_WINDOWS)
    (void)path;
    return {};
#else
    std::error_code ec;
    std::filesystem::permissions(path,
        std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
        std::filesystem::perm_options::replace, ec);
    if (ec) {
        return std::unexpected(detail::make_failure(AcquireFailure::Reason::Unsafe, path.string(), { path }));
    }
    return {};
#endif
}

} // namespace MayaFlux::Platform
