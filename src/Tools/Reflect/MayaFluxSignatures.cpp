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
};

int fail(const std::string& message)
{
    std::cerr << message << '\n';
    return 1;
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

    llvm::cl::HideUnrelatedOptions(category);
    llvm::cl::ParseCommandLineOptions(argc, argv,
        "Writes the Creator proxy's methods from the public constructors of the registered classes.\n");

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
    return generate(config);
}
