#pragma once

namespace MayaFlux::Reflect::Tool {

/**
 * @struct SidecarBuildOptions
 * @brief A build step that parses every header it is given in one translation unit.
 *
 * paths holds headers and directories. A directory contributes the headers
 * under it that hold a request. A header named directly must produce one.
 */
struct SidecarBuildOptions {
    std::vector<std::filesystem::path> paths;
    std::filesystem::path build_path;
    std::filesystem::path translation_unit;
    std::filesystem::path prefix_header;
    std::string resource_dir;
    std::vector<std::string> extra_args;
    std::vector<std::string> compile_args;
    std::filesystem::path depfile;
    std::string depfile_target;
    std::filesystem::path executable;
    bool insert_includes { true };
};

/**
 * @struct SidecarKind
 * @brief What differs between the kinds of sidecar a build step writes.
 */
struct SidecarKind {
    std::string_view marker;
    std::string_view suffix;
    std::string_view noun;
    std::function<void(clang::ast_matchers::MatchFinder&)> attach;
    std::function<const std::vector<std::string>&()> errors;
    std::function<std::optional<std::pair<std::string, unsigned>>(const std::filesystem::path&)> rendered;
};

/**
 * @brief Writes the sidecar of every header, its include when missing, and one depfile.
 *
 * With a depfile, the files read are recorded beside it as `<depfile>.fingerprint`.
 * A later run whose inputs differ only in comments or spacing writes the depfile
 * again without parsing.
 *
 * @return 0 on success, 1 after printing why not.
 */
[[nodiscard]] int run_sidecar_build(const SidecarBuildOptions& options, const SidecarKind& kind);

}
