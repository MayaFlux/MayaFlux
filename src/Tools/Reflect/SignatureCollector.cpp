#include "SignatureCollector.hpp"

namespace MayaFlux::Reflect::Tool {

namespace {

    constexpr const char* record_id = "record";

    bool is_word(char c)
    {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
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

    std::set<std::string> nested_names(const clang::CXXRecordDecl* record)
    {
        std::set<std::string> names;
        for (const clang::Decl* declaration : record->decls()) {
            if (declaration->isImplicit()) {
                continue;
            }
            const bool type_like = llvm::isa<clang::CXXRecordDecl>(declaration)
                || llvm::isa<clang::EnumDecl>(declaration)
                || llvm::isa<clang::TypeAliasDecl>(declaration)
                || llvm::isa<clang::TypedefDecl>(declaration);
            if (!type_like) {
                continue;
            }
            const auto* named = llvm::cast<clang::NamedDecl>(declaration);
            if (named->getIdentifier() != nullptr) {
                names.insert(named->getName().str());
            }
        }
        return names;
    }

    std::string relative_to_root(const std::string& spelling)
    {
        constexpr std::string_view prefix = "MayaFlux::";
        std::string out;
        std::size_t cursor = 0;
        while (cursor < spelling.size()) {
            const std::size_t at = spelling.find(prefix, cursor);
            if (at == std::string::npos) {
                break;
            }
            out.append(spelling, cursor, at - cursor);
            const bool starts_name = at == 0 || (!is_word(spelling[at - 1]) && spelling[at - 1] != ':');
            if (!starts_name) {
                out += prefix;
            }
            cursor = at + prefix.size();
        }
        out.append(spelling, cursor, std::string::npos);
        return out;
    }

    std::string qualify_nested(std::string value, const std::string& owner, const std::set<std::string>& names)
    {
        std::vector<std::string> ordered(names.begin(), names.end());
        std::ranges::sort(ordered, [](const std::string& a, const std::string& b) { return a.size() > b.size(); });
        for (const std::string& name : ordered) {
            std::string result;
            std::size_t cursor = 0;
            while (cursor < value.size()) {
                const std::size_t at = value.find(name, cursor);
                if (at == std::string::npos) {
                    break;
                }
                const std::size_t end = at + name.size();
                const bool before = at == 0 || (!is_word(value[at - 1]) && value[at - 1] != ':');
                const bool after = end >= value.size() || !is_word(value[end]);
                result.append(value, cursor, at - cursor);
                if (before && after) {
                    result += owner + "::" + name;
                } else {
                    result += name;
                }
                cursor = end;
            }
            result.append(value, cursor, std::string::npos);
            value = std::move(result);
        }
        return value;
    }

    std::string spell(clang::QualType type, const clang::ASTContext& context)
    {
        const clang::QualType qualified = clang::TypeName::getFullyQualifiedType(type, context, false);
        return relative_to_root(qualified.getAsString(context.getPrintingPolicy()));
    }

    /**
     * Gathers the headers under the source root that declare the MayaFlux types a signature
     * names, reading the types as written so an alias contributes its own header.
     */
    class HeaderCollector {
    public:
        HeaderCollector(const clang::SourceManager& manager, std::filesystem::path source_root,
            const std::set<std::string>& excluded, std::set<std::filesystem::path>& out)
            : m_manager(manager)
            , m_source_root(std::move(source_root))
            , m_excluded(excluded)
            , m_out(out)
        {
        }

        void add_type(clang::QualType type)
        {
            type = type.getNonReferenceType();
            while (!type.isNull() && type->getAs<clang::PointerType>() != nullptr) {
                type = type->getAs<clang::PointerType>()->getPointeeType();
            }
            if (type.isNull()) {
                return;
            }
            type = clang::QualType(type->getBaseElementTypeUnsafe(), 0);

            if (const auto* alias = type->getAs<clang::TypedefType>()) {
                add_declaration(alias->getDecl());
                return;
            }
            if (const auto* specialization = type->getAs<clang::TemplateSpecializationType>()) {
                for (const clang::TemplateArgument& argument : specialization->template_arguments()) {
                    if (argument.getKind() == clang::TemplateArgument::Type) {
                        add_type(argument.getAsType());
                    }
                }
                return;
            }
            if (const auto* function = type->getAs<clang::FunctionProtoType>()) {
                add_type(function->getReturnType());
                for (const clang::QualType parameter : function->param_types()) {
                    add_type(parameter);
                }
                return;
            }
            if (const clang::TagDecl* tag = type->getAsTagDecl()) {
                add_tag(tag);
            }
        }

        void add_value(const clang::ValueDecl* value)
        {
            if (const auto* enumerator = llvm::dyn_cast<clang::EnumConstantDecl>(value)) {
                if (const auto* owner = llvm::dyn_cast<clang::EnumDecl>(enumerator->getDeclContext())) {
                    add_tag(owner);
                }
                return;
            }
            if (const auto* owner = llvm::dyn_cast<clang::TagDecl>(value->getDeclContext())) {
                add_tag(owner);
            }
        }

    private:
        void add_tag(const clang::TagDecl* tag)
        {
            if (m_excluded.contains(tag->getQualifiedNameAsString())) {
                return;
            }
            add_declaration(tag);
        }

        void add_declaration(const clang::Decl* declaration)
        {
            const std::filesystem::path relative
                = file_of(m_manager, declaration->getLocation()).lexically_relative(m_source_root);
            if (relative.empty() || *relative.begin() != "MayaFlux" || relative.extension() != ".hpp") {
                return;
            }
            m_out.insert(relative);
        }

        const clang::SourceManager& m_manager;
        std::filesystem::path m_source_root;
        const std::set<std::string>& m_excluded;
        std::set<std::filesystem::path>& m_out;
    };

    /** Visits a default argument and reports the types and enumerators it names. */
    class DefaultArgumentVisitor : public clang::RecursiveASTVisitor<DefaultArgumentVisitor> {
    public:
        explicit DefaultArgumentVisitor(HeaderCollector& headers)
            : m_headers(headers)
        {
        }

        bool VisitExpr(clang::Expr* expression)
        {
            m_headers.add_type(expression->getType());
            return true;
        }

        bool VisitDeclRefExpr(clang::DeclRefExpr* reference)
        {
            m_headers.add_value(reference->getDecl());
            return true;
        }

    private:
        HeaderCollector& m_headers;
    };

    std::string argument_text(const clang::ParmVarDecl* parameter, const clang::ASTContext& context)
    {
        const clang::SourceManager& manager = context.getSourceManager();
        const clang::CharSourceRange range = manager.getExpansionRange(parameter->getDefaultArg()->getSourceRange());
        bool invalid = false;
        const llvm::StringRef text = clang::Lexer::getSourceText(range, manager, context.getLangOpts(), &invalid);
        std::string spelling = invalid ? std::string {} : text.str();
        if (spelling.starts_with('=')) {
            const std::size_t start = spelling.find_first_not_of(" \t", 1);
            spelling = start == std::string::npos ? std::string {} : spelling.substr(start);
        }
        return spelling;
    }

}

SignatureCollector::SignatureCollector(std::vector<SignatureEntry> entries, std::filesystem::path source_root)
    : m_entries(std::move(entries))
    , m_source_root(std::move(source_root))
    , m_found(m_entries.size())
{
    for (const SignatureEntry& entry : m_entries) {
        m_simple_names.insert(entry.type_name.substr(entry.type_name.rfind("::") + 2));
        m_excluded.insert(entry.type_name);
    }
    m_excluded.insert("MayaFlux::Nodes::Node");
    m_excluded.insert("MayaFlux::Nodes::Network::NodeNetwork");
    m_excluded.insert("MayaFlux::Core::VKImage");
}

void SignatureCollector::attach(clang::ast_matchers::MatchFinder& finder)
{
    using namespace clang::ast_matchers;
    finder.addMatcher(cxxRecordDecl(isDefinition()).bind(record_id), this);
}

void SignatureCollector::run(const clang::ast_matchers::MatchFinder::MatchResult& result)
{
    const auto* record = result.Nodes.getNodeAs<clang::CXXRecordDecl>(record_id);
    if (record == nullptr || record->getIdentifier() == nullptr || record->isDependentContext()
        || !m_simple_names.contains(record->getName().str())) {
        return;
    }

    const std::string qualified = record->getQualifiedNameAsString();
    const auto entry = std::ranges::find(m_entries, qualified, &SignatureEntry::type_name);
    if (entry == m_entries.end()) {
        return;
    }
    std::optional<SignatureClass>& slot = m_found[static_cast<std::size_t>(entry - m_entries.begin())];
    if (slot) {
        return;
    }

    const clang::ASTContext& context = *result.Context;
    const clang::SourceManager& manager = *result.SourceManager;

    SignatureClass info;
    info.entry = *entry;
    info.entry.header = file_of(manager, record->getLocation());
    info.abstract = record->isAbstract();
    if (info.abstract) {
        slot = std::move(info);
        return;
    }

    const std::set<std::string> nested = nested_names(record);
    HeaderCollector headers(manager, m_source_root, m_excluded, info.includes);

    for (const clang::Decl* declaration : record->decls()) {
        if (const auto* declared_template = llvm::dyn_cast<clang::FunctionTemplateDecl>(declaration)) {
            if (llvm::isa<clang::CXXConstructorDecl>(declared_template->getTemplatedDecl())) {
                m_errors.push_back(entry->name + ": constructor template needs explicit generator support");
                return;
            }
            continue;
        }
        const auto* constructor = llvm::dyn_cast<clang::CXXConstructorDecl>(declaration);
        if (constructor == nullptr || constructor->isImplicit() || constructor->isDeleted()
            || constructor->getAccess() != clang::AS_public) {
            continue;
        }

        ConstructorSignature built;
        unsigned number = 0;
        for (const clang::ParmVarDecl* parameter : constructor->parameters()) {
            SignatureParameter item;
            item.type = spell(parameter->getType(), context);
            if (item.type.find("(*)") != std::string::npos || item.type.find("(&)") != std::string::npos) {
                m_errors.push_back(entry->name + ": cannot spell parameter " + std::to_string(number)
                    + " as a method parameter");
                return;
            }
            item.name = parameter->getNameAsString();
            if (item.name.empty()) {
                item.name = "arg" + std::to_string(number);
            }

            headers.add_type(parameter->getType());
            if (parameter->hasDefaultArg() && !parameter->hasUnparsedDefaultArg()
                && !parameter->hasUninstantiatedDefaultArg()) {
                const std::string text = argument_text(parameter, context);
                if (text.empty()) {
                    m_errors.push_back(entry->name + ": cannot recover default for parameter " + std::to_string(number));
                    return;
                }
                item.default_value = qualify_nested(text, qualified, nested);
                DefaultArgumentVisitor visitor(headers);
                visitor.TraverseStmt(const_cast<clang::Expr*>(parameter->getDefaultArg()));
            }
            built.parameters.push_back(std::move(item));
            ++number;
        }
        info.constructors.push_back(std::move(built));
    }

    if (info.constructors.empty()) {
        m_errors.push_back(entry->name + ": no public source-declared constructor found");
        return;
    }
    slot = std::move(info);
}

std::vector<SignatureClass> SignatureCollector::classes() const
{
    std::vector<SignatureClass> found;
    for (const auto& slot : m_found) {
        if (slot) {
            found.push_back(*slot);
        }
    }
    return found;
}

std::vector<std::string> SignatureCollector::missing() const
{
    std::vector<std::string> names;
    for (std::size_t i = 0; i < m_entries.size(); ++i) {
        if (!m_found[i]) {
            names.push_back(m_entries[i].name);
        }
    }
    return names;
}

}
