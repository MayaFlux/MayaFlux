#pragma once

#include "Session.hpp"

namespace MayaFlux::Host {

/**
 * @brief Add a directory to the JIT include search path.
 *
 * The directory is checked at call time and only that directory is looked at.
 * Include paths cannot change a session that has started, so after
 * start_session this is refused.
 *
 * @param directory Directory to add, absolute or relative to the working directory.
 * @param marker_file Plain file name that must exist inside directory, or empty for none.
 * @return The absolute directory, or the reason it was refused.
 */
MAYAFLUX_HOST_API Result<std::filesystem::path> add_include_root(
    const std::filesystem::path& directory,
    std::string_view marker_file = {});

/**
 * @brief Include a header in the session.
 *
 * Each root is tried in the order given, without descending into
 * subdirectories. The first root holding file_name wins, and the header is
 * included by its absolute path. The include search path is not changed.
 * Before start_session the include is staged and evaluated at start.
 *
 * @param file_name Plain header file name, e.g. "Dust.hpp".
 * @param roots Directories to look in, in order.
 * @return The absolute header path, or every path that was tried.
 */
MAYAFLUX_HOST_API Result<std::filesystem::path> add_header(
    std::string_view file_name,
    std::span<const std::filesystem::path> roots);

/**
 * @brief Load a shared library from an explicit file path.
 *
 * Loading the same file twice is a no-op. Before start_session the load is
 * staged and performed at start.
 *
 * @param path Path to the library, absolute or relative to the working directory.
 * @return The canonical path, or the reason it was refused or failed to load.
 */
MAYAFLUX_HOST_API Result<std::filesystem::path> load_library(const std::filesystem::path& path);

/**
 * @brief Load a shared library by name from one prefix.
 *
 * Candidates are tried under lib, lib64 and bin of the prefix, using the
 * platform's file name for base_name first and base_name as written last.
 *
 * @param prefix Install prefix to search.
 * @param base_name Plain library name, e.g. "Dust" or "libDust".
 * @return The canonical path, or every path that was tried.
 */
MAYAFLUX_HOST_API Result<std::filesystem::path> load_library_from_prefix(
    const std::filesystem::path& prefix,
    std::string_view base_name);

/**
 * @brief Add a compile flag to the JIT.
 *
 * Flags cannot change a session that has started, so after start_session
 * this is refused.
 *
 * @param flag Compiler flag, e.g. "-DMY_DEFINE".
 * @return Success, or the reason it was refused.
 */
MAYAFLUX_HOST_API Result<void> add_compile_flag(const std::string& flag);

} // namespace MayaFlux::Host
