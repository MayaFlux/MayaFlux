#pragma once

#include "Lila/SymbolRegistry.hpp"

#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Frontend/FrontendAction.h>

namespace Lila {

class SymbolVisitor : public clang::RecursiveASTVisitor<SymbolVisitor> {
public:
    std::vector<Declaration> m_declarations;
    clang::SourceManager* m_source_manager = nullptr;

    bool VisitVarDecl(clang::VarDecl* var_decl);
};

class SymbolConsumer : public clang::ASTConsumer {
public:
    explicit SymbolConsumer(SymbolVisitor* visitor);

    void HandleTranslationUnit(clang::ASTContext& context) override;

private:
    SymbolVisitor* m_visitor;
};

class SemanticSymbolAction : public clang::ASTFrontendAction {
public:
    explicit SemanticSymbolAction(SymbolVisitor* visitor);

    std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
        clang::CompilerInstance& compiler, llvm::StringRef) override;

    bool BeginSourceFileAction(clang::CompilerInstance& compiler) override;

    void ExecuteAction() override;

private:
    SymbolVisitor* m_visitor;
};

class SymbolAction : public clang::ASTFrontendAction {
public:
    explicit SymbolAction(SymbolVisitor* visitor);

    std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
        clang::CompilerInstance&, llvm::StringRef) override;

private:
    SymbolVisitor* m_visitor;
};

}
