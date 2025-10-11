#pragma once

#include "SymbolRegistry.hpp"

#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Frontend/FrontendAction.h>

namespace Lila {

class ClangSymbolParser {
public:
    static std::vector<Declaration> parse_declarations(
        const std::string& code,
        const std::vector<std::string>& compile_args = { "-std=c++23" });

    static std::vector<Declaration> parse_from_file(
        const std::string& filepath,
        const std::vector<std::string>& compile_args);

    static std::vector<Declaration> parse_with_semantic_analysis(
        const std::string& filepath,
        const std::vector<std::string>& compile_args);

    static std::string deduce_auto_type(
        const std::string& filepath,
        const std::string& variable_name,
        const std::vector<std::string>& compile_args);
};

class AutoTypeVisitor : public clang::RecursiveASTVisitor<AutoTypeVisitor> {
public:
    std::string deduced_type;
    std::string target_variable;
    clang::ASTContext* context = nullptr;
    clang::SourceManager* source_manager = nullptr;

    AutoTypeVisitor(std::string var_name)
        : target_variable(std::move(var_name))
    {
    }

    bool VisitVarDecl(clang::VarDecl* var_decl);

private:
    std::string normalize_type_string(const std::string& type_str);
};

class AutoTypeAction : public clang::ASTFrontendAction {
public:
    explicit AutoTypeAction(AutoTypeVisitor* visitor);

    std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
        clang::CompilerInstance& compiler, llvm::StringRef) override;

private:
    AutoTypeVisitor* visitor_;
};

class AutoTypeConsumer : public clang::ASTConsumer {
public:
    explicit AutoTypeConsumer(AutoTypeVisitor* visitor);

    void HandleTranslationUnit(clang::ASTContext& context) override;

private:
    AutoTypeVisitor* visitor_;
};

} // namespace Lila
