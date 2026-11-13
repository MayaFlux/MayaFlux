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
};

/**
 * @brief Parses every header of @p input in one translation unit and fills @p collector.
 *
 * The translation unit borrows the compile command of input.translation_unit,
 * or of the first database entry under a root, then the first entry overall.
 * Its source is replaced in memory by the marker definitions and an include of
 * each header. Precompiled-header inputs are dropped from the command.
 *
 * @return false with @p error set when the database, the command or Clang fails.
 */
[[nodiscard]] bool scan(const ScanInput& input, RequestCollector& collector, std::string& error);

}
