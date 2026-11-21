#pragma once

namespace MayaFlux::Reflect::Tool {

/**
 * @struct CreatorBuildOptions
 * @brief What `mayaflux_signatures --build` needs to write the Creator files.
 *
 * root, registry and output mean what they do for the default mode. The
 * compile command comes from compile_args when given, else from the database in
 * build_path through translation_unit.
 */
struct CreatorBuildOptions {
    std::filesystem::path root;
    std::filesystem::path registry;
    std::filesystem::path output;
    std::filesystem::path build_path;
    std::filesystem::path translation_unit;
    std::string resource_dir;
    std::vector<std::string> extra_args;
    std::vector<std::string> compile_args;
    std::filesystem::path depfile;
    std::string depfile_target;
};

/**
 * @brief Writes the four Creator files, for a build step.
 *
 * Writes the same text the default mode writes, marks each file as modified, and
 * writes a depfile of the registry and the headers the scan read when
 * options.depfile is set.
 *
 * @return 0 on success, 1 with the reason on stderr otherwise.
 */
[[nodiscard]] int run_creator_build(const CreatorBuildOptions& options);

}
