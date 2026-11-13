#pragma once

#include "Request.hpp"

namespace MayaFlux::Reflect::Tool {

/** @brief Path of the sidecar for @p header: Name.hpp becomes Name.describe.hpp beside it. */
[[nodiscard]] std::filesystem::path sidecar_path(const std::filesystem::path& header);

/** @brief The whole content of @p path, or nullopt when it cannot be read. */
[[nodiscard]] std::optional<std::string> read_text(const std::filesystem::path& path);

/** @brief Text of the sidecar for one header, with its records sorted by name. */
[[nodiscard]] std::string render_sidecar(const HeaderRequests& requests);

/** @brief Writes an empty sidecar when none exists, so the include never fails a first scan. */
[[nodiscard]] bool ensure_stub(const std::filesystem::path& path, std::string& error);

/** @brief Replaces @p path with @p content only when the content differs. */
[[nodiscard]] bool write_if_changed(const std::filesystem::path& path, const std::string& content, std::string& error);

/**
 * @brief Inserts the sidecar include on the line after the last marker when the header has none.
 *
 * A header that already includes its sidecar anywhere is left untouched, so a
 * misplaced include is reported by check_include and never moved.
 *
 * @param inserted Set to true when the header was edited.
 * @return false with @p error set when the header cannot be read or written.
 */
[[nodiscard]] bool insert_include(const HeaderRequests& requests, bool& inserted, std::string& error);

/**
 * @brief Checks that the header includes its sidecar exactly once, after its last marker.
 * @return A diagnostic, or nullopt when the placement is correct.
 */
[[nodiscard]] std::optional<std::string> check_include(const HeaderRequests& requests);

}
