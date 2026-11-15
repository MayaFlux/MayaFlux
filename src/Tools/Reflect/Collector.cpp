#include "Collector.hpp"

namespace MayaFlux::Reflect::Tool {

namespace {

    constexpr const char* request_id = "request";
    constexpr const char* request_pattern = "mf_reflection_request_";

    bool nameable(const clang::CXXRecordDecl* record)
    {
        if (record->isDependentContext()) {
            return false;
        }
        const clang::Decl* current = record;
        while (true) {
            if (current->getAccess() == clang::AS_private || current->getAccess() == clang::AS_protected) {
                return false;
            }
            const clang::DeclContext* parent = current->getDeclContext();
            if (parent->isTranslationUnit()) {
                return true;
            }
            if (const auto* space = llvm::dyn_cast<clang::NamespaceDecl>(parent)) {
                if (space->isAnonymousNamespace()) {
                    return false;
                }
            } else if (const auto* outer = llvm::dyn_cast<clang::CXXRecordDecl>(parent)) {
                if (outer->getName().empty() || outer->isDependentContext()) {
                    return false;
                }
            } else if (!llvm::isa<clang::LinkageSpecDecl>(parent)) {
                return false;
            }
            current = clang::Decl::castFromDeclContext(parent);
        }
    }

    void collect_nested(clang::QualType type, std::vector<const clang::CXXRecordDecl*>& out)
    {
        const clang::Type* underlying = type.getNonReferenceType()->getBaseElementTypeUnsafe();
        const clang::CXXRecordDecl* record = underlying->getAsCXXRecordDecl();
        if (record != nullptr) {
            record = record->getDefinition();
        }
        if (record == nullptr) {
            return;
        }
        if (const auto* specialization = llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(record)) {
            if (!record->isInStdNamespace()) {
                return;
            }
            for (const clang::TemplateArgument& argument : specialization->getTemplateArgs().asArray()) {
                if (argument.getKind() == clang::TemplateArgument::Type) {
                    collect_nested(argument.getAsType(), out);
                }
            }
            return;
        }
        if (record->isInStdNamespace() || record->isUnion() || !record->isAggregate() || !nameable(record)) {
            return;
        }
        if (std::ranges::find(out, record) == out.end()) {
            out.push_back(record);
        }
    }

    bool collect_members(const clang::CXXRecordDecl* record, std::vector<std::string>& names,
        std::vector<const clang::CXXRecordDecl*>& nested, std::string& error)
    {
        for (const auto& base : record->bases()) {
            if (base.getAccessSpecifier() != clang::AS_public) {
                continue;
            }
            const clang::CXXRecordDecl* parent = base.getType()->getAsCXXRecordDecl();
            if (parent != nullptr) {
                parent = parent->getDefinition();
            }
            if (parent == nullptr) {
                error = "a public base class is not a complete record";
                return false;
            }
            if (!collect_members(parent, names, nested, error)) {
                return false;
            }
        }
        for (const clang::FieldDecl* field : record->fields()) {
            if (field->getAccess() != clang::AS_public) {
                continue;
            }
            const std::string name = field->getNameAsString();
            if (name.empty()) {
                error = "an anonymous member cannot use a member-pointer descriptor";
                return false;
            }
            if (field->isBitField() || field->getType()->isReferenceType()) {
                error = "member " + name + " is a bit-field or reference and cannot use a member-pointer descriptor";
                return false;
            }
            if (std::ranges::find(names, name) != names.end()) {
                error = "member name " + name + " appears in more than one class of the hierarchy";
                return false;
            }
            names.push_back(name);
            collect_nested(field->getType(), nested);
        }
        return true;
    }

    std::filesystem::path real_path(llvm::StringRef file)
    {
        llvm::SmallString<256> resolved;
        if (llvm::sys::fs::real_path(file, resolved)) {
            return { file.str() };
        }
        return { resolved.str().str() };
    }

