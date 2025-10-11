#include "SymbolActions.hpp"

namespace Lila {

bool SymbolVisitor::VisitVarDecl(clang::VarDecl* var_decl)
{
    if (!var_decl->isLocalVarDecl() || !var_decl->hasInit()) {
        return true;
    }

    if (m_source_manager) {
        clang::SourceLocation loc = var_decl->getLocation();
        if (!m_source_manager->isInMainFile(loc)) {
            return true;
        }
    }

    auto* decl_context = var_decl->getDeclContext();
    if (auto* func = llvm::dyn_cast<clang::FunctionDecl>(decl_context)) {
        if (func->getNameAsString() != "__clang_parse_func") {
            return true;
        }
    } else {
        return true;
    }

    Declaration decl;
    decl.name = var_decl->getNameAsString();
    clang::QualType type = var_decl->getType();

    clang::ASTContext& context = var_decl->getASTContext();

    clang::PrintingPolicy policy = context.getPrintingPolicy();

    policy.SuppressUnwrittenScope = true;
    policy.SuppressTagKeyword = true;
    policy.FullyQualifiedName = false;
    policy.PrintCanonicalTypes = false;

    std::string type_str = type.getAsString(policy);

    if (type_str == "auto" || type->getAs<clang::AutoType>() != nullptr) {
        decl.type = "auto";
        decl.requires_type_deduction = true;
        std::cout << "SymbolVisitor: Found AUTO variable: " << decl.name << "\n";
    } else {
        decl.type = type_str;
        decl.requires_type_deduction = false;
        std::cout << "SymbolVisitor: Found typed variable: " << decl.type << " " << decl.name << "\n";
    }

    decl.has_initializer = true;
    std::cout << "Clang found: " << decl.type << " " << decl.name << "\n";
    m_declarations.push_back(decl);
    return true;
}

SymbolConsumer::SymbolConsumer(SymbolVisitor* visitor)
    : m_visitor(visitor)
{
}

void SymbolConsumer::HandleTranslationUnit(clang::ASTContext& context)
{
    m_visitor->m_source_manager = &context.getSourceManager();
    m_visitor->TraverseDecl(context.getTranslationUnitDecl());
}

SemanticSymbolAction::SemanticSymbolAction(SymbolVisitor* visitor)
    : m_visitor(visitor)
{
}

std::unique_ptr<clang::ASTConsumer> SemanticSymbolAction::CreateASTConsumer(
    clang::CompilerInstance& compiler, llvm::StringRef)
{
    compiler.createSema(clang::TU_Complete, nullptr);
    return std::make_unique<SymbolConsumer>(m_visitor);
}

bool SemanticSymbolAction::BeginSourceFileAction(clang::CompilerInstance& compiler)
{
    compiler.getDiagnostics().setSuppressAllDiagnostics(false);
    compiler.getInvocation().getLangOpts().CPlusPlus = true;
    return true;
}

void SemanticSymbolAction::ExecuteAction()
{
    clang::CompilerInstance& compiler = getCompilerInstance();
    compiler.createSema(clang::TU_Complete, nullptr);
    clang::ASTFrontendAction::ExecuteAction();
}

SymbolAction::SymbolAction(SymbolVisitor* visitor)
    : m_visitor(visitor)
{
}

std::unique_ptr<clang::ASTConsumer> SymbolAction::CreateASTConsumer(
    clang::CompilerInstance&, llvm::StringRef)
{
    return std::make_unique<SymbolConsumer>(m_visitor);
}

}
