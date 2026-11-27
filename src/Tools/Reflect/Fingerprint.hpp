#pragma once

namespace MayaFlux::Reflect::Tool {

/**
 * @brief @p source without comments, blank lines and redundant spacing.
 *
 * Literals are kept as written. Two files with equal results preprocess to
 * the same tokens, so a scan of either finds the same declarations.
 */
[[nodiscard]] std::string code_only(std::string_view source);

/**
 * @struct Fingerprint
 * @brief Everything a build scan's result depends on, reduced to one value.
 */
struct Fingerprint {
    std::filesystem::path executable;
    std::vector<std::string> settings;
    std::vector<std::filesystem::path> dependencies;
    std::vector<std::filesystem::path> outputs;
};

/**
 * @brief The value of @p fingerprint, reading its files.
 *
 * Dependencies count by their code, outputs by their text without carriage
 * returns, the executable by size and modification time.
 *
 * @return std::nullopt when a file cannot be read.
 */
[[nodiscard]] std::optional<std::string> fingerprint_value(const Fingerprint& fingerprint);

/**
 * @brief The dependencies recorded in @p state when its value still matches.
 *
 * @p current supplies the executable, settings and outputs. Its dependencies
 * are replaced by the recorded ones.
 *
 * @return std::nullopt when there is no record or anything differs.
 */
[[nodiscard]] std::optional<std::vector<std::filesystem::path>> unchanged_since(
    const std::filesystem::path& state, Fingerprint current);

/** @return false with @p error set when @p state cannot be written. */
[[nodiscard]] bool record_fingerprint(const std::filesystem::path& state, const Fingerprint& fingerprint, std::string& error);

}
