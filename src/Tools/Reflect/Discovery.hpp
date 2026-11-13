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

}
