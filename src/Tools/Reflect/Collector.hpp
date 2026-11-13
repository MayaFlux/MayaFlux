#pragma once

#include "Request.hpp"

namespace MayaFlux::Reflect::Tool {

/**
 * @class RequestCollector
 * @brief Turns each MF_DESCRIBE alias in the scanned translation unit into a RecordRequest.
 *
 * Attach it to a MatchFinder, run the finder, then read requests() and errors().
 * Requests are grouped by the header holding the marker.
 */
class RequestCollector : public clang::ast_matchers::MatchFinder::MatchCallback {
public:
    /** @brief Registers the matcher for the marker aliases on @p finder. */
    void attach(clang::ast_matchers::MatchFinder& finder);

    void run(const clang::ast_matchers::MatchFinder::MatchResult& result) override;

    [[nodiscard]] const RequestMap& requests() const noexcept { return m_requests; }
    [[nodiscard]] const std::vector<std::string>& errors() const noexcept { return m_errors; }

    /** @brief Marker aliases the matcher reached, before any filtering. */
    [[nodiscard]] std::size_t matched() const noexcept { return m_matched; }

    /** @brief Marker aliases ignored for sitting in a system header or an invalid location. */
    [[nodiscard]] std::size_t skipped() const noexcept { return m_skipped; }

private:
    bool store(const std::filesystem::path& header, RecordRequest request, const std::string& where);

    void describe_nested(const clang::CXXRecordDecl* record, const std::filesystem::path& header,
        unsigned line, const clang::SourceManager& manager);

    RequestMap m_requests;
    std::vector<std::string> m_errors;
    std::size_t m_matched {};
    std::size_t m_skipped {};
};

}
