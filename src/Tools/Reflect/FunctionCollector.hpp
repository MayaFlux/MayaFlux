#pragma once

#include "FunctionRequest.hpp"

namespace MayaFlux::Reflect::Tool {

/**
 * @class FunctionCollector
 * @brief Turns each MF_DESCRIBE_FUNCTION variable in the scanned translation unit into a FunctionRequest.
 *
 * Attach it to a MatchFinder, run the finder, then read requests() and errors(). Requests are
 * grouped by the header holding the marker. A function that is a member, still a template, or
 * has a parameter without a name is an error, since a parameter name is what a caller refers to.
 *
 * The scan switches sidecars off, so a header that holds requests must not use what its own
 * sidecar generates. Keep the described functions in a header of their own.
 */
class FunctionCollector : public clang::ast_matchers::MatchFinder::MatchCallback {
public:
    /** @brief Registers the matcher for the marker variables on @p finder. */
    void attach(clang::ast_matchers::MatchFinder& finder);

    void run(const clang::ast_matchers::MatchFinder::MatchResult& result) override;

    [[nodiscard]] const FunctionMap& requests() const noexcept { return m_requests; }
    [[nodiscard]] const std::vector<std::string>& errors() const noexcept { return m_errors; }

    /** @brief Marker variables the matcher reached, before any filtering. */
    [[nodiscard]] std::size_t matched() const noexcept { return m_matched; }

private:
    FunctionMap m_requests;
    std::vector<std::string> m_errors;
    std::size_t m_matched {};
};

}
