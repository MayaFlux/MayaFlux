#pragma once

#include "Collector.hpp"

namespace MayaFlux::Reflect::Tool {

/**
 * @struct ScanInput
 * @brief What one scan needs: the compile database, the headers, and flag overrides.
 */
struct ScanInput {
    std::filesystem::path build_path;
    std::filesystem::path translation_unit;
    std::vector<std::filesystem::path> roots;
    std::vector<std::filesystem::path> headers;
    std::string resource_dir;
    std::vector<std::string> extra_args;
    std::vector<std::string> stubs;
};

/**
 * @brief Parses every header of @p input in one translation unit and fills @p collector.
 *
 * The translation unit borrows the compile command of input.translation_unit,
 * or of the first database entry under a root, then the first entry overall.
 * Its source is replaced in memory by the marker definitions and an include of
 * each header. Precompiled-header inputs are dropped from the command.
 *
 * Each name in input.stubs is written as an empty header into the scan's
 * temporary directory, which is added as an include path. A header whose
 * sidecar does not exist yet still parses, and the source tree is not touched.
 *
 * @return false with @p error set when the database, the command or Clang fails.
 */
[[nodiscard]] bool scan(const ScanInput& input, RequestCollector& collector, std::string& error);

}
