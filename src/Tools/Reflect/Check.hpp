#pragma once

#include "Request.hpp"

namespace MayaFlux::Reflect::Tool {

/**
 * @brief Compares the sidecars on disk with what the generator would write, touching nothing.
 *
 * Reports, one message per problem:
 * - a header with MF_DESCRIBE text that produced no request;
 * - a missing sidecar;
 * - a sidecar that differs, naming the records and members added or removed;
 * - a missing, repeated or misplaced sidecar include;
 * - a sidecar in @p existing whose header has no request left.
 *
 * @param headers Every header the scan covered.
 * @param existing Sidecars found on disk under the roots, for the orphan check. Empty skips it.
 * @return No messages when everything is current.
 */
[[nodiscard]] std::vector<std::string> check_sidecars(
    const RequestMap& requests,
    const std::vector<std::filesystem::path>& headers,
    const std::vector<std::filesystem::path>& existing);

}
