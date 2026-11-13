#include "Sidecar.hpp"

namespace MayaFlux::Reflect::Tool {

namespace {

    constexpr const char* stub_text = "#pragma once\n";

    std::vector<unsigned> include_lines(const std::string& text, const std::string& quoted)
    {
        std::vector<unsigned> includes;
        std::istringstream lines(text);
        std::string line;
        unsigned number = 0;
        while (std::getline(lines, line)) {
            ++number;
            const auto first = line.find_first_not_of(" \t");
            if (first == std::string::npos || line[first] != '#') {
                continue;
            }
            if (line.find("include", first) != std::string::npos && line.find(quoted) != std::string::npos) {
                includes.push_back(number);
            }
        }
        return includes;
    }

}

std::optional<std::string> read_text(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        return std::nullopt;
    }
    std::ostringstream content;
    content << stream.rdbuf();
    return content.str();
}

bool insert_include(const HeaderRequests& requests, bool& inserted, std::string& error)
{
    inserted = false;
    const std::string file = sidecar_path(requests.header).filename().string();
    auto text = read_text(requests.header);
    if (!text) {
        error = "cannot read " + requests.header.string();
        return false;
    }
    if (!include_lines(*text, file + "\"").empty()) {
        return true;
    }

    unsigned last_marker = 0;
    for (const RecordRequest& record : requests.records) {
        last_marker = std::max(last_marker, record.line);
    }

    const std::string newline = text->find("\r\n") != std::string::npos ? "\r\n" : "\n";
    const std::string line = "#include \"" + file + "\"" + newline;

    std::size_t offset = 0;
    for (unsigned i = 0; i < last_marker && offset != std::string::npos; ++i) {
        const auto next = text->find('\n', offset);
        offset = next == std::string::npos ? std::string::npos : next + 1;
    }
    if (offset == std::string::npos) {
        *text += newline + line;
    } else {
        text->insert(offset, line);
    }

    if (!write_if_changed(requests.header, *text, error)) {
        return false;
    }
    inserted = true;
    return true;
}

std::filesystem::path sidecar_path(const std::filesystem::path& header)
{
    return header.parent_path() / (header.stem().string() + ".describe.hpp");
}

std::string render_sidecar(const HeaderRequests& requests)
{
    std::vector<const RecordRequest*> records;
    records.reserve(requests.records.size());
    for (const auto& record : requests.records) {
        records.push_back(&record);
    }
    std::ranges::sort(records, {}, &RecordRequest::name);

    std::string out;
    out += "#pragma once\n\n";
    out += "#include \"MayaFlux/Transitive/Reflect/Describe.hpp\"\n\n";
    out += "#ifndef MAYAFLUX_REFLECT_SCAN\n\n";
    out += "namespace MayaFlux::Reflect::Generated {\n\n";
    for (const RecordRequest* record : records) {
        out += "template <>\n";
        out += "struct Description<::" + record->name + "> {\n";
        out += "    static constexpr std::string_view name = \"" + record->name + "\";\n\n";
        out += "    static constexpr auto members()\n    {\n        return std::tuple {\n";
        for (const std::string& member : record->members) {
            out += "            Member<&::";
            out += record->name;
            out += "::";
            out += member;
            out += "> { \"";
            out += member;
            out += "\" },\n";
        }
        out += "        };\n    }\n};\n\n";
    }
    out += "}\n\n#endif\n";
    return out;
}

bool ensure_stub(const std::filesystem::path& path, std::string& error)
{
    if (std::filesystem::exists(path)) {
        return true;
    }
    return write_if_changed(path, stub_text, error);
}

bool write_if_changed(const std::filesystem::path& path, const std::string& content, std::string& error)
{
    if (read_text(path) == std::optional<std::string>(content)) {
        return true;
    }
    const std::filesystem::path staged = path.string() + ".tmp";
    {
        std::ofstream stream(staged, std::ios::binary | std::ios::trunc);
        stream << content;
        if (!stream) {
            error = "cannot write " + staged.string();
            return false;
        }
    }
    std::error_code code;
    std::filesystem::rename(staged, path, code);
    if (code) {
        std::filesystem::remove(staged, code);
        error = "cannot replace " + path.string();
        return false;
    }
    return true;
}

std::optional<std::string> check_include(const HeaderRequests& requests)
{
    const std::string file = sidecar_path(requests.header).filename().string();
    const std::string quoted = file + "\"";

    unsigned last_marker = 0;
    for (const RecordRequest& record : requests.records) {
        last_marker = std::max(last_marker, record.line);
    }

    const auto text = read_text(requests.header);
    if (!text) {
        return requests.header.string() + ": cannot read the header to check its sidecar include";
    }

    const std::vector<unsigned> includes = include_lines(*text, quoted);

    const std::string where = requests.header.string() + ": ";
    if (includes.empty()) {
        return where + "add #include \"" + file + "\" after line " + std::to_string(last_marker);
    }
    if (includes.size() > 1) {
        return where + "#include \"" + file + "\" appears more than once";
    }
    if (includes.front() <= last_marker) {
        return where + "#include \"" + file + "\" at line " + std::to_string(includes.front())
            + " must come after line " + std::to_string(last_marker)
            + ", the end of the namespace holding the last MF_DESCRIBE";
    }
    return std::nullopt;
}

}
