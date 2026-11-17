#pragma once

#include <expected>

namespace MayaFlux::Platform {

/**
 * @brief Why an acquisition request was refused, with every path that was checked.
 */
struct AcquireFailure {
    enum class Reason : std::uint8_t {
        EmptyRequest,
        InvalidName,
        Missing,
        NotDirectory,
        NotRegularFile,
        MarkerMissing
    };

    Reason reason;
    std::string request;
    std::vector<std::filesystem::path> tried;
};

template <typename T>
using Acquired = std::expected<T, AcquireFailure>;

/**
 * @brief A shared library at an explicit file path, absolute or relative to the working directory.
 */
struct LibraryFromPath {
    std::filesystem::path path;
};

/**
 * @brief A shared library given by name, searched under one prefix.
 *
 * Candidates are tried in this order. First the platform file name built from base_name:
 * "lib<name>.so" on Linux, "lib<name>.dylib" on macOS, "<name>.dll" on Windows. If
 * base_name starts with "lib", the same decoration of the rest of it. Then base_name is
 * tried as the exact file name. Each round searches lib, lib64, then bin.
 */
struct LibraryFromPrefix {
    std::filesystem::path prefix;
    std::string base_name;
};

/**
 * @brief A shared library given by name, searched directly inside each of several directories.
 *
 * For directories that hold libraries themselves, such as /usr/lib, where LibraryFromPrefix
 * would look in lib, lib64 and bin below them. The names are the ones LibraryFromPrefix
 * tries, in the same order, and each name is tried in every directory, in the order given,
 * before the next name. Relative directories resolve against the working directory. Empty
 * entries are ignored, and directories that do not exist are recorded as tried.
 */
struct LibraryFromDirectories {
    std::vector<std::filesystem::path> directories;
    std::string base_name;
};

/**
 * @brief A shared library the system loader resolves by its exact file name.
 *
 * Nothing is searched here. The name is passed to the loader unchanged.
 */
struct LibraryFromSystem {
    std::string file_name;
};

using LibrarySource = std::variant<LibraryFromPath, LibraryFromPrefix, LibraryFromDirectories, LibraryFromSystem>;

/**
 * @brief Where a library comes from after acquisition.
 *
 * path is set for LibraryFromPath and LibraryFromPrefix, always absolute. resolved_by_system
 * is true for LibraryFromSystem, where path is empty and file_name is what the loader receives.
 */
struct LibraryLocation {
    std::filesystem::path path;
    std::string file_name;
    bool resolved_by_system;
};

namespace detail {

    template <typename... Ts>
    struct Overloaded : Ts... {
        using Ts::operator()...;
    };

    template <typename... Ts>
    Overloaded(Ts...) -> Overloaded<Ts...>;

    inline AcquireFailure make_failure(AcquireFailure::Reason reason,
        std::string request,
        std::vector<std::filesystem::path> tried = {})
    {
        return AcquireFailure { .reason = reason, .request = std::move(request), .tried = std::move(tried) };
    }

    inline bool is_single_component(std::string_view name)
    {
        const std::filesystem::path path { std::string(name) };
        return path.filename() == path && !path.has_root_path() && !path.has_parent_path();
    }

    inline bool has_control_character(std::string_view name)
    {
        return std::ranges::any_of(name, [](char c) {
            const auto byte = static_cast<unsigned char>(c);
            return byte < 0x20 || byte == 0x7F;
        });
    }

    inline std::optional<AcquireFailure> check_file_name(std::string_view name)
    {
        if (name.empty()) {
            return make_failure(AcquireFailure::Reason::EmptyRequest, "");
        }
        const bool unsafe = name == "." || name == ".." || !is_single_component(name)
            || has_control_character(name)
#if defined(MAYAFLUX_PLATFORM_WINDOWS)
            || name.find(':') != std::string_view::npos
#endif
            ;
        if (unsafe) {
            return make_failure(AcquireFailure::Reason::InvalidName, std::string(name));
        }
        return std::nullopt;
    }

    inline std::expected<std::filesystem::path, AcquireFailure> resolve_absolute(const std::filesystem::path& path)
    {
        if (path.empty()) {
            return std::unexpected(make_failure(AcquireFailure::Reason::EmptyRequest, ""));
        }
        std::error_code ec;
        std::filesystem::path absolute = std::filesystem::absolute(path, ec);
        if (ec) {
            return std::unexpected(make_failure(AcquireFailure::Reason::Missing, path.string(), { path }));
        }
        return absolute;
    }

    inline std::string shared_library_file_name(const std::string& base_name)
    {
#if defined(MAYAFLUX_PLATFORM_WINDOWS)
        return base_name + ".dll";
#elif defined(MAYAFLUX_PLATFORM_MACOS)
        return "lib" + base_name + ".dylib";
#else
        return "lib" + base_name + ".so";
#endif
    }

    inline bool path_is_regular_file(const std::filesystem::path& path)
    {
        std::error_code ec;
        return std::filesystem::is_regular_file(path, ec) && !ec;
    }

    inline bool path_is_directory(const std::filesystem::path& path)
    {
        std::error_code ec;
        return std::filesystem::is_directory(path, ec) && !ec;
    }

    inline bool path_exists(const std::filesystem::path& path)
    {
        std::error_code ec;
        return std::filesystem::exists(path, ec) && !ec;
    }

