#include "FunctionCollector.hpp"

namespace MayaFlux::Reflect::Tool {

namespace {

    constexpr const char* request_id = "request";
    constexpr const char* request_pattern = "mf_signature_request_";

    std::filesystem::path real_path(llvm::StringRef file)
    {
        llvm::SmallString<256> resolved;
        if (llvm::sys::fs::real_path(file, resolved)) {
            return { file.str() };
        }
        return { resolved.str().str() };
    }

    /** Finds the first function a marker's initializer refers to. */
    class FunctionFinder : public clang::RecursiveASTVisitor<FunctionFinder> {
    public:
        bool VisitDeclRefExpr(clang::DeclRefExpr* reference)
        {
            if (const auto* function = llvm::dyn_cast<clang::FunctionDecl>(reference->getDecl())) {
                m_function = function;
                return false;
            }
            return true;
        }

        [[nodiscard]] const clang::FunctionDecl* function() const noexcept { return m_function; }

    private:
        const clang::FunctionDecl* m_function {};
    };

    bool overloaded(const clang::FunctionDecl* function)
    {
        const clang::DeclContext* context = function->getDeclContext()->getRedeclContext();
        std::set<const clang::Decl*> entities;
        for (const clang::NamedDecl* found : context->lookup(function->getDeclName())) {
            if (llvm::isa<clang::FunctionDecl>(found) || llvm::isa<clang::FunctionTemplateDecl>(found)) {
                entities.insert(found->getCanonicalDecl());
            }
        }
        return entities.size() > 1;
    }

    clang::PrintingPolicy qualified_policy(const clang::ASTContext& context)
    {
        clang::PrintingPolicy policy = context.getPrintingPolicy();
        policy.SuppressScope = false;
        policy.FullyQualifiedName = true;
        return policy;
    }

    /** The name a call is written with: qualified, with the template arguments of a specialization. */
    std::string callee_of(const clang::FunctionDecl* function, const clang::ASTContext& context)
    {
        const std::string name = "::" + function->getQualifiedNameAsString();
        if (const clang::TemplateArgumentList* arguments = function->getTemplateSpecializationArgs()) {
            std::string text;
            llvm::raw_string_ostream stream(text);
            clang::printTemplateArgumentList(stream, arguments->asArray(), qualified_policy(context));
            return name + text;
        }
        return name;
    }

    /**
     * How the function is written as a template argument: its address, with the template
     * arguments of a specialization, or cast to its own type when the name is overloaded.
     */
    std::string expression_of(const clang::FunctionDecl* function, const clang::ASTContext& context)
    {
        const std::string name = "::" + function->getQualifiedNameAsString();

        if (function->getTemplateSpecializationArgs() != nullptr) {
            return "&" + callee_of(function, context);
        }
        if (!overloaded(function)) {
            return "&" + name;
        }
        const clang::QualType pointer = clang::TypeName::getFullyQualifiedType(
            context.getPointerType(function->getType()), context, true);
        return "static_cast<" + pointer.getAsString(context.getPrintingPolicy()) + ">(&" + name + ")";
    }

}

void FunctionCollector::attach(clang::ast_matchers::MatchFinder& finder)
{
    using namespace clang::ast_matchers;
    finder.addMatcher(varDecl(matchesName(request_pattern)).bind(request_id), this);
}

void FunctionCollector::run(const clang::ast_matchers::MatchFinder::MatchResult& result)
{
    const auto* variable = result.Nodes.getNodeAs<clang::VarDecl>(request_id);
    if (variable == nullptr) {
        return;
    }

    ++m_matched;

    const clang::SourceManager& manager = *result.SourceManager;
    const clang::SourceLocation location = manager.getExpansionLoc(variable->getBeginLoc());
    if (!location.isValid() || manager.isInSystemHeader(location)) {
        return;
    }

    const std::filesystem::path header = real_path(manager.getFilename(location));
    const unsigned line = manager.getSpellingLineNumber(location);
    const std::string where = header.string() + ":" + std::to_string(line) + ": ";

    if (!variable->getDeclContext()->getRedeclContext()->isFileContext()) {
        m_errors.push_back(where + "MF_DESCRIBE_FUNCTION must appear at namespace scope");
        return;
    }

    FunctionFinder finder;
    if (variable->getInit() != nullptr) {
        finder.TraverseStmt(const_cast<clang::Expr*>(variable->getInit()));
    }
    const clang::FunctionDecl* function = finder.function();
    if (function == nullptr) {
        m_errors.push_back(where + "request must name a function");
        return;
    }

    FunctionRequest request;
    request.name = function->getQualifiedNameAsString();
    const std::string label = where + request.name + ": ";

    if (const auto* method = llvm::dyn_cast<clang::CXXMethodDecl>(function); method != nullptr && !method->isStatic()) {
        m_errors.push_back(label + "member functions are unsupported");
        return;
    }
    if (function->getDescribedFunctionTemplate() != nullptr) {
        m_errors.push_back(label + "a function template needs explicit template arguments, as &name<T>");
        return;
    }

    bool defaulted = false;
    unsigned number = 0;
    for (const clang::ParmVarDecl* parameter : function->parameters()) {
        const std::string name = parameter->getNameAsString();
        if (name.empty()) {
            m_errors.push_back(label + "parameter " + std::to_string(number) + " has no name");
            return;
        }
        defaulted = defaulted || parameter->hasDefaultArg();
        if (!defaulted) {
            ++request.required;
        }
        request.parameters.push_back(name);
        ++number;
    }

    request.expression = expression_of(function, *result.Context);
    request.callee = callee_of(function, *result.Context);

    unsigned anchor = line;
    const clang::NamespaceDecl* outermost = nullptr;
    for (const clang::DeclContext* context = variable->getLexicalDeclContext();
         context != nullptr && !context->isTranslationUnit(); context = context->getLexicalParent()) {
        if (const auto* space = llvm::dyn_cast<clang::NamespaceDecl>(context)) {
            outermost = space;
        }
    }
    if (outermost != nullptr && outermost->getRBraceLoc().isValid()) {
        anchor = manager.getSpellingLineNumber(manager.getExpansionLoc(outermost->getRBraceLoc()));
    }
    request.line = anchor;

    HeaderFunctions& entry = m_requests[header];
    entry.header = header;
    const auto existing = std::ranges::find(entry.functions, request.expression, &FunctionRequest::expression);
    if (existing != entry.functions.end()) {
        existing->line = std::max(existing->line, request.line);
        return;
    }
    for (const auto& [other_header, other] : m_requests) {
        if (other_header != header
            && std::ranges::find(other.functions, request.expression, &FunctionRequest::expression) != other.functions.end()) {
            m_errors.push_back(label + "already described in " + other_header.string());
            return;
        }
    }
    entry.functions.push_back(std::move(request));
}

}
