#pragma once

namespace MayaFlux::Reflect::Tool {

/**
 * @struct ReflectBuildOptions
 * @brief What `mayaflux_reflect --build` needs to describe one header.
 *
 * The compile command comes from compile_args when given, else from the
 * database in build_path through translation_unit.
 */
struct ReflectBuildOptions {
    std::filesystem::path header;
    std::filesystem::path build_path;
    std::filesystem::path translation_unit;
    std::string resource_dir;
    std::vector<std::string> extra_args;
    std::vector<std::string> compile_args;
    std::filesystem::path depfile;
    std::string depfile_target;
    bool insert_includes { true };
};

/**
 * @brief Writes the sidecar of one header, for a build step.
 *
 * Inserts the sidecar include when the header lacks it, writes the sidecar, and
 * writes a depfile of the files the scan read when options.depfile is set. A
 * missing sidecar is stubbed in the scan's temporary directory, and no other
 * file in the source tree is touched.
 *
 * @return 0 on success, 1 with the reason on stderr otherwise.
 */
[[nodiscard]] int run_reflect_build(const ReflectBuildOptions& options);

}
