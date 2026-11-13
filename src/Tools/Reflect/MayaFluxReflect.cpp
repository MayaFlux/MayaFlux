#include "Check.hpp"
#include "Discovery.hpp"
#include "Scan.hpp"
#include "Sidecar.hpp"

#ifndef MAYAFLUX_REFLECT_RESOURCE_DIR
#define MAYAFLUX_REFLECT_RESOURCE_DIR ""
#endif

namespace {

using namespace MayaFlux::Reflect::Tool;

struct Config {
    std::vector<std::filesystem::path> roots;
    std::vector<std::filesystem::path> headers;
    std::filesystem::path build_path;
    std::filesystem::path translation_unit;
    std::string resource_dir;
    std::vector<std::string> extra_args;
    bool insert_includes { true };
    bool check { false };
};

int fail(const std::string& message)
{
    std::cerr << message << '\n';
    return 1;
}

int generate(const Config& config)
{
    std::vector<std::filesystem::path> headers = discover_headers(config.roots);
    std::error_code code;
    for (const auto& header : config.headers) {
        const auto canonical = std::filesystem::canonical(header, code);
        if (code) {
            return fail("cannot read header " + header.string());
        }
        headers.push_back(canonical);
    }
    std::ranges::sort(headers);
    headers.erase(std::ranges::unique(headers).begin(), headers.end());
    if (headers.empty()) {
        return fail("no MF_DESCRIBE requests found; pass --root or header paths");
    }

    std::string error;
    ScanInput input;
    for (const auto& header : headers) {
        const std::filesystem::path sidecar = sidecar_path(header);
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
    input.roots = config.roots;
    input.headers = headers;
    input.resource_dir = config.resource_dir;
    input.extra_args = config.extra_args;

    RequestCollector collector;
    if (!scan(input, collector, error)) {
        return fail(error);
    }
    if (!collector.errors().empty()) {
        for (const auto& message : collector.errors()) {
            std::cerr << message << '\n';
        }
        return 1;
    }
    if (collector.requests().empty()) {
        return fail("no MF_DESCRIBE requests were recognised in the selected headers (marker aliases matched: "
            + std::to_string(collector.matched()) + ", skipped: " + std::to_string(collector.skipped()) + ")");
    }

    if (config.check) {
        const auto existing = config.roots.empty() ? std::vector<std::filesystem::path> {} : discover_sidecars(config.roots);
        const auto problems = check_sidecars(collector.requests(), headers, existing);
        for (const auto& problem : problems) {
            std::cerr << problem << '\n';
        }
        if (problems.empty()) {
            std::cout << "All " << collector.requests().size() << " sidecars are current\n";
            return 0;
        }
        std::cout << problems.size() << " problem(s) found\n";
        return 1;
    }

    bool clean = true;
    std::size_t types = 0;
    for (const auto& header : headers) {
        const auto found = collector.requests().find(header);
        if (found == collector.requests().end()) {
            std::cerr << header.string() << ": contains MF_DESCRIBE text but produced no request\n";
            clean = false;
            continue;
        }
        if (!write_if_changed(sidecar_path(header), render_sidecar(found->second), error)) {
            return fail(error);
        }
        types += found->second.records.size();
        if (config.insert_includes) {
            bool inserted = false;
            if (!insert_include(found->second, inserted, error)) {
                return fail(error);
            }
            if (inserted) {
                std::cout << "Inserted #include \"" << sidecar_path(header).filename().string() << "\" into " << header.string() << '\n';
            }
        }
        if (const auto problem = check_include(found->second)) {
            std::cerr << *problem << '\n';
            clean = false;
        }
    }

    std::cout << "Generated " << types << " type descriptions in " << collector.requests().size() << " sidecars\n";
    return clean ? 0 : 1;
}

}

int main(int argc, const char** argv)
{
    llvm::cl::OptionCategory category("mayaflux-reflect options");
    llvm::cl::list<std::string> roots("root",
        llvm::cl::desc("Directory searched recursively for MF_DESCRIBE, or a single header "
                       "(default without headers: src/MayaFlux when it exists, else src)"),
        llvm::cl::value_desc("path"), llvm::cl::cat(category));
    llvm::cl::list<std::string> headers(llvm::cl::Positional,
        llvm::cl::desc("<header>..."), llvm::cl::cat(category));
    llvm::cl::opt<std::string> build_path("p",
        llvm::cl::desc("Directory holding compile_commands.json"),
        llvm::cl::value_desc("dir"), llvm::cl::init("build"), llvm::cl::cat(category));
    llvm::cl::opt<std::string> translation_unit("translation-unit",
        llvm::cl::desc("Source file whose compile command is borrowed for the scan"),
        llvm::cl::value_desc("file"), llvm::cl::cat(category));
    llvm::cl::opt<std::string> resource_dir("resource-dir",
        llvm::cl::desc("Clang resource directory holding the builtin headers"),
        llvm::cl::value_desc("dir"), llvm::cl::init(MAYAFLUX_REFLECT_RESOURCE_DIR),
        llvm::cl::cat(category));
    llvm::cl::list<std::string> extra_args("extra-arg",
        llvm::cl::desc("Extra Clang argument added to the scan"),
        llvm::cl::value_desc("argument"), llvm::cl::cat(category));

    llvm::cl::opt<bool> no_insert_include("no-insert-include",
        llvm::cl::desc("Report a missing sidecar include instead of inserting it"),
        llvm::cl::cat(category));

    llvm::cl::opt<bool> check("check",
        llvm::cl::desc("Write nothing; exit 1 when a sidecar, its include or a marker is out of date"),
        llvm::cl::cat(category));

    llvm::cl::HideUnrelatedOptions(category);
    llvm::cl::ParseCommandLineOptions(argc, argv,
        "Writes a Name.describe.hpp sidecar beside each header that holds MF_DESCRIBE requests.\n");

    Config config;
    config.roots.assign(roots.begin(), roots.end());
    config.headers.assign(headers.begin(), headers.end());
    config.build_path = build_path.getValue();
    config.translation_unit = translation_unit.getValue();
    config.resource_dir = resource_dir.getValue();
    config.extra_args.assign(extra_args.begin(), extra_args.end());
    config.insert_includes = !no_insert_include;
    config.check = check;

    if (config.roots.empty() && config.headers.empty()) {
        config.roots.emplace_back(std::filesystem::is_directory("src/MayaFlux") ? "src/MayaFlux" : "src");
    }

    for (auto& root : config.roots) {
        std::error_code code;
        const auto canonical = std::filesystem::canonical(root, code);
        if (code) {
            return fail("cannot read root " + root.string());
        }
        root = canonical;
    }
    return generate(config);
}
