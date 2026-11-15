#pragma once

#include "FunctionRequest.hpp"

namespace MayaFlux::Reflect::Tool {

/** @brief The suffix of a function sidecar: Name.hpp becomes Name.signatures.hpp beside it. */
inline constexpr std::string_view function_sidecar_suffix = ".signatures.hpp";

/** @brief Text of the sidecar for one header, one Signature specialization per function, sorted by name. */
[[nodiscard]] std::string render_function_sidecar(const HeaderFunctions& functions);

/** @brief The line after which the sidecar include may appear: the last marker of the header. */
[[nodiscard]] unsigned last_function_line(const HeaderFunctions& functions);

/**
 * @brief Compares the function sidecars on disk with what the generator would write, touching nothing.
 *
 * Reports, one message per problem: a header with MF_DESCRIBE_FUNCTION text that produced no
 * request, a missing sidecar, a sidecar that differs, a missing, repeated or misplaced include,
 * and a sidecar in @p existing whose header has no request left.
 *
 * @param headers Every header the scan covered.
 * @param existing Sidecars found on disk under the roots, for the orphan check. Empty skips it.
 * @return No messages when everything is current.
 */
[[nodiscard]] std::vector<std::string> check_function_sidecars(
    const FunctionMap& requests,
    const std::vector<std::filesystem::path>& headers,
    const std::vector<std::filesystem::path>& existing);

}
