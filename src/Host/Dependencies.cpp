#include "Dependencies.hpp"

#include "MayaFlux/Transitive/Platform/Acquisition.hpp"

namespace MayaFlux::Host {

namespace {

    Result<std::filesystem::path> load_acquired(const Platform::Acquired<Platform::LibraryLocation>& located)
    {
        if (!located) {
            return std::unexpected(detail::to_failure(located.error()));
        }

        std::error_code ec;
        std::filesystem::path canonical = std::filesystem::canonical(located->path, ec);
        if (ec) {
            canonical = located->path;
        }

        auto loaded = detail::apply_or_stage(detail::Stage::Library, canonical.string());
        if (!loaded) {
            return std::unexpected(std::move(loaded.error()));
        }
        return canonical;
    }

}

Result<std::filesystem::path> add_include_root(
    const std::filesystem::path& directory,
    std::string_view marker_file)
{
    auto root = Platform::acquire_header_root(directory, marker_file);
    if (!root) {
        return std::unexpected(detail::to_failure(root.error()));
    }

    auto added = detail::apply_or_stage(detail::Stage::IncludePath, root->string());
    if (!added) {
        return std::unexpected(std::move(added.error()));
    }
    return *root;
}

Result<std::filesystem::path> add_header(
    std::string_view file_name,
    std::span<const std::filesystem::path> roots)
{
    Failure failure { "header not found: " + std::string(file_name), {} };

    for (const auto& root : roots) {
        auto found = Platform::acquire_header_root(root, file_name);
        if (!found) {
            Failure refused = detail::to_failure(found.error());
            failure.tried.insert(failure.tried.end(), refused.tried.begin(), refused.tried.end());
            continue;
        }

        const std::filesystem::path header = *found / file_name;
        if (auto included = eval_file(header); !included) {
            return std::unexpected(std::move(included.error()));
        }
        return header;
    }

    return std::unexpected(std::move(failure));
}

Result<std::filesystem::path> load_library(const std::filesystem::path& path)
{
    return load_acquired(Platform::acquire_library(Platform::LibraryFromPath { path }));
}

Result<std::filesystem::path> load_library_from_prefix(
    const std::filesystem::path& prefix,
    std::string_view base_name)
{
    return load_acquired(Platform::acquire_library(
        Platform::LibraryFromPrefix { prefix, std::string(base_name) }));
}

Result<void> add_compile_flag(const std::string& flag)
{
    if (flag.empty()) {
        return std::unexpected(Failure { "empty compile flag", {} });
    }
    return detail::apply_or_stage(detail::Stage::CompileFlag, flag);
}

} // namespace MayaFlux::Host
