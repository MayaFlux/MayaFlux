#pragma once

namespace MayaFlux::Reflect::Tool {

/**
 * @brief The Clang resource directory a build scan uses.
 *
 * @p configured when it names a directory, else the output of
 * `clang -print-resource-dir`.
 *
 * @return The directory, or empty when neither exists.
 */
[[nodiscard]] std::string resolve_resource_dir(const std::string& configured);

/**
 * @brief Writes a depfile of the form `target: dependency...` that CMake's DEPFILE reads.
 *
 * Paths use forward slashes. Spaces, hashes and dollar signs are escaped.
 *
 * @return false with @p error set when the file cannot be written.
 */
[[nodiscard]] bool write_depfile(const std::filesystem::path& path, const std::string& target,
    const std::vector<std::filesystem::path>& dependencies, std::string& error);

/**
 * @brief Writes @p content when it differs, then marks @p path as modified now.
 *
 * A build graph compares the timestamp of an output with its inputs. An
 * unchanged file that kept an old timestamp would be rebuilt every time its
 * header changes.
 *
 * @return false with @p error set when the file cannot be written or touched.
 */
[[nodiscard]] bool write_generated(const std::filesystem::path& path, const std::string& content, std::string& error);

/** @brief @p dependencies without files that end in a generated sidecar suffix. */
[[nodiscard]] std::vector<std::filesystem::path> without_sidecars(std::vector<std::filesystem::path> dependencies);

}
