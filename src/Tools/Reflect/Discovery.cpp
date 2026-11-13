#include "Discovery.hpp"

namespace MayaFlux::Reflect::Tool {

namespace {

    constexpr std::string_view marker = "MF_DESCRIBE";

    bool is_header(const std::filesystem::path& path)
    {
        const std::string name = path.filename().string();
        if (name.ends_with(".describe.hpp")) {
            return false;
        }
        const std::string extension = path.extension().string();
        return extension == ".hpp" || extension == ".h" || extension == ".hh" || extension == ".hxx";
    }

    bool has_request(const std::filesystem::path& path)
    {
        std::ifstream stream(path);
        std::string line;
        while (std::getline(stream, line)) {
            std::string_view view(line);
            while (!view.empty() && (view.front() == ' ' || view.front() == '\t' || view.front() == ';' || view.front() == '}')) {
                view.remove_prefix(1);
            }
            if (!view.starts_with(marker)) {
                continue;
            }
            view.remove_prefix(marker.size());
            while (!view.empty() && (view.front() == ' ' || view.front() == '\t')) {
                view.remove_prefix(1);
            }
            if (!view.empty() && view.front() == '(') {
                return true;
            }
        }
        return false;
    }

}

std::vector<std::filesystem::path> discover_headers(const std::vector<std::filesystem::path>& roots)
{
    std::vector<std::filesystem::path> found;
    std::error_code code;
    for (const auto& root : roots) {
        if (std::filesystem::is_regular_file(root, code)) {
            if (is_header(root) && has_request(root)) {
                found.push_back(std::filesystem::canonical(root, code));
            }
            continue;
        }
        for (std::filesystem::recursive_directory_iterator it(root, code), end; it != end; it.increment(code)) {
            if (code) {
                break;
            }
            if (it->is_regular_file(code) && is_header(it->path()) && has_request(it->path())) {
                found.push_back(std::filesystem::canonical(it->path(), code));
            }
        }
    }
    std::ranges::sort(found);
    found.erase(std::ranges::unique(found).begin(), found.end());
    return found;
}

std::vector<std::filesystem::path> discover_sidecars(const std::vector<std::filesystem::path>& roots)
{
    std::vector<std::filesystem::path> found;
    std::error_code code;
    for (const auto& root : roots) {
        if (!std::filesystem::is_directory(root, code)) {
            continue;
        }
        for (std::filesystem::recursive_directory_iterator it(root, code), end; it != end; it.increment(code)) {
            if (code) {
                break;
            }
            if (it->is_regular_file(code) && it->path().filename().string().ends_with(".describe.hpp")) {
                found.push_back(std::filesystem::canonical(it->path(), code));
            }
        }
    }
    std::ranges::sort(found);
    found.erase(std::ranges::unique(found).begin(), found.end());
    return found;
}

}
