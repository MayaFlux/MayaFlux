#include "Build.hpp"

#include "Sidecar.hpp"

#include <array>
#include <cctype>
#include <cstdio>

#ifdef _WIN32
#define MAYAFLUX_REFLECT_POPEN _popen
#define MAYAFLUX_REFLECT_PCLOSE _pclose
#define MAYAFLUX_REFLECT_QUIET " 2>nul"
#else
#define MAYAFLUX_REFLECT_POPEN popen
#define MAYAFLUX_REFLECT_PCLOSE pclose
#define MAYAFLUX_REFLECT_QUIET " 2>/dev/null"
#endif

namespace MayaFlux::Reflect::Tool {

namespace {

    std::string clang_resource_dir()
    {
        std::string command = "clang -print-resource-dir";
        command += MAYAFLUX_REFLECT_QUIET;
        FILE* pipe = MAYAFLUX_REFLECT_POPEN(command.c_str(), "r");
        if (pipe == nullptr) {
            return {};
        }
        std::string output;
        std::array<char, 256> buffer {};
        while (std::fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
            output += buffer.data();
        }
        MAYAFLUX_REFLECT_PCLOSE(pipe);
        while (!output.empty() && std::isspace(static_cast<unsigned char>(output.back())) != 0) {
            output.pop_back();
        }
        return output;
    }

    std::string escaped(std::string_view text)
    {
        std::string out;
        out.reserve(text.size());
        for (const char c : text) {
            if (c == ' ' || c == '\t' || c == '#') {
                out += '\\';
            } else if (c == '$') {
                out += '$';
            }
            out += c;
        }
        return out;
    }

}

std::string resolve_resource_dir(const std::string& configured)
{
    if (!configured.empty() && llvm::sys::fs::is_directory(configured)) {
        return configured;
    }
    std::string found = clang_resource_dir();
    if (found.empty() || !llvm::sys::fs::is_directory(found)) {
        return {};
    }
    return found;
}

bool write_depfile(const std::filesystem::path& path, const std::string& target,
    const std::vector<std::filesystem::path>& dependencies, std::string& error)
{
    std::string text = escaped(std::filesystem::path(target).generic_string()) + ":";
    for (const auto& dependency : dependencies) {
        text += " \\\n  " + escaped(dependency.generic_string());
    }
    text += '\n';

    std::error_code code;
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(), code);
        if (code) {
            error = "cannot create " + path.parent_path().string();
            return false;
        }
    }
    return write_if_changed(path, text, error);
}

bool write_generated(const std::filesystem::path& path, const std::string& content, std::string& error)
{
    return write_if_changed(path, content, error);
}

std::vector<std::filesystem::path> without_sidecars(std::vector<std::filesystem::path> dependencies)
{
    std::erase_if(dependencies, [](const std::filesystem::path& path) {
        const std::string name = path.filename().string();
        return name.ends_with(".describe.hpp") || name.ends_with(".signatures.hpp");
    });
    return dependencies;
}

}
