#include "ClangInterpreter.hpp"

#include <clang/Frontend/CompilerInstance.h>
#include <clang/Interpreter/Interpreter.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/TargetParser/Host.h>

#include <filesystem>
#include <iostream>

namespace Lila {

struct ClangInterpreter::Impl {
    std::unique_ptr<clang::Interpreter> interpreter;

    std::vector<std::string> include_paths;
    std::vector<std::string> compile_flags;
    std::string target_triple;

    std::unordered_map<std::string, void*> symbol_table;
    int eval_counter = 0;
};

ClangInterpreter::ClangInterpreter()
    : m_impl(std::make_unique<Impl>())
{
    m_impl->target_triple = llvm::sys::getDefaultTargetTriple();
}

ClangInterpreter::~ClangInterpreter()
{
    shutdown();
}

bool ClangInterpreter::initialize()
{
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();

    std::vector<const char*> args;
    args.push_back("-std=c++23");
    args.push_back("-DMAYASIMPLE");

    // CRITICAL: Add resource directory for builtin headers
    // things were failing because of this init order of not having IncrementalCompilerBuilder
    // before Interpreter
    // This is where stddef.h, stdint.h, etc. live
    std::string resource_dir = "-resource-dir=/usr/lib/clang/20";
    m_impl->compile_flags.push_back(resource_dir);
    args.push_back(m_impl->compile_flags.back().c_str());

    for (const auto& path : m_impl->include_paths) {
        m_impl->compile_flags.push_back("-I" + path);
        args.push_back(m_impl->compile_flags.back().c_str());
    }

    clang::IncrementalCompilerBuilder ICB;
    ICB.SetCompilerArgs(args);

    auto CI = ICB.CreateCpp();
    if (!CI) {
        m_last_error = "Failed to create CompilerInstance: " + llvm::toString(CI.takeError());
        return false;
    }

    auto interp = clang::Interpreter::create(std::move(*CI));
    if (!interp) {
        m_last_error = "Failed to create interpreter: " + llvm::toString(interp.takeError());
        return false;
    }

    m_impl->interpreter = std::move(*interp);

    std::cout << "Clang Interpreter initialized successfully\n";

    auto result = m_impl->interpreter->ParseAndExecute(
        "#include \"Lila/pch.h\"\n"
        "#include \"MayaFlux/MayaFlux.hpp\"\n");
    if (result) {
        std::cout << "Warning: Failed to load MayaFlux headers: "
                  << llvm::toString(std::move(result)) << "\n";
    } else {
        std::cout << "MayaFlux headers loaded successfully\n";
    }

    return true;
}

ClangInterpreter::EvalResult ClangInterpreter::eval(const std::string& code)
{
    EvalResult result;

    if (!m_impl->interpreter) {
        result.error = "Interpreter not initialized";
        return result;
    }

    auto eval_result = m_impl->interpreter->ParseAndExecute(code);

    if (!eval_result) {
        result.success = true;
    } else {
        result.success = false;
        result.error = "Execution failed: " + llvm::toString(std::move(eval_result));
    }

    m_impl->eval_counter++;
    return result;
}

ClangInterpreter::EvalResult ClangInterpreter::eval_file(const std::string& filepath)
{
    EvalResult result;

    if (!std::filesystem::exists(filepath)) {
        result.error = "File does not exist: " + filepath;
        return result;
    }

    std::string code = "#include \"" + filepath + "\"\n";
    return eval(code);
}

void* ClangInterpreter::get_symbol_address(const std::string& name)
{
    if (!m_impl->interpreter)
        return nullptr;

    auto symbol = m_impl->interpreter->getSymbolAddress(name);
    if (symbol) {
        return reinterpret_cast<void*>(symbol->getValue());
    }

    return nullptr;
}

std::vector<std::string> ClangInterpreter::get_defined_symbols()
{
    std::vector<std::string> symbols;
    for (const auto& pair : m_impl->symbol_table) {
        symbols.push_back(pair.first);
    }
    return symbols;
}

void ClangInterpreter::add_include_path(const std::string& path)
{
    if (std::filesystem::exists(path)) {
        m_impl->include_paths.push_back(path);
    }
}

void ClangInterpreter::add_library_path(const std::string& path)
{
    // Note: Library loading would need LoadDynamicLibrary() on the interpreter
}

void ClangInterpreter::add_compile_flag(const std::string& flag)
{
    m_impl->compile_flags.push_back(flag);
}

void ClangInterpreter::set_target_triple(const std::string& triple)
{
    m_impl->target_triple = triple;
}

void ClangInterpreter::reset()
{
    shutdown();
    m_impl = std::make_unique<Impl>();
}

void ClangInterpreter::shutdown()
{
    if (m_impl->interpreter) {
        m_impl->interpreter.reset();
    }
}

bool ClangInterpreter::load_pch(const std::string& pch_path)
{
    // PCH would need to be added to compile flags before initialization
    if (!std::filesystem::exists(pch_path)) {
        m_last_error = "PCH file does not exist: " + pch_path;
        return false;
    }

    m_impl->compile_flags.push_back("-include-pch");
    m_impl->compile_flags.push_back(pch_path);
    return true;
}

bool ClangInterpreter::create_pch(const std::string& header_path,
    const std::string& output_path)
{
    std::string cmd = "clang++ -x c++-header -std=c++23 ";
    for (const auto& flag : m_impl->compile_flags) {
        cmd += flag + " ";
    }
    cmd += header_path + " -o " + output_path;
    return std::system(cmd.c_str()) == 0;
}

} // namespace Lila
