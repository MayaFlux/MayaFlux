#include "ClangSymbolParser.hpp"
#include "Lila/SymbolActions.hpp"

#include "fstream"
#include <regex>

#include <clang/Tooling/Tooling.h>

// #include <clang/AST/ASTConsumer.h>
// #include <clang/AST/RecursiveASTVisitor.h>
// #include <clang/Frontend/CompilerInstance.h>
// #include <clang/Frontend/FrontendAction.h>
// #include <clang/Tooling/Tooling.h>

namespace Lila {

std::vector<Declaration> ClangSymbolParser::parse_declarations(
    const std::string& code,
    const std::vector<std::string>& compile_args)
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

    return visitor.m_declarations;
}

std::vector<Declaration> ClangSymbolParser::parse_from_file(
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
    return visitor.m_declarations;
}

std::vector<Declaration> ClangSymbolParser::parse_with_semantic_analysis(
    const std::string& filepath,
    const std::vector<std::string>& compile_args)
{
    std::cout << "=== SEMANTIC ANALYSIS PARSING ===\n";

    SymbolVisitor visitor;

    std::ifstream file(filepath);
    if (!file) {
        std::cerr << "Failed to read parse file: " << filepath << "\n";
        return {};
    }
    std::string code((std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>());
    file.close();

    std::cout << "Compile args for semantic analysis:\n";
    for (const auto& arg : compile_args) {
        std::cout << "  " << arg << "\n";
    }

    bool success = clang::tooling::runToolOnCodeWithArgs(
        std::make_unique<SemanticSymbolAction>(&visitor),
        code,
        compile_args,
        filepath);

    if (!success) {
        std::cerr << "Clang semantic analysis parsing failed\n";
        return {};
    }

    std::cout << "Semantic analysis completed. Found " << visitor.m_declarations.size() << " declarations\n";
    return visitor.m_declarations;
}

std::string ClangSymbolParser::deduce_auto_type(
    const std::string& filepath,
    const std::string& variable_name,
    const std::vector<std::string>& compile_args)
{
    std::cout << "=== AUTO TYPE DEDUCTION ===\n";
    std::cout << "Variable: " << variable_name << "\n";

    AutoTypeVisitor visitor(variable_name);

    std::ifstream file(filepath);
    std::string code((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    bool success = clang::tooling::runToolOnCodeWithArgs(
        std::make_unique<AutoTypeAction>(&visitor),
        code,
        compile_args,
        filepath);

    if (!success) {
        std::cerr << "Auto type deduction parsing failed\n";
        return "";
    }

    if (!visitor.deduced_type.empty()) {
        std::cout << "SUCCESS: Deduced type '" << visitor.deduced_type << "' for '" << variable_name << "'\n";
        return visitor.deduced_type;
    } else {
        std::cout << "FAILED: Could not deduce type for '" << variable_name << "'\n";
        return "";
    }
}

bool AutoTypeVisitor::VisitVarDecl(clang::VarDecl* var_decl)
{
    if (var_decl->getNameAsString() != target_variable) {
        return true;
    }

    if (!var_decl->isLocalVarDecl() || !var_decl->hasInit()) {
        return true;
    }

    if (source_manager) {
        clang::SourceLocation loc = var_decl->getLocation();
        if (!source_manager->isInMainFile(loc)) {
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

    clang::QualType type = var_decl->getType();

    if (type->isDependentType() || type->isUndeducedType()) {
        std::cout << "AutoTypeVisitor: Skipping dependent/undeduced type for '"
                  << target_variable << "'\n";
        return true;
    }

    std::string type_str;
    if (context) {
        clang::PrintingPolicy policy = context->getPrintingPolicy();
        policy.SuppressUnwrittenScope = true;
        policy.SuppressTagKeyword = true;
        policy.FullyQualifiedName = true;
        policy.PrintCanonicalTypes = false;

        type_str = type.getAsString(policy);
    } else {
        type_str = type.getAsString();
    }

    if (!type_str.empty() && type_str != "auto" && type_str.find("dependent") == std::string::npos && type_str.find("Dependent") == std::string::npos) {

        type_str = normalize_type_string(type_str);

        std::cout << "AutoTypeVisitor: Final resolved type for '"
                  << target_variable << "': " << type_str << "\n";

        deduced_type = type_str;
    }

    return true;
}

std::string AutoTypeVisitor::normalize_type_string(const std::string& type_str)
{
    std::string result = type_str;

    std::vector<std::pair<std::regex, std::string>> rules = {
        { std::regex(R"(std::shared_ptr<_NonArray<([^>]+>)>)"), "std::shared_ptr<$1" },
        { std::regex(R"(shared_ptr<_NonArray<([^>]+>)>)"), "std::shared_ptr<$1" },
        { std::regex(R"(std::shared_ptr<__1::([^>]+>)>)"), "std::shared_ptr<$1" },
        { std::regex(R"(class |struct |union |enum )"), "" },
    };

    for (const auto& [pattern, replacement] : rules) {
        result = std::regex_replace(result, pattern, replacement);
    }

    return result;
}

AutoTypeAction::AutoTypeAction(AutoTypeVisitor* visitor)
    : visitor_(visitor)
{
}

std::unique_ptr<clang::ASTConsumer> AutoTypeAction::CreateASTConsumer(
    clang::CompilerInstance& compiler, llvm::StringRef)
{
    return std::make_unique<AutoTypeConsumer>(visitor_);
}

AutoTypeConsumer::AutoTypeConsumer(AutoTypeVisitor* visitor)
    : visitor_(visitor)
{
}

void AutoTypeConsumer::HandleTranslationUnit(clang::ASTContext& context)
{
    visitor_->context = &context;
    visitor_->TraverseDecl(context.getTranslationUnitDecl());
}

}
