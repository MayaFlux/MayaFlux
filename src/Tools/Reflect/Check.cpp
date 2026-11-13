#include "Check.hpp"

#include "Sidecar.hpp"

namespace MayaFlux::Reflect::Tool {

namespace {

    using Members = std::map<std::string, std::vector<std::string>>;

    constexpr std::string_view record_marker = "struct Description<::";

    Members parse_sidecar(const std::string& text)
    {
        Members parsed;
        auto current = parsed.end();
        std::istringstream lines(text);
        std::string line;
        while (std::getline(lines, line)) {
            if (const auto at = line.find(record_marker); at != std::string::npos) {
                const auto begin = at + record_marker.size();
                const auto end = line.find('>', begin);
                if (end != std::string::npos) {
                    current = parsed.try_emplace(line.substr(begin, end - begin)).first;
                }
                continue;
            }
            if (current == parsed.end() || line.find("Member<&::") == std::string::npos) {
                continue;
            }
            const auto open = line.find('"');
            const auto close = open == std::string::npos ? open : line.find('"', open + 1);
            if (close != std::string::npos) {
                current->second.push_back(line.substr(open + 1, close - open - 1));
            }
        }
        return parsed;
    }

    std::string joined(const std::vector<std::string>& names)
    {
        std::string out;
        for (const auto& name : names) {
            out += out.empty() ? name : ", " + name;
        }
        return out;
    }

    std::vector<std::string> missing_from(const std::vector<std::string>& names, const std::vector<std::string>& other)
    {
        std::vector<std::string> out;
        for (const auto& name : names) {
            if (std::ranges::find(other, name) == other.end()) {
                out.push_back(name);
            }
        }
        return out;
    }

    std::vector<std::string> drift(const std::string& where, const std::string& have_text, const std::string& want_text)
    {
        const Members have = parse_sidecar(have_text);
        const Members want = parse_sidecar(want_text);
        std::vector<std::string> messages;

        for (const auto& [name, members] : want) {
            const auto found = have.find(name);
            if (found == have.end()) {
                messages.push_back(where + name + " is not described in the sidecar");
                continue;
            }
            if (found->second == members) {
                continue;
            }
            const auto added = missing_from(members, found->second);
            const auto removed = missing_from(found->second, members);
            std::string change = where + name + ":";
            if (!added.empty()) {
                change += " add " + joined(added);
            }
            if (!removed.empty()) {
                change += " remove " + joined(removed);
            }
            if (added.empty() && removed.empty()) {
                change += " member order changed";
            }
            messages.push_back(change);
        }
        for (const auto& [name, members] : have) {
            if (!want.contains(name)) {
                messages.push_back(where + name + " is in the sidecar but no longer requested");
            }
        }
        if (messages.empty()) {
            messages.push_back(where + "the sidecar text differs from the generator output");
        }
        return messages;
    }

}

std::vector<std::string> check_sidecars(
    const RequestMap& requests,
    const std::vector<std::filesystem::path>& headers,
    const std::vector<std::filesystem::path>& existing)
{
    std::vector<std::string> problems;
    std::vector<std::filesystem::path> expected;

    for (const auto& header : headers) {
        const auto found = requests.find(header);
        if (found == requests.end()) {
            problems.push_back(header.string() + ": contains MF_DESCRIBE text but produced no request");
            continue;
        }

        const std::filesystem::path sidecar = sidecar_path(header);
        expected.push_back(sidecar);
        const std::string where = sidecar.string() + ": ";
        const std::string want = render_sidecar(found->second);

        if (const auto have = read_text(sidecar)) {
            if (*have != want) {
                const auto messages = drift(where, *have, want);
                problems.insert(problems.end(), messages.begin(), messages.end());
            }
        } else {
            problems.push_back(where + "missing, expected " + std::to_string(found->second.records.size()) + " record(s)");
        }

        if (const auto problem = check_include(found->second)) {
            problems.push_back(*problem);
        }
    }

    for (const auto& sidecar : existing) {
        if (std::ranges::find(expected, sidecar) == expected.end()) {
            problems.push_back(sidecar.string() + ": orphaned, its header has no MF_DESCRIBE left");
        }
    }
    return problems;
}

}
