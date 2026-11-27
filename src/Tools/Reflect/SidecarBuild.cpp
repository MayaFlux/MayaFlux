#include "SidecarBuild.hpp"

#include "Build.hpp"
#include "Discovery.hpp"
#include "Fingerprint.hpp"
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

int run_sidecar_build(const SidecarBuildOptions& options, const SidecarKind& kind)
{
    std::error_code code;
    std::vector<std::filesystem::path> headers;
    for (const auto& path : options.paths) {
        const auto canonical = std::filesystem::canonical(path, code);
        if (code) {
            return fail("cannot read " + path.string());
        }
        if (std::filesystem::is_directory(canonical, code)) {
            const auto found = discover_headers({ canonical }, kind.marker);
            headers.insert(headers.end(), found.begin(), found.end());
        } else {
            headers.push_back(canonical);
        }
    }
    std::ranges::sort(headers);
    headers.erase(std::ranges::unique(headers).begin(), headers.end());
    if (headers.empty()) {
        return fail("no " + std::string(kind.marker) + " requests found under the given paths");
    }
    if (options.compile_args.empty() && options.translation_unit.empty()) {
        return fail("--build needs --translation-unit with a compile database, or --compile-arg");
    }

    BuildScanInput input;
    input.scan.build_path = options.build_path;
    input.scan.translation_unit = options.translation_unit;
    input.scan.headers = headers;
    input.scan.resource_dir = resolve_resource_dir(options.resource_dir);
    input.scan.extra_args = options.extra_args;
    input.scan.prefix_header = options.prefix_header;
    input.scan.skip_function_bodies = true;
    input.compile_args = options.compile_args;
    if (input.scan.resource_dir.empty()) {
        return fail("no Clang resource directory: pass --resource-dir, or put clang on PATH");
    }

    std::string error;
    Fingerprint fingerprint;
    fingerprint.executable = options.executable;
    if (!scan_arguments(input, fingerprint.settings, error)) {
        return fail(error);
    }
    fingerprint.settings.push_back(input.scan.resource_dir);
    fingerprint.settings.push_back(options.prefix_header.generic_string());
    fingerprint.settings.insert(fingerprint.settings.end(), options.extra_args.begin(), options.extra_args.end());
    for (const auto& header : headers) {
        const std::filesystem::path sidecar = sidecar_path(header, kind.suffix);
        fingerprint.settings.push_back(header.generic_string());
        fingerprint.outputs.push_back(sidecar);
        if (!std::filesystem::exists(sidecar)) {
            input.scan.stubs.push_back(sidecar.filename().string());
        }
    }
    std::ranges::sort(input.scan.stubs);
    input.scan.stubs.erase(std::ranges::unique(input.scan.stubs).begin(), input.scan.stubs.end());

    const std::filesystem::path state = options.depfile.empty()
        ? std::filesystem::path {}
        : std::filesystem::path(options.depfile.string() + ".fingerprint");

    const auto emit_depfile = [&options, &error](const std::vector<std::filesystem::path>& dependencies) {
        return options.depfile.empty()
            || write_depfile(options.depfile, options.depfile_target, dependencies, error);
    };

    if (!state.empty()) {
        if (const auto dependencies = unchanged_since(state, fingerprint)) {
            if (!emit_depfile(*dependencies)) {
                return fail(error);
            }
            std::cout << "Code unchanged for " << headers.size() << " " << kind.noun << " header(s)\n";
            return 0;
        }
    }

    std::vector<std::filesystem::path> dependencies;
    if (!scan_for_build(input, kind.attach, dependencies, error)) {
        return fail(error);
    }
    if (!kind.errors().empty()) {
        for (const auto& message : kind.errors()) {
            std::cerr << message << '\n';
        }
        return 1;
    }

    bool clean = true;
    for (const auto& header : headers) {
        const auto rendered = kind.rendered(header);
        if (!rendered) {
            std::cerr << header.string() << ": contains " << kind.marker << " text but produced no request\n";
            clean = false;
            continue;
        }
        const auto& [text, last_marker] = *rendered;
        const std::filesystem::path sidecar = sidecar_path(header, kind.suffix);
        if (options.insert_includes) {
            bool inserted = false;
            if (!insert_include(header, last_marker, kind.suffix, inserted, error)) {
                return fail(error);
            }
            if (inserted) {
                std::cout << "Inserted #include \"" << sidecar.filename().string() << "\" into " << header.string() << '\n';
            }
        }
        if (const auto problem = check_include(header, last_marker, kind.suffix)) {
            std::cerr << *problem << '\n';
            clean = false;
            continue;
        }
        if (!write_generated(sidecar, text, error)) {
            return fail(error);
        }
    }
    if (!clean) {
        return 1;
    }

    fingerprint.dependencies = without_sidecars(std::move(dependencies));
    if (!emit_depfile(fingerprint.dependencies)) {
        return fail(error);
    }
    if (!state.empty() && !record_fingerprint(state, fingerprint, error)) {
        return fail(error);
    }

    std::cout << "Described " << headers.size() << " " << kind.noun << " header(s) in one scan\n";
    return 0;
}

}
