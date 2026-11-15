#include "Discovery.hpp"
#include "FunctionCollector.hpp"
#include "FunctionSidecar.hpp"
#include "Scan.hpp"
#include "Sidecar.hpp"
#include "SignatureOutput.hpp"

#ifndef MAYAFLUX_REFLECT_RESOURCE_DIR
#define MAYAFLUX_REFLECT_RESOURCE_DIR ""
#endif

namespace {

using namespace MayaFlux::Reflect::Tool;

struct Config {
    std::filesystem::path root;
    std::filesystem::path registry;
    std::filesystem::path output;
    std::filesystem::path build_path;
    std::filesystem::path translation_unit;
    std::string resource_dir;
    std::vector<std::string> extra_args;
    bool check { false };
    bool functions { false };
    bool insert_includes { true };
    std::vector<std::filesystem::path> paths;
};

int fail(const std::string& message)
{
    std::cerr << message << '\n';
    return 1;
}

int generate_functions(const Config& config)
{
    std::vector<std::filesystem::path> roots = config.paths;
    if (roots.empty()) {
        roots.push_back(config.root / "src/MayaFlux");
    }
    std::error_code code;
    for (auto& root : roots) {
        const auto canonical = std::filesystem::canonical(root, code);
        if (code) {
            return fail("cannot read " + root.string());
        }
        root = canonical;
    }

    const std::vector<std::filesystem::path> headers = discover_headers(roots, "MF_DESCRIBE_FUNCTION");
    if (headers.empty()) {
        return fail("no MF_DESCRIBE_FUNCTION requests found under the given paths");
    }

    std::string error;
    ScanInput input;
    for (const auto& header : headers) {
        const std::filesystem::path sidecar = sidecar_path(header, function_sidecar_suffix);
        if (config.check) {
            if (!std::filesystem::exists(sidecar)) {
                input.stubs.push_back(sidecar.filename().string());
            }
        } else if (!ensure_stub(sidecar, error)) {
            return fail(error);
        }
    }
    input.build_path = config.build_path;
    input.translation_unit = config.translation_unit;
    input.roots = roots;
    input.headers = headers;
    input.resource_dir = config.resource_dir;
    input.extra_args = config.extra_args;

    FunctionCollector collector;
    if (!scan(input, [&collector](clang::ast_matchers::MatchFinder& finder) { collector.attach(finder); }, error)) {
        return fail(error);
    }
    if (!collector.errors().empty()) {
        for (const auto& message : collector.errors()) {
            std::cerr << message << '\n';
        }
        return 1;
    }
    if (collector.requests().empty()) {
        return fail("no MF_DESCRIBE_FUNCTION requests were recognised in the selected headers (marker variables matched: "
            + std::to_string(collector.matched()) + ")");
    }

    if (config.check) {
        std::vector<std::filesystem::path> directories;
        for (const auto& root : roots) {
            if (std::filesystem::is_directory(root, code)) {
                directories.push_back(root);
            }
        }
        const auto existing = directories.empty()
            ? std::vector<std::filesystem::path> {}
            : discover_sidecars(directories, function_sidecar_suffix);
        const auto problems = check_function_sidecars(collector.requests(), headers, existing);
        for (const auto& problem : problems) {
            std::cerr << problem << '\n';
        }
        if (problems.empty()) {
            std::cout << "All " << collector.requests().size() << " function sidecars are current\n";
            return 0;
        }
        std::cout << problems.size() << " problem(s) found\n";
        return 1;
    }

    bool clean = true;
    std::size_t functions = 0;
    for (const auto& header : headers) {
        const auto found = collector.requests().find(header);
        if (found == collector.requests().end()) {
            std::cerr << header.string() << ": contains MF_DESCRIBE_FUNCTION text but produced no request\n";
            clean = false;
            continue;
        }
        if (!write_if_changed(sidecar_path(header, function_sidecar_suffix), render_function_sidecar(found->second), error)) {
            return fail(error);
        }
        functions += found->second.functions.size();
        const unsigned last = last_function_line(found->second);
        if (config.insert_includes) {
            bool inserted = false;
            if (!insert_include(header, last, function_sidecar_suffix, inserted, error)) {
                return fail(error);
            }
            if (inserted) {
                std::cout << "Inserted #include \"" << sidecar_path(header, function_sidecar_suffix).filename().string()
                          << "\" into " << header.string() << '\n';
            }
        }
        if (const auto problem = check_include(header, last, function_sidecar_suffix)) {
            std::cerr << *problem << '\n';
            clean = false;
        }
    }
    std::cout << "Generated " << functions << " function descriptions in " << collector.requests().size() << " sidecars\n";
    return clean ? 0 : 1;
}

int generate(const Config& config)
{
    std::error_code code;
    const std::filesystem::path source_root = std::filesystem::canonical(config.root / "src", code);
    if (code) {
        return fail("cannot read " + (config.root / "src").string());
    }

    std::string error;
    auto entries = read_signature_entries(config.registry, source_root, error);
    if (!entries) {
        return fail(error);
    }

    ScanInput input;
    input.build_path = config.build_path;
    input.translation_unit = config.translation_unit;
    input.roots = { source_root };
    input.resource_dir = config.resource_dir;
    input.extra_args = config.extra_args;
    for (const SignatureEntry& entry : *entries) {
        const auto canonical = std::filesystem::canonical(entry.header, code);
        if (code) {
            return fail("cannot read header " + entry.header.string());
        }
        input.headers.push_back(canonical);
    }
    std::ranges::sort(input.headers);
    input.headers.erase(std::ranges::unique(input.headers).begin(), input.headers.end());

    SignatureCollector collector(*entries, source_root);
    if (!scan(input, [&collector](clang::ast_matchers::MatchFinder& finder) { collector.attach(finder); }, error)) {
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

    if (config.check) {
        std::size_t stale = 0;
        for (const auto& [name, text] : outputs) {
            if (read_text(config.output / name) != std::optional<std::string>(*text)) {
                std::cerr << (config.output / name).string() << ": differs from what the registry generates\n";
                ++stale;
            }
        }
        if (stale == 0) {
            std::cout << "All " << outputs.size() << " generated files are current\n";
            return 0;
        }
        std::cout << stale << " file(s) out of date\n";
        return 1;
    }

    std::filesystem::create_directories(config.output, code);
    if (code) {
        return fail("cannot create " + config.output.string());
    }
    for (const auto& [name, text] : outputs) {
        if (!write_if_changed(config.output / name, *text, error)) {
            return fail(error);
        }
    }
    std::cout << "Generated methods for " << collector.classes().size() << " classes\n";
    return 0;
}

}

int main(int argc, const char** argv)
{
    llvm::cl::OptionCategory category("mayaflux-signatures options");
    llvm::cl::opt<std::string> root("root",
        llvm::cl::desc("Repository root holding src/"),
        llvm::cl::value_desc("dir"), llvm::cl::init("."), llvm::cl::cat(category));
    llvm::cl::opt<std::string> registry("registry",
        llvm::cl::desc("Registry header (default: <root>/src/MayaFlux/API/Proxy/Registry.hpp)"),
        llvm::cl::value_desc("file"), llvm::cl::cat(category));
    llvm::cl::opt<std::string> output("output",
        llvm::cl::desc("Directory the generated files are written to (default: <root>/src/MayaFlux/API/Proxy/Generated)"),
        llvm::cl::value_desc("dir"), llvm::cl::cat(category));
    llvm::cl::opt<std::string> build_path("p",
        llvm::cl::desc("Directory holding compile_commands.json"),
        llvm::cl::value_desc("dir"), llvm::cl::init("build"), llvm::cl::cat(category));
    llvm::cl::opt<std::string> translation_unit("translation-unit",
        llvm::cl::desc("Source file whose compile command is borrowed (default: <root>/src/MayaFlux/API/Proxy/Creator.cpp)"),
        llvm::cl::value_desc("file"), llvm::cl::cat(category));
    llvm::cl::opt<std::string> resource_dir("resource-dir",
        llvm::cl::desc("Clang resource directory holding the builtin headers"),
        llvm::cl::value_desc("dir"), llvm::cl::init(MAYAFLUX_REFLECT_RESOURCE_DIR),
        llvm::cl::cat(category));
    llvm::cl::list<std::string> extra_args("extra-arg",
        llvm::cl::desc("Extra Clang argument added to the scan"),
        llvm::cl::value_desc("argument"), llvm::cl::cat(category));
    llvm::cl::opt<bool> check("check",
        llvm::cl::desc("Write nothing; exit 1 when a generated file is out of date"),
        llvm::cl::cat(category));
    llvm::cl::opt<bool> functions("functions",
        llvm::cl::desc("Write a Name.signatures.hpp sidecar beside each header that holds MF_DESCRIBE_FUNCTION "
                       "requests, instead of the Creator methods"),
        llvm::cl::cat(category));
    llvm::cl::opt<bool> no_insert_include("no-insert-include",
        llvm::cl::desc("With --functions, report a missing sidecar include instead of inserting it"),
        llvm::cl::cat(category));
    llvm::cl::list<std::string> paths(llvm::cl::Positional,
        llvm::cl::desc("<path>... (with --functions: headers or directories searched, default src/MayaFlux)"),
        llvm::cl::cat(category));

    llvm::cl::HideUnrelatedOptions(category);
    llvm::cl::ParseCommandLineOptions(argc, argv,
        "Writes the Creator proxy's methods from the public constructors of the registered classes, or with "
        "--functions the parameter names of the functions named by MF_DESCRIBE_FUNCTION.\n");

    Config config;
    config.root = root.getValue();
    const std::filesystem::path proxy = config.root / "src/MayaFlux/API/Proxy";
    config.registry = registry.empty() ? proxy / "Registry.hpp" : std::filesystem::path(registry.getValue());
    config.output = output.empty() ? proxy / "Generated" : std::filesystem::path(output.getValue());
    config.build_path = build_path.getValue();
    config.translation_unit = translation_unit.empty() ? proxy / "Creator.cpp" : std::filesystem::path(translation_unit.getValue());
    config.resource_dir = resource_dir.getValue();
    config.extra_args.assign(extra_args.begin(), extra_args.end());
    config.check = check;
    config.functions = functions;
    config.insert_includes = !no_insert_include;
    config.paths.assign(paths.begin(), paths.end());
    return config.functions ? generate_functions(config) : generate(config);
}