    inline Acquired<LibraryLocation> acquire_from_path(const LibraryFromPath& from)
    {
        auto absolute = resolve_absolute(from.path);
        if (!absolute) {
            return std::unexpected(absolute.error());
        }
        if (!path_exists(*absolute)) {
            return std::unexpected(make_failure(AcquireFailure::Reason::Missing, from.path.string(), { *absolute }));
        }
        if (!path_is_regular_file(*absolute)) {
            return std::unexpected(make_failure(AcquireFailure::Reason::NotRegularFile, from.path.string(), { *absolute }));
        }
        return LibraryLocation { .path = *absolute, .file_name = absolute->filename().string(), .resolved_by_system = false };
    }

    inline std::vector<std::string> prefix_file_names(const std::string& base_name)
    {
        std::vector<std::string> names { shared_library_file_name(base_name) };
        if (base_name.size() > 3 && base_name.starts_with("lib")) {
            names.push_back(shared_library_file_name(base_name.substr(3)));
        }
        names.push_back(base_name);
        return names;
    }

    inline Acquired<LibraryLocation> acquire_from_prefix(const LibraryFromPrefix& from)
    {
        if (auto failure = check_file_name(from.base_name)) {
            return std::unexpected(*failure);
        }
        auto absolute = resolve_absolute(from.prefix);
        if (!absolute) {
            return std::unexpected(absolute.error());
        }
        if (!path_is_directory(*absolute)) {
            return std::unexpected(make_failure(AcquireFailure::Reason::NotDirectory, from.base_name, { *absolute }));
        }

        std::vector<std::filesystem::path> tried;
        for (const std::string& file_name : prefix_file_names(from.base_name)) {
            for (const char* subdir : { "lib", "lib64", "bin" }) {
                const std::filesystem::path candidate = *absolute / subdir / file_name;
                tried.push_back(candidate);
                if (path_is_regular_file(candidate)) {
                    return LibraryLocation { .path = candidate, .file_name = file_name, .resolved_by_system = false };
                }
            }
        }
        return std::unexpected(make_failure(AcquireFailure::Reason::Missing, from.base_name, std::move(tried)));
    }

    inline Acquired<LibraryLocation> acquire_from_directories(const LibraryFromDirectories& from)
    {
        if (auto failure = check_file_name(from.base_name)) {
            return std::unexpected(*failure);
        }

        std::vector<std::filesystem::path> directories;
        for (const std::filesystem::path& directory : from.directories) {
            if (directory.empty()) {
                continue;
            }
            auto absolute = resolve_absolute(directory);
            if (!absolute) {
                return std::unexpected(absolute.error());
            }
            directories.push_back(std::move(*absolute));
        }
        if (directories.empty()) {
            return std::unexpected(make_failure(AcquireFailure::Reason::EmptyRequest, from.base_name));
        }

        std::vector<std::filesystem::path> tried;
        for (const std::string& file_name : prefix_file_names(from.base_name)) {
            for (const std::filesystem::path& directory : directories) {
                const std::filesystem::path candidate = directory / file_name;
                tried.push_back(candidate);
                if (path_is_regular_file(candidate)) {
                    return LibraryLocation { .path = candidate, .file_name = file_name, .resolved_by_system = false };
                }
            }
        }
        return std::unexpected(make_failure(AcquireFailure::Reason::Missing, from.base_name, std::move(tried)));
    }

    inline Acquired<LibraryLocation> acquire_from_system(const LibraryFromSystem& from)
    {
        if (auto failure = check_file_name(from.file_name)) {
            return std::unexpected(*failure);
        }
        return LibraryLocation { .path = {}, .file_name = from.file_name, .resolved_by_system = true };
    }

}

/**
 * @brief Resolve a library request to a location the loader can open.
 *
 * Each source kind has its own rule, and the compiler requires every kind to be handled.
 * Relative paths resolve against the working directory. Names must be plain file names.
 * A refusal lists the paths that were checked.
 *
 * @param source The request, as one of the LibrarySource alternatives.
 * @return The resolved location, or the reason the request was refused.
 */
[[nodiscard]] inline Acquired<LibraryLocation> acquire_library(const LibrarySource& source)
{
    return std::visit(detail::Overloaded {
                          [](const LibraryFromPath& from) { return detail::acquire_from_path(from); },
                          [](const LibraryFromPrefix& from) { return detail::acquire_from_prefix(from); },
                          [](const LibraryFromDirectories& from) { return detail::acquire_from_directories(from); },
                          [](const LibraryFromSystem& from) { return detail::acquire_from_system(from); } },
        source);
}

/**
 * @brief Validate a header root: an existing directory, optionally containing a marker file.
 *
 * @param directory The directory the JIT will add to its include search path. Relative
 *        paths resolve against the working directory.
 * @param marker_file A plain file name that must be a regular file inside directory, or empty for none.
 * @return The absolute directory, or the reason it was refused.
 */
[[nodiscard]] inline Acquired<std::filesystem::path> acquire_header_root(
    const std::filesystem::path& directory,
    std::string_view marker_file = {})
{
    auto absolute = detail::resolve_absolute(directory);
    if (!absolute) {
        return std::unexpected(absolute.error());
    }
    if (!detail::path_exists(*absolute)) {
        return std::unexpected(detail::make_failure(AcquireFailure::Reason::Missing, directory.string(), { *absolute }));
    }
    if (!detail::path_is_directory(*absolute)) {
        return std::unexpected(detail::make_failure(AcquireFailure::Reason::NotDirectory, directory.string(), { *absolute }));
    }
    if (!marker_file.empty()) {
        if (auto failure = detail::check_file_name(marker_file)) {
            return std::unexpected(*failure);
        }
        const std::filesystem::path marker = *absolute / marker_file;
        if (!detail::path_is_regular_file(marker)) {
            return std::unexpected(detail::make_failure(AcquireFailure::Reason::MarkerMissing, directory.string(), { marker }));
        }
    }
    return *absolute;
}

}
