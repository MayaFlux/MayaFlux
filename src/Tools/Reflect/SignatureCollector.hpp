#pragma once

#include "SignatureEntries.hpp"

namespace MayaFlux::Reflect::Tool {

/** @brief One constructor parameter: its fully qualified type, its name and its default argument text. */
struct SignatureParameter {
    std::string type;
    std::string name;
    std::optional<std::string> default_value;
};

/** @brief One public constructor a Creator method is generated from. */
struct ConstructorSignature {
    std::vector<SignatureParameter> parameters;
};

/**
 * @struct SignatureClass
 * @brief A registered class as the scan found it.
 *
 * includes holds, relative to the source root, the header of every MayaFlux type and enumerator
 * the constructors' parameter types and default arguments name, except the registered classes
 * themselves.
 */
struct SignatureClass {
    SignatureEntry entry;
    bool abstract {};
    std::vector<ConstructorSignature> constructors;
    std::set<std::filesystem::path> includes;
};

/**
 * @class SignatureCollector
 * @brief Reads the public constructors of each registered class from the scanned translation unit.
 *
 * Attach it to a MatchFinder, run the finder, then read classes(), missing() and errors().
 * Parameter types are spelled fully qualified, so the generated methods compile in any scope.
 * Default arguments are kept as written, with the class's nested type names qualified.
 */
class SignatureCollector : public clang::ast_matchers::MatchFinder::MatchCallback {
public:
    /**
     * @param entries Registered classes, in registry order.
     * @param source_root The directory MayaFlux/ sits in; headers are reported relative to it.
     */
    SignatureCollector(std::vector<SignatureEntry> entries, std::filesystem::path source_root);

    /** @brief Registers the matcher for class definitions on @p finder. */
    void attach(clang::ast_matchers::MatchFinder& finder);

    void run(const clang::ast_matchers::MatchFinder::MatchResult& result) override;

    /** @brief The registered classes that were found, in registry order. */
    [[nodiscard]] std::vector<SignatureClass> classes() const;

    /** @brief Registered classes the scan never reached a definition of. */
    [[nodiscard]] std::vector<std::string> missing() const;

    [[nodiscard]] const std::vector<std::string>& errors() const noexcept { return m_errors; }

private:
    std::vector<SignatureEntry> m_entries;
    std::filesystem::path m_source_root;
    std::vector<std::optional<SignatureClass>> m_found;
    std::unordered_set<std::string> m_simple_names;
    std::set<std::string> m_excluded;
    std::vector<std::string> m_errors;
};

}
