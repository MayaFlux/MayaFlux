#pragma once

#include <regex>

namespace Lila {

/**
 * @brief Dead simple symbol parser using regex
 *
 * This is a TEMPORARY solution for POC. We'll replace with Clang AST later.
 * Only handles basic variable declarations for now.
 */
class SimpleSymbolParser {
public:
    struct Declaration {
        std::string type;
        std::string name;
        bool has_initializer {};
    };

    /**
     * @brief Parse simple variable declarations from code
     *
     * Matches patterns like:
     * - int x = 5;
     * - auto y = something;
     * - std::shared_ptr<Foo> ptr = ...;
     */
    static std::vector<Declaration> parse_declarations(const std::string& code)
    {
        std::vector<Declaration> decls;

        std::regex auto_pattern(R"(\bauto\s+(\w+)\s*=)");
        std::smatch match;
        std::string::const_iterator search_start(code.cbegin());

        while (std::regex_search(search_start, code.cend(), match, auto_pattern)) {
            Declaration decl;
            decl.type = "auto";
            decl.name = match[1].str();
            decl.has_initializer = true;
            decls.push_back(decl);
            search_start = match.suffix().first;
        }

        std::regex type_pattern(R"(\b(int|float|double|bool|size_t|uint32_t|uint64_t)\s+(\w+)\s*[=;])");
        search_start = code.cbegin();

        while (std::regex_search(search_start, code.cend(), match, type_pattern)) {
            Declaration decl;
            decl.type = match[1].str();
            decl.name = match[2].str();
            decl.has_initializer = true;
            decls.push_back(decl);
            search_start = match.suffix().first;
        }

        std::regex std_pattern(R"(\b(std::[\w:]+(?:<[^>]+>)?)\s+(\w+)\s*[=;])");
        search_start = code.cbegin();

        while (std::regex_search(search_start, code.cend(), match, std_pattern)) {
            Declaration decl;
            decl.type = match[1].str();
            decl.name = match[2].str();
            decl.has_initializer = true;
            decls.push_back(decl);
            search_start = match.suffix().first;
        }

        return decls;
    }
};

} // namespace Lila