    std::filesystem::path file_of(const clang::SourceManager& manager, clang::SourceLocation location)
    {
        return real_path(manager.getFilename(manager.getExpansionLoc(location)));
    }

}

bool RequestCollector::store(const std::filesystem::path& header, RecordRequest request, const std::string& where)
{
    HeaderRequests& entry = m_requests[header];
    entry.header = header;
    const auto existing = std::ranges::find(entry.records, request.name, &RecordRequest::name);
    if (existing != entry.records.end()) {
        existing->line = std::max(existing->line, request.line);
        return false;
    }
    for (const auto& [other_header, other] : m_requests) {
        if (other_header != header
            && std::ranges::find(other.records, request.name, &RecordRequest::name) != other.records.end()) {
            m_errors.push_back(where + request.name + ": already described in " + other_header.string());
            return false;
        }
    }
    entry.records.push_back(std::move(request));
    return true;
}

void RequestCollector::describe_nested(const clang::CXXRecordDecl* record, const std::filesystem::path& header,
    unsigned line, const clang::SourceManager& manager)
{
    RecordRequest request;
    request.name = record->getQualifiedNameAsString();
    request.line = line;

    std::vector<const clang::CXXRecordDecl*> nested;
    std::string error;
    const std::string where = header.string() + ": ";
    if (!collect_members(record, request.members, nested, error)) {
        m_errors.push_back(where + "nested type " + request.name + ": " + error);
        return;
    }
    if (!store(header, std::move(request), where)) {
        return;
    }
    for (const clang::CXXRecordDecl* child : nested) {
        if (file_of(manager, child->getLocation()) == header) {
            describe_nested(child, header, line, manager);
        }
    }
}

void RequestCollector::attach(clang::ast_matchers::MatchFinder& finder)
{
    using namespace clang::ast_matchers;
    finder.addMatcher(typeAliasDecl(matchesName(request_pattern)).bind(request_id), this);
}

void RequestCollector::run(const clang::ast_matchers::MatchFinder::MatchResult& result)
{
    const auto* alias = result.Nodes.getNodeAs<clang::TypeAliasDecl>(request_id);
    if (alias == nullptr) {
        return;
    }

    ++m_matched;

    const clang::SourceManager& manager = *result.SourceManager;
    const clang::SourceLocation location = manager.getExpansionLoc(alias->getBeginLoc());
    if (!location.isValid() || manager.isInSystemHeader(location)) {
        ++m_skipped;
        return;
    }

    const std::filesystem::path header = real_path(manager.getFilename(location));
    const unsigned line = manager.getSpellingLineNumber(location);
    const std::string where = header.string() + ":" + std::to_string(line) + ": ";
    const std::string spelling = alias->getUnderlyingType().getAsString();

    if (!alias->getDeclContext()->getRedeclContext()->isFileContext()) {
        m_errors.push_back(where + "MF_DESCRIBE must appear at namespace scope");
        return;
    }

    const clang::CXXRecordDecl* record = alias->getUnderlyingType()->getAsCXXRecordDecl();
    if (record != nullptr) {
        record = record->getDefinition();
    }
    if (record == nullptr) {
        m_errors.push_back(where + spelling + ": request must name a complete struct or class");
        return;
    }
    if (llvm::isa<clang::ClassTemplateSpecializationDecl>(record) || record->isUnion() || !nameable(record)) {
        m_errors.push_back(where + spelling + ": template, union, anonymous and inaccessible records are unsupported");
        return;
    }

    unsigned anchor = line;
    const clang::NamespaceDecl* outermost = nullptr;
    for (const clang::DeclContext* context = alias->getLexicalDeclContext();
         context != nullptr && !context->isTranslationUnit(); context = context->getLexicalParent()) {
        if (const auto* space = llvm::dyn_cast<clang::NamespaceDecl>(context)) {
            outermost = space;
        }
    }
    if (outermost != nullptr && outermost->getRBraceLoc().isValid()) {
        anchor = manager.getSpellingLineNumber(manager.getExpansionLoc(outermost->getRBraceLoc()));
    }

    RecordRequest request;
    request.name = record->getQualifiedNameAsString();
    request.line = anchor;

    std::vector<const clang::CXXRecordDecl*> nested;
    std::string error;
    if (!collect_members(record, request.members, nested, error)) {
        m_errors.push_back(where + request.name + ": " + error);
        return;
    }
    if (!store(header, std::move(request), where)) {
        return;
    }
    for (const clang::CXXRecordDecl* child : nested) {
        if (file_of(manager, child->getLocation()) == header) {
            describe_nested(child, header, anchor, manager);
        }
    }
}

}
