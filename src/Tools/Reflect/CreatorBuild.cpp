#include "CreatorBuild.hpp"

#include "Build.hpp"
#include "Scan.hpp"
#include "SignatureCollector.hpp"
#include "SignatureEntries.hpp"
#include "SignatureOutput.hpp"

namespace MayaFlux::Reflect::Tool {

namespace {

    int fail(const std::string& message)
    {
        std::cerr << message << '\n';
        return 1;
    }

}

int run_creator_build(const CreatorBuildOptions& options)
{
    std::error_code code;
    const std::filesystem::path source_root = std::filesystem::canonical(options.root / "src", code);
    if (code) {
        return fail("cannot read " + (options.root / "src").string());
    }
    if (options.compile_args.empty() && options.translation_unit.empty()) {
        return fail("--build needs --translation-unit with a compile database, or --compile-arg");
    }

    const std::string resource_dir = resolve_resource_dir(options.resource_dir);
    if (resource_dir.empty()) {
        return fail("no Clang resource directory: pass --resource-dir, or put clang on PATH");
    }

    std::string error;
    auto entries = read_signature_entries(options.registry, source_root, error);
    if (!entries) {
        return fail(error);
    }

    BuildScanInput input;
    input.scan.build_path = options.build_path;
    input.scan.translation_unit = options.translation_unit;
    input.scan.roots = { source_root };
    input.scan.resource_dir = resource_dir;
    input.scan.extra_args = options.extra_args;
    input.scan.prefix_header = options.prefix_header;
    input.compile_args = options.compile_args;
    for (const SignatureEntry& entry : *entries) {
        const auto canonical = std::filesystem::canonical(entry.header, code);
        if (code) {
            return fail("cannot read header " + entry.header.string());
        }
        input.scan.headers.push_back(canonical);
    }
    std::ranges::sort(input.scan.headers);
    input.scan.headers.erase(std::ranges::unique(input.scan.headers).begin(), input.scan.headers.end());

    SignatureCollector collector(*entries, source_root);
    std::vector<std::filesystem::path> dependencies;
    if (!scan_for_build(input, [&collector](clang::ast_matchers::MatchFinder& finder) { collector.attach(finder); },
            dependencies, error)) {
        return fail(error);
    }
    if (!collector.errors().empty()) {
        for (const auto& message : collector.errors()) {
            std::cerr << message << '\n';
        }
        return 1;
    }
    if (const auto missing = collector.missing(); !missing.empty()) {
        for (const auto& name : missing) {
            std::cerr << name << ": expected one complete class definition, found none\n";
        }
        return 1;
    }

    const SignatureFiles files = render_signature_files(collector.classes(), source_root);
    const std::vector<std::pair<std::string, const std::string*>> outputs {
        { "CreatorIncludes.hpp", &files.includes },
        { "CreatorClasses.hpp", &files.classes },
        { "CreatorDeclarations.inc", &files.declarations },
        { "CreatorDefinitions.inc", &files.definitions },
    };

    std::filesystem::create_directories(options.output, code);
    if (code) {
        return fail("cannot create " + options.output.string());
    }
    for (const auto& [name, text] : outputs) {
        if (!write_generated(options.output / name, *text, error)) {
            return fail(error);
        }
    }

    if (!options.depfile.empty()) {
        const std::filesystem::path output_root = std::filesystem::weakly_canonical(options.output, code);
        std::erase_if(dependencies, [&output_root](const std::filesystem::path& path) {
            const auto relative = path.lexically_relative(output_root);
            return !relative.empty() && *relative.begin() != "..";
        });
        const auto registry = std::filesystem::weakly_canonical(options.registry, code);
        dependencies.push_back(registry);
        std::ranges::sort(dependencies);
        dependencies.erase(std::ranges::unique(dependencies).begin(), dependencies.end());

        const std::string target = options.depfile_target.empty()
            ? (options.output / outputs.front().first).generic_string()
            : options.depfile_target;
        if (!write_depfile(options.depfile, target, without_sidecars(std::move(dependencies)), error)) {
            return fail(error);
        }
    }

    std::cout << "Generated methods for " << collector.classes().size() << " classes\n";
    return 0;
}

}
