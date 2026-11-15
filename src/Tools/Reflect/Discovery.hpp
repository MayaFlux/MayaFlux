#pragma once

namespace MayaFlux::Reflect::Tool {

/**
 * @brief Finds headers under @p roots that contain an MF_DESCRIBE request.
 *
 * A root may be a directory searched recursively or a single header. A request
 * is a line that starts with MF_DESCRIBE, or has it after ';' or '}', followed
 * by '('. Generated sidecars are skipped. Results are canonical and sorted.
 */
[[nodiscard]] std::vector<std::filesystem::path> discover_headers(const std::vector<std::filesystem::path>& roots);

/** @brief Every Name.describe.hpp under the directories among @p roots, canonical and sorted. */
[[nodiscard]] std::vector<std::filesystem::path> discover_sidecars(const std::vector<std::filesystem::path>& roots);

/**
 * @brief discover_headers() for another request marker, such as MF_DESCRIBE_FUNCTION.
 *
 * The marker must be followed by '(' on its line, so MF_DESCRIBE does not match
 * MF_DESCRIBE_FUNCTION and the reverse. Generated sidecars are skipped.
 */
[[nodiscard]] std::vector<std::filesystem::path> discover_headers(
    const std::vector<std::filesystem::path>& roots, std::string_view marker);

/** @brief discover_sidecars() for another sidecar suffix, such as ".signatures.hpp". */
[[nodiscard]] std::vector<std::filesystem::path> discover_sidecars(
    const std::vector<std::filesystem::path>& roots, std::string_view suffix);

}
