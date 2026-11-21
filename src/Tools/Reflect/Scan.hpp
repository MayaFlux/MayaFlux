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

/** @brief Registers the matchers of one tool on the MatchFinder a scan runs. */
using AttachMatchers = std::function<void(clang::ast_matchers::MatchFinder&)>;

/**
 * @brief Parses every header of @p input in one translation unit and runs the matchers @p attach registers.
 *
 * The translation unit and the arguments are chosen as scan() below describes.
 *
 * @return false with @p error set when the database, the command or Clang fails.
 */
[[nodiscard]] bool scan(const ScanInput& input, const AttachMatchers& attach, std::string& error);

/**
 * @struct BuildScanInput
 * @brief What a build scan needs: a ScanInput, plus optional explicit compile arguments.
 *
 * With compile_args empty the command is borrowed from the compile database as
 * scan() does. Otherwise compile_args are the whole command line, and neither
 * a database nor a translation unit is read.
 */
struct BuildScanInput {
    ScanInput scan;
    std::vector<std::string> compile_args;
};

/**
 * @brief scan() for a build step: also reports the files the scan read.
 *
 * @p dependencies receives every non-system file the preprocessor opened, as
 * absolute paths, without the scan's own temporary files.
 *
 * @return false with @p error set when the database, the command or Clang fails.
 */
[[nodiscard]] bool scan_for_build(const BuildScanInput& input, const AttachMatchers& attach,
    std::vector<std::filesystem::path>& dependencies, std::string& error);

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
