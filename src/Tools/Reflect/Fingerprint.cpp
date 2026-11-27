#include "Fingerprint.hpp"

#include "Sidecar.hpp"

#include <cctype>
#include <cstdint>

namespace MayaFlux::Reflect::Tool {

namespace {

    constexpr std::string_view state_header = "mayaflux-reflect-fingerprint 1";

    class Hash {
    public:
        void add(std::string_view text)
        {
            for (const char c : text) {
                m_value ^= static_cast<unsigned char>(c);
                m_value *= 1099511628211ULL;
            }
            m_value ^= 0xFFU;
            m_value *= 1099511628211ULL;
        }

        [[nodiscard]] std::string text() const { return std::to_string(m_value); }

    private:
        std::uint64_t m_value { 14695981039346656037ULL };
    };

    bool is_word(char c)
    {
        return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
    }

    std::size_t literal_end(std::string_view source, std::size_t begin)
    {
        const char quote = source[begin];
        if (quote == '"' && begin > 0 && source[begin - 1] == 'R') {
            const std::size_t open = source.find('(', begin + 1);
            if (open != std::string_view::npos) {
                const std::string close = ")" + std::string(source.substr(begin + 1, open - begin - 1)) + "\"";
                const std::size_t end = source.find(close, open + 1);
                return end == std::string_view::npos ? source.size() : end + close.size();
            }
        }
        for (std::size_t i = begin + 1; i < source.size(); ++i) {
            if (source[i] == '\\') {
                ++i;
            } else if (source[i] == quote || source[i] == '\n') {
                return i + 1;
            }
        }
        return source.size();
    }

}

std::string code_only(std::string_view source)
{
    std::string out;
    out.reserve(source.size());
    bool spaced = false;
    const auto append = [&out, &spaced](std::string_view text) {
        if (spaced && !out.empty() && out.back() != '\n') {
            out += ' ';
        }
        spaced = false;
        out += text;
    };

    for (std::size_t i = 0; i < source.size(); ++i) {
        const char c = source[i];
        const char next = i + 1 < source.size() ? source[i + 1] : '\0';
        if (c == '/' && next == '/') {
            const std::size_t end = source.find('\n', i);
            i = (end == std::string_view::npos ? source.size() : end) - 1;
            spaced = true;
        } else if (c == '/' && next == '*') {
            const std::size_t end = source.find("*/", i + 2);
            i = (end == std::string_view::npos ? source.size() : end + 2) - 1;
            spaced = true;
        } else if (c == '"' || (c == '\'' && (i == 0 || !is_word(source[i - 1])))) {
            const std::size_t end = literal_end(source, i);
            append(source.substr(i, end - i));
            i = end - 1;
        } else if (c == '\n') {
            if (!out.empty() && out.back() != '\n') {
                out += '\n';
            }
            spaced = false;
        } else if (c == ' ' || c == '\t' || c == '\r') {
            spaced = true;
        } else {
            append(source.substr(i, 1));
        }
    }
    return out;
}

std::optional<std::string> fingerprint_value(const Fingerprint& fingerprint)
{
    Hash hash;
    std::error_code code;

    const auto size = std::filesystem::file_size(fingerprint.executable, code);
    if (code) {
        return std::nullopt;
    }
    const auto written = std::filesystem::last_write_time(fingerprint.executable, code);
    if (code) {
        return std::nullopt;
    }
    hash.add(std::to_string(size));
    hash.add(std::to_string(written.time_since_epoch().count()));

    for (const auto& setting : fingerprint.settings) {
        hash.add(setting);
    }
    for (const auto& path : fingerprint.dependencies) {
        const auto text = read_text(path);
        if (!text) {
            return std::nullopt;
        }
        hash.add(path.generic_string());
        hash.add(code_only(*text));
    }
    for (const auto& path : fingerprint.outputs) {
        auto text = read_text(path);
        if (!text) {
            return std::nullopt;
        }
        std::erase(*text, '\r');
        hash.add(path.generic_string());
        hash.add(*text);
    }
    return hash.text();
}

std::optional<std::vector<std::filesystem::path>> unchanged_since(
    const std::filesystem::path& state, Fingerprint current)
{
    std::ifstream stream(state);
    std::string line;
    std::string recorded;
    if (!std::getline(stream, line) || line != state_header || !std::getline(stream, recorded)) {
        return std::nullopt;
    }
    current.dependencies.clear();
    while (std::getline(stream, line)) {
        current.dependencies.emplace_back(line);
    }
    if (fingerprint_value(current) != std::optional<std::string>(recorded)) {
        return std::nullopt;
    }
    return std::move(current.dependencies);
}

bool record_fingerprint(const std::filesystem::path& state, const Fingerprint& fingerprint, std::string& error)
{
    const auto value = fingerprint_value(fingerprint);
    if (!value) {
        error = "cannot read the files behind " + state.string();
        return false;
    }
    std::string text = std::string(state_header) + "\n" + *value + "\n";
    for (const auto& path : fingerprint.dependencies) {
        text += path.generic_string() + "\n";
    }
    std::error_code code;
    if (state.has_parent_path()) {
        std::filesystem::create_directories(state.parent_path(), code);
    }
    return write_if_changed(state, text, error);
}

}
