#include "ReflectBuild.hpp"

#include "Build.hpp"
#include "Collector.hpp"
#include "Scan.hpp"
#include "Sidecar.hpp"

namespace MayaFlux::Reflect::Tool {

namespace {

    int fail(const std::string& message)
    {
        std::cerr << message << '\n';
        return 1;
    }

}

int run_reflect_build(const ReflectBuildOptions& options)
{
    std::error_code code;
    const std::filesystem::path header = std::filesystem::canonical(options.header, code);
    if (code) {
        return fail("cannot read header " + options.header.string());
    }
    if (options.compile_args.empty() && options.translation_unit.empty()) {
        return fail("--build needs --translation-unit with a compile database, or --compile-arg");
    }

    const std::string resource_dir = resolve_resource_dir(options.resource_dir);
    if (resource_dir.empty()) {
        return fail("no Clang resource directory: pass --resource-dir, or put clang on PATH");
    }

    const std::filesystem::path sidecar = sidecar_path(header);

    BuildScanInput input;
    input.scan.build_path = options.build_path;
    input.scan.translation_unit = options.translation_unit;
    input.scan.headers = { header };
    input.scan.resource_dir = resource_dir;
    input.scan.extra_args = options.extra_args;
    input.compile_args = options.compile_args;
    if (!std::filesystem::exists(sidecar)) {
        input.scan.stubs.push_back(sidecar.filename().string());
    }

    RequestCollector collector;
    std::vector<std::filesystem::path> dependencies;
    std::string error;
    if (!scan_for_build(input, [&collector](clang::ast_matchers::MatchFinder& finder) { collector.attach(finder); },
            dependencies, error)) {
        return fail(error);
    }

    bool clean = true;
    for (const auto& message : collector.errors()) {
        if (message.starts_with(header.string())) {
            std::cerr << message << '\n';
            clean = false;
        }
    }
    if (!clean) {
        return 1;
    }

    const auto found = collector.requests().find(header);
    if (found == collector.requests().end()) {
        return fail(header.string() + ": contains MF_DESCRIBE text but produced no request");
    }

    if (options.insert_includes) {
        bool inserted = false;
        if (!insert_include(found->second, inserted, error)) {
            return fail(error);
        }
        if (inserted) {
            std::cout << "Inserted #include \"" << sidecar.filename().string() << "\" into " << header.string() << '\n';
        }
    }
    if (const auto problem = check_include(found->second)) {
        return fail(*problem);
    }

    if (!write_generated(sidecar, render_sidecar(found->second), error)) {
        return fail(error);
    }

    if (!options.depfile.empty()) {
        const std::string target = options.depfile_target.empty() ? sidecar.generic_string() : options.depfile_target;
        if (!write_depfile(options.depfile, target, without_sidecars(std::move(dependencies)), error)) {
            return fail(error);
        }
    }

    std::cout << "Described " << found->second.records.size() << " type(s) in " << header.filename().string() << '\n';
    return 0;
}

}
