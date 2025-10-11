#pragma once

#include "SymbolRegistry.hpp"

#include <clang/AST/ASTConsumer.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendAction.h>
#include <clang/Tooling/Tooling.h>

#include "fstream"

namespace Lila {

class SymbolVisitor : public clang::RecursiveASTVisitor<SymbolVisitor> {
public:
    std::vector<Declaration> declarations;
    clang::SourceManager* source_manager = nullptr;

    bool VisitVarDecl(clang::VarDecl* var_decl)
    {
        if (!var_decl->isLocalVarDecl() || !var_decl->hasInit()) {
            return true;
        }

        // CRITICAL: Only variables in the MAIN FILE (not headers)
        // Avoids all the __ and using dump. impossible to filter manually
        if (source_manager) {
            clang::SourceLocation loc = var_decl->getLocation();
            if (!source_manager->isInMainFile(loc)) {
                return true; // Note: THIS SKIPS ALL HEADER DECLS
            }
        }

        // Also check if it's inside MY __clang_parse_func
        auto* decl_context = var_decl->getDeclContext();
        if (auto* func = llvm::dyn_cast<clang::FunctionDecl>(decl_context)) {
            if (func->getNameAsString() != "__clang_parse_func") {
                return true;
            }
        } else {
            return true; // Not in any function. TODO: what do we do about new function decls?
        }

        Declaration decl;
        decl.name = var_decl->getNameAsString();

        clang::QualType type = var_decl->getType();
        decl.type = type.getAsString();
        decl.has_initializer = true;

        std::cout << "Clang found: " << decl.type << " " << decl.name << "\n";

        declarations.push_back(decl);
        return true;
    }
};

class SymbolConsumer : public clang::ASTConsumer {
public:
    explicit SymbolConsumer(SymbolVisitor* visitor)
        : visitor_(visitor)
    {
    }

    void HandleTranslationUnit(clang::ASTContext& context) override
    {
        visitor_->source_manager = &context.getSourceManager();
        visitor_->TraverseDecl(context.getTranslationUnitDecl());
    }

private:
    SymbolVisitor* visitor_;
};

class SymbolAction : public clang::ASTFrontendAction {
public:
    explicit SymbolAction(SymbolVisitor* visitor)
        : visitor_(visitor)
    {
    }

    std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
        clang::CompilerInstance&, llvm::StringRef) override
    {
        return std::make_unique<SymbolConsumer>(visitor_);
    }

private:
    SymbolVisitor* visitor_;
};

class ClangSymbolParser {
public:
    // Does not work because of missing context
    static std::vector<Declaration> parse_declarations(
        const std::string& code,
        const std::vector<std::string>& compile_args = { "-std=c++23" })
    {

        SymbolVisitor visitor;

        /* std::string wrapped_code = "#include \"MayaFlux/MayaFlux.hpp\"\n"
                                   "void __parse_func() {\n"
            + code + "\n}\n"; */

        std::string wrapped_code = "void __parse_func() {\n" + code + "\n}\n";

        bool success = clang::tooling::runToolOnCodeWithArgs(
            std::make_unique<SymbolAction>(&visitor),
            wrapped_code,
            compile_args,
            "input.cpp");

        if (!success) {
            std::cerr << "Clang parsing failed, falling back to regex parser\n";
            return {};
        }

        return visitor.declarations;
    }

    static std::vector<Declaration> parse_from_file(
        const std::string& filepath,
        const std::vector<std::string>& compile_args)
    {
        std::cout << "ClangSymbolParser: Parsing " << filepath << "\n";

        SymbolVisitor visitor;

        std::ifstream file(filepath);
        if (!file) {
            std::cerr << "Failed to read parse file\n";
            return {};
        }
        std::string code((std::istreambuf_iterator<char>(file)),
            std::istreambuf_iterator<char>());
        file.close();

        bool success = clang::tooling::runToolOnCodeWithArgs(
            std::make_unique<SymbolAction>(&visitor),
            code,
            compile_args,
            filepath);

        if (!success) {
            std::cerr << "Clang AST parsing failed\n";
            return {};
        }

        // std::cout << "Clang found " << visitor.declarations.size() << " declarations\n";
        return visitor.declarations;
    }
};

} // namespace Lila
