#include "SignatureEntries.hpp"

#include "Sidecar.hpp"

namespace MayaFlux::Reflect::Tool {

namespace {

    std::multimap<std::string, std::filesystem::path> headers_by_name(const std::filesystem::path& directory)
    {
        std::multimap<std::string, std::filesystem::path> found;
        std::error_code code;
        for (std::filesystem::recursive_directory_iterator it(directory, code), end; it != end; it.increment(code)) {
            if (code) {
                break;
            }
            if (it->is_regular_file(code) && it->path().extension() == ".hpp") {
                found.emplace(it->path().filename().string(), it->path());
            }
        }
        return found;
    }

    bool declares_class(const std::filesystem::path& header, const std::string& class_name)
    {
        const auto text = read_text(header);
        if (!text) {
            return false;
        }
        const std::regex declaration(
            R"((?:^|\n)\s*(?:class|struct)\s+(?:\w+\s+)*)" + class_name + R"(\s*(?:final\s*)?(?::|\{))");
        return std::regex_search(*text, declaration);
    }

}

std::optional<std::vector<SignatureEntry>> read_signature_entries(
    const std::filesystem::path& registry, const std::filesystem::path& source_root, std::string& error)
{
    std::ifstream stream(registry);
    if (!stream) {
        error = "cannot read " + registry.string();
        return std::nullopt;
    }

    const std::regex entry(R"(^\s*([NWB])\((\w+),\s*(MayaFlux::[\w:]+)\)\s*\\?\s*$)");
    std::vector<SignatureEntry> entries;
    std::set<std::string> names;
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        std::smatch match;
        if (!std::regex_match(line, match, entry)) {
            continue;
        }
        if (!names.insert(match[2].str()).second) {
            error = "registry is empty or contains duplicate factory names: " + match[2].str();
            return std::nullopt;
        }
        entries.push_back(SignatureEntry { .name = match[2].str(), .type_name = match[3].str(), .header = {} });
    }
    if (entries.empty()) {
        error = "registry is empty or contains duplicate factory names";
        return std::nullopt;
    }

    const auto headers = headers_by_name(source_root / "MayaFlux");
    for (SignatureEntry& item : entries) {
        const std::string class_name = item.type_name.substr(item.type_name.rfind("::") + 2);
        std::vector<std::filesystem::path> matches;
        const auto range = headers.equal_range(class_name + ".hpp");
        for (auto it = range.first; it != range.second; ++it) {
            if (declares_class(it->second, class_name)) {
                matches.push_back(it->second);
            }
        }
        if (matches.size() != 1) {
            error = item.name + ": expected one defining class header, found " + std::to_string(matches.size());
            return std::nullopt;
        }
        item.header = matches.front();
    }
    return entries;
}

}
